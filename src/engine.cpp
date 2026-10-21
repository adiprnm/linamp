#include "engine.h"

#include "util.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <condition_variable>
#include <cctype>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace {

constexpr int OUT_CHANNELS = 2;
constexpr int TARGET_RATE = 48000;
constexpr int FFT_SIZE = 2048;
constexpr int RING_SECONDS = 3;

/* ---------------- ring buffer (float interleaved) ---------------- */
class RingBuffer {
public:
    explicit RingBuffer(int frames) : m_buf(size_t(frames) * OUT_CHANNELS, 0.f), m_cap(frames) {}

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_read = m_write = 0;
        m_notFull.notify_all();
    }

    void abort()
    {
        m_aborted.store(true);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_notFull.notify_all();
        m_notEmpty.notify_all();
    }

    void wakeAll()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_notFull.notify_all();
        m_notEmpty.notify_all();
    }

    int available() const { return int(m_write - m_read); }

    int read(float *dst, int frames)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const int n = std::min(frames, available());
        for (int i = 0; i < n; ++i) {
            const int64_t idx = (m_read + i) % m_cap;
            std::memcpy(dst + i * OUT_CHANNELS, m_buf.data() + size_t(idx) * OUT_CHANNELS,
                        OUT_CHANNELS * sizeof(float));
        }
        m_read += n;
        if (n > 0)
            m_notFull.notify_all();
        return n;
    }

    void writeBlocking(const float *src, int frames)
    {
        int done = 0;
        while (done < frames && !m_aborted.load()) {
            int chunk = 0;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                while (available() >= m_cap && !m_aborted.load())
                    m_notFull.wait_for(lock, std::chrono::milliseconds(50));
                if (m_aborted.load())
                    return;
                chunk = std::min(frames - done, m_cap - available());
                for (int i = 0; i < chunk; ++i) {
                    const int64_t idx = (m_write + i) % m_cap;
                    std::memcpy(m_buf.data() + size_t(idx) * OUT_CHANNELS,
                                src + size_t(done + i) * OUT_CHANNELS,
                                OUT_CHANNELS * sizeof(float));
                }
                m_write += chunk;
                m_notEmpty.notify_all();
            }
            done += chunk;
        }
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_notFull;
    std::condition_variable m_notEmpty;
    std::vector<float> m_buf;
    int m_cap;
    int64_t m_read = 0;
    int64_t m_write = 0;
    std::atomic<bool> m_aborted{false};
};

/* ---------------- DSP settings shared with the audio thread ---------------- */
struct DspState {
    std::mutex mutex;
    double volume = 0.82;
    double balance = 0.0;
    double preampDb = 0.0;
    bool eqOn = true;
    bool eqAuto = true;
    double gains[EQ_BANDS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    uint64_t generation = 1;
};

struct DspSnapshot {
    double volume = 0.82;
    double balance = 0.0;
    double preampDb = 0.0;
    bool eqOn = true;
    bool eqAuto = true;
    double gains[EQ_BANDS] = {};
    uint64_t generation = 0;
};

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

struct Analysis {
    std::mutex mutex;
    std::vector<float> spectrum = std::vector<float>(SPECTRUM_BANDS, 0.f);
    std::vector<float> waveform = std::vector<float>(WAVEFORM_POINTS, 0.f);
};

/* ---------------- pull source: ring → DSP → FFT (audio thread) ------------ */
class PcmProcessor {
public:
    PcmProcessor(RingBuffer *ring, DspState *dsp, Analysis *analysis, std::atomic<int64_t> *framesReal,
                 int outRate)
        : m_ring(ring), m_dsp(dsp), m_analysis(analysis), m_framesReal(framesReal), m_rate(outRate)
    {
        m_fftBuf.reserve(FFT_SIZE * 2);
        m_waveformBuf.assign(1024, 0.f);
        m_smooth.assign(SPECTRUM_BANDS, 0.f);
        m_lastEmitNs = 0;
    }

    /* Fills `frames` interleaved float frames (silence padded). */
    void pull(float *out, int frames)
    {
        const int got = m_ring->read(out, frames);
        if (got < frames)
            std::memset(out + size_t(got) * OUT_CHANNELS, 0,
                        size_t(frames - got) * OUT_CHANNELS * sizeof(float));
        if (got > 0)
            m_framesReal->fetch_add(got);
        process(out, frames);
    }

private:
    void updateCoefficients(const DspSnapshot &s)
    {
        for (int i = 0; i < EQ_BANDS; ++i) {
            const double A = std::pow(10.0, s.gains[i] / 40.0);
            const double w0 = 2.0 * M_PI * EQ_FREQS[i] / double(m_rate);
            const double alpha = std::sin(w0) / (2.0 * 1.15);
            const double b0 = 1.0 + alpha * A;
            const double b1 = -2.0 * std::cos(w0);
            const double b2 = 1.0 - alpha * A;
            const double a0 = 1.0 + alpha / A;
            const double a1 = -2.0 * std::cos(w0);
            const double a2 = 1.0 - alpha / A;
            m_coeff[i] = {float(b0 / a0), float(b1 / a0), float(b2 / a0), float(a1 / a0),
                          float(a2 / a0)};
        }
        for (int b = 0; b < EQ_BANDS; ++b)
            for (int c = 0; c < 2; ++c)
                m_z1[b][c] = m_z2[b][c] = 0.f;
    }

    void process(float *data, int frames)
    {
        DspSnapshot s;
        {
            std::lock_guard<std::mutex> lock(m_dsp->mutex);
            s.volume = m_dsp->volume;
            s.balance = m_dsp->balance;
            s.preampDb = m_dsp->preampDb;
            s.eqOn = m_dsp->eqOn;
            s.eqAuto = m_dsp->eqAuto;
            s.generation = m_dsp->generation;
            for (int i = 0; i < EQ_BANDS; ++i)
                s.gains[i] = m_dsp->gains[i];
        }
        if (s.generation != m_lastGen) {
            updateCoefficients(s);
            m_lastGen = s.generation;
        }

        double pre = 1.0;
        if (s.eqOn) {
            double maxBoost = 0;
            for (int i = 0; i < EQ_BANDS; ++i)
                maxBoost = std::max(maxBoost, s.gains[i]);
            double preDb = s.preampDb;
            if (s.eqAuto)
                preDb -= maxBoost * 0.55;
            pre = std::pow(10.0, preDb / 20.0);
        }

        const double vol = std::pow(std::clamp(s.volume, 0.0, 1.0), 1.8) * 0.95;
        const double bal = std::clamp(s.balance, -1.0, 1.0);
        const double gainL = vol * (bal < 0 ? 1.0 : 1.0 - bal);
        const double gainR = vol * (bal > 0 ? 1.0 : 1.0 + bal);

        for (int i = 0; i < frames; ++i) {
            float l = data[i * 2];
            float r = data[i * 2 + 1];

            if (s.eqOn) {
                l = float(l * pre);
                r = float(r * pre);
                for (int b = 0; b < EQ_BANDS; ++b) {
                    const Biquad &c = m_coeff[b];
                    float yl = c.b0 * l + m_z1[b][0];
                    m_z1[b][0] = c.b1 * l - c.a1 * yl + m_z2[b][0];
                    m_z2[b][0] = c.b2 * l - c.a2 * yl;
                    l = yl;

                    float yr = c.b0 * r + m_z1[b][1];
                    m_z1[b][1] = c.b1 * r - c.a1 * yr + m_z2[b][1];
                    m_z2[b][1] = c.b2 * r - c.a2 * yr;
                    r = yr;
                }
            }

            analyzeSample((l + r) * 0.5f);

            data[i * 2] = float(std::clamp(l * float(gainL), -1.f, 1.f));
            data[i * 2 + 1] = float(std::clamp(r * float(gainR), -1.f, 1.f));
        }

        maybeEmitAnalysis();
    }

    void analyzeSample(float v)
    {
        m_fftBuf.push_back(v);
        if (m_fftBuf.size() > size_t(FFT_SIZE))
            m_fftBuf.erase(m_fftBuf.begin(), m_fftBuf.begin() + (m_fftBuf.size() - FFT_SIZE));

        m_waveformBuf[size_t(m_wavePos)] = v;
        m_wavePos = (m_wavePos + 1) % int(m_waveformBuf.size());
    }

    void maybeEmitAnalysis()
    {
        if (m_fftBuf.size() < size_t(FFT_SIZE))
            return;
        const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
        if (m_lastEmitNs != 0 && now - m_lastEmitNs < 33)
            return;
        m_lastEmitNs = now;

        std::vector<std::complex<float>> spec(static_cast<std::size_t>(FFT_SIZE), {0.f, 0.f});
        for (int i = 0; i < FFT_SIZE; ++i) {
            const float w = 0.5f * (1.f - std::cos(2.f * float(M_PI) * i / (FFT_SIZE - 1)));
            spec[size_t(i)] = std::complex<float>(m_fftBuf[size_t(i)] * w, 0.f);
        }
        fft(spec);

        const double df = double(m_rate) / FFT_SIZE;
        std::vector<float> out;
        out.reserve(SPECTRUM_BANDS);
        for (int b = 0; b < SPECTRUM_BANDS; ++b) {
            const double f0 = 32.0 * std::pow(16500.0 / 32.0, double(b) / SPECTRUM_BANDS);
            const double f1 = 32.0 * std::pow(16500.0 / 32.0, double(b + 1) / SPECTRUM_BANDS);
            int k0 = std::max(1, int(f0 / df));
            int k1 = std::min(FFT_SIZE / 2 - 1, std::max(k0 + 1, int(f1 / df)));
            float mx = 0.f;
            for (int k = k0; k <= k1; ++k)
                mx = std::max(mx, std::abs(spec[size_t(k)]));
            const double db = 20.0 * std::log10(double(mx) / (FFT_SIZE / 2.0) + 1e-9);
            double norm = (db + 74.0) / 64.0;
            norm *= 1.0 + (double(b) / SPECTRUM_BANDS) * 0.25;
            norm = std::clamp(norm, 0.0, 1.0);
            const float smoothed = std::max(float(norm), m_smooth[size_t(b)] * 0.82f);
            m_smooth[size_t(b)] = smoothed;
            out.push_back(smoothed);
        }

        std::vector<float> wave(128, 0.f);
        for (int i = 0; i < 128; ++i) {
            const int idx = (m_wavePos + int(m_waveformBuf.size()) - 1024 + i * 8) %
                            int(m_waveformBuf.size());
            wave[size_t(i)] = m_waveformBuf[size_t(idx)];
        }

        {
            std::lock_guard<std::mutex> lock(m_analysis->mutex);
            m_analysis->spectrum = out;
            m_analysis->waveform = wave;
        }

        if (m_fftBuf.size() >= size_t(FFT_SIZE))
            m_fftBuf.erase(m_fftBuf.begin(), m_fftBuf.begin() + FFT_SIZE / 2);
    }

    static void fft(std::vector<std::complex<float>> &a)
    {
        const int n = int(a.size());
        for (int i = 1, j = 0; i < n; ++i) {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap(a[size_t(i)], a[size_t(j)]);
        }
        for (int len = 2; len <= n; len <<= 1) {
            const double ang = -2.0 * M_PI / len;
            const std::complex<float> wlen(float(std::cos(ang)), float(std::sin(ang)));
            for (int i = 0; i < n; i += len) {
                std::complex<float> w(1.f, 0.f);
                for (int j = 0; j < len / 2; ++j) {
                    const auto u = a[size_t(i + j)];
                    const auto v = a[size_t(i + j + len / 2)] * w;
                    a[size_t(i + j)] = u + v;
                    a[size_t(i + j + len / 2)] = u - v;
                    w *= wlen;
                }
            }
        }
    }

    RingBuffer *m_ring;
    DspState *m_dsp;
    Analysis *m_analysis;
    std::atomic<int64_t> *m_framesReal;
    int m_rate;
    uint64_t m_lastGen = 0;
    Biquad m_coeff[EQ_BANDS];
    float m_z1[EQ_BANDS][2] = {};
    float m_z2[EQ_BANDS][2] = {};
    std::vector<float> m_fftBuf;
    std::vector<float> m_waveformBuf;
    int m_wavePos = 0;
    std::vector<float> m_smooth;
    int64_t m_lastEmitNs = 0;
};

/* ---------------- decoder thread ---------------- */
class DecoderThread {
public:
    DecoderThread(AVFormatContext *fmt, AVCodecContext *codec, SwrContext *swr, int streamIndex,
                  RingBuffer *ring, int outRate)
        : m_fmt(fmt), m_codec(codec), m_swr(swr), m_streamIndex(streamIndex), m_ring(ring),
          m_outRate(outRate)
    {
    }

    ~DecoderThread() { stop(); }

    void start() { m_thread = std::thread([this] { run(); }); }

    void stop()
    {
        m_abort.store(true);
        if (m_ring)
            m_ring->abort();
        if (m_thread.joinable())
            m_thread.join();
    }

    void requestSeek(int64_t ms)
    {
        m_seekTarget.store(ms);
        m_seekRequested.store(true);
        if (m_ring)
            m_ring->wakeAll();
    }

    std::atomic<int64_t> totalFrames{0};
    std::atomic<bool> eos{false};

private:
    void run()
    {
        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        std::vector<float> pcm;

        while (!m_abort.load()) {
            if (m_seekRequested.exchange(false)) {
                const int64_t ms = m_seekTarget.load();
                m_ring->clear();
                const int ret = av_seek_frame(m_fmt, -1, ms * 1000, AVSEEK_FLAG_BACKWARD);
                if (ret < 0)
                    SDL_Log("seek failed: %d", ret);
                avcodec_flush_buffers(m_codec);
                swr_init(m_swr);
                totalFrames.store(0);
                eos.store(false);
            }

            const int ret = av_read_frame(m_fmt, packet);
            if (ret < 0) {
                avcodec_send_packet(m_codec, nullptr);
                while (avcodec_receive_frame(m_codec, frame) >= 0) {
                    writeFrame(frame, pcm);
                    av_frame_unref(frame);
                }
                int outN = swr_get_out_samples(m_swr, 0);
                if (outN > 0) {
                    pcm.assign(size_t(outN) * OUT_CHANNELS, 0.f);
                    uint8_t *dst = reinterpret_cast<uint8_t *>(pcm.data());
                    const int got = swr_convert(m_swr, &dst, outN, nullptr, 0);
                    if (got > 0) {
                        m_ring->writeBlocking(pcm.data(), got);
                        totalFrames.fetch_add(got);
                    }
                }
                eos.store(true);
                break;
            }

            if (packet->stream_index == m_streamIndex) {
                if (avcodec_send_packet(m_codec, packet) >= 0) {
                    while (avcodec_receive_frame(m_codec, frame) >= 0) {
                        writeFrame(frame, pcm);
                        av_frame_unref(frame);
                    }
                }
            }
            av_packet_unref(packet);
        }

        av_frame_free(&frame);
        av_packet_free(&packet);
    }

    void writeFrame(AVFrame *frame, std::vector<float> &pcm)
    {
        if (!frame || frame->nb_samples <= 0)
            return;
        const int outN = swr_get_out_samples(m_swr, frame->nb_samples);
        if (outN <= 0)
            return;
        pcm.assign(size_t(outN) * OUT_CHANNELS, 0.f);
        uint8_t *dst = reinterpret_cast<uint8_t *>(pcm.data());
        const int got = swr_convert(m_swr, &dst, outN,
                                    const_cast<const uint8_t **>(frame->extended_data),
                                    frame->nb_samples);
        if (got > 0) {
            m_ring->writeBlocking(pcm.data(), got);
            totalFrames.fetch_add(got);
        }
    }

    AVFormatContext *m_fmt;
    AVCodecContext *m_codec;
    SwrContext *m_swr;
    int m_streamIndex;
    RingBuffer *m_ring;
    int m_outRate;
    std::thread m_thread;
    std::atomic<bool> m_abort{false};
    std::atomic<bool> m_seekRequested{false};
    std::atomic<int64_t> m_seekTarget{0};
};

std::string fileNameWithoutExtension(const std::string &path)
{
    return util::completeBaseName(path);
}

} // namespace

const double EQ_FREQS[EQ_BANDS] = {60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000};

/* ==================================================================
   AudioEngine::Private
   ================================================================== */
class AudioEngine::Private {
public:
    AudioEngine *q = nullptr;

    AVFormatContext *fmt = nullptr;
    AVCodecContext *codec = nullptr;
    SwrContext *swr = nullptr;
    int streamIndex = -1;
    AVStream *stream = nullptr;

    RingBuffer *ring = nullptr;
    PcmProcessor *processor = nullptr;
    SDL_AudioStream *audioStream = nullptr;
    SDL_AudioDeviceID deviceId = 0;
    DecoderThread *decoder = nullptr;
    Analysis analysis;

    DspState dsp;
    std::atomic<int64_t> framesReal{0};
    std::atomic<int64_t> seekBaseMs{0};
    std::atomic<bool> finishedFlag{false};
    std::atomic<bool> metadataFlag{false};
    int64_t finishPendingAt = 0;

    std::string title, artist, source, lastError;
    int64_t durationMs = 0;
    int64_t positionMs = 0;
    int bitrateKbps = 0;
    int sampleRate = 0;
    int channels = 0;
    int outRate = TARGET_RATE;
    bool playing = false;
    bool started = false;

    void cleanup(bool keepMeta = false)
    {
        if (audioStream) {
            SDL_PauseAudioStreamDevice(audioStream);
            SDL_DestroyAudioStream(audioStream);
            audioStream = nullptr;
        }
        deviceId = 0;
        delete processor;
        processor = nullptr;
        if (decoder) {
            delete decoder;
            decoder = nullptr;
        }
        delete ring;
        ring = nullptr;

        if (swr)
            swr_free(&swr);
        swr = nullptr;
        if (codec)
            avcodec_free_context(&codec);
        codec = nullptr;
        if (fmt)
            avformat_close_input(&fmt);
        fmt = nullptr;
        stream = nullptr;
        streamIndex = -1;

        framesReal.store(0);
        seekBaseMs.store(0);
        finishedFlag.store(false);
        finishPendingAt = 0;
        started = false;
        if (!keepMeta) {
            title.clear();
            artist.clear();
            source.clear();
            durationMs = 0;
            positionMs = 0;
            bitrateKbps = 0;
            sampleRate = 0;
            channels = 0;
        }
    }

    void setPlaying(bool p)
    {
        if (playing == p)
            return;
        playing = p;
    }

    void tick()
    {
        if (!started)
            return;
        const int64_t frames = framesReal.load();
        int64_t pos = seekBaseMs.load() + frames * 1000 / outRate;
        if (durationMs > 0)
            pos = std::min(pos, durationMs);
        if (pos != positionMs)
            positionMs = pos;

        if (decoder && decoder->eos.load() && !finishedFlag.load()) {
            const int64_t total = decoder->totalFrames.load();
            const bool consumed = frames >= total - outRate / 100;
            if (consumed) {
                const int64_t now = int64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::steady_clock::now().time_since_epoch())
                                                .count());
                if (finishPendingAt == 0) {
                    finishPendingAt = now + 250;
                } else if (now >= finishPendingAt) {
                    finishedFlag.store(true);
                    playing = false;
                    if (audioStream)
                        SDL_PauseAudioStreamDevice(audioStream);
                    if (durationMs > 0)
                        positionMs = durationMs;
                }
            } else {
                finishPendingAt = 0;
            }
        }
    }

    bool openFile(const std::string &url);
};

static void SDLCALL audioCallback(void *userdata, SDL_AudioStream *stream, int additionalAmount,
                                  int totalAmount)
{
    auto *proc = static_cast<PcmProcessor *>(userdata);
    (void)totalAmount;
    if (additionalAmount <= 0)
        return;
    const int frameBytes = OUT_CHANNELS * int(sizeof(float));
    int frames = additionalAmount / frameBytes;
    if (frames <= 0)
        return;
    static thread_local std::vector<float> buf;
    buf.resize(size_t(frames) * OUT_CHANNELS);
    proc->pull(buf.data(), frames);
    SDL_PutAudioStreamData(stream, buf.data(), frames * frameBytes);
}

bool AudioEngine::Private::openFile(const std::string &url)
{
    cleanup();

    const bool fileUrl = util::isFileUrl(url);
    const bool remote = util::isRemoteUrl(url);
    const bool local = !remote;
    const std::string path = fileUrl ? util::urlToPath(url) : url;

    if (avformat_open_input(&fmt, path.c_str(), nullptr, nullptr) < 0) {
        lastError = "Tidak bisa membuka file";
        return false;
    }
    if (avformat_find_stream_info(fmt, nullptr) < 0) {
        lastError = "Tidak bisa membaca stream audio";
        cleanup();
        return false;
    }
    streamIndex = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIndex < 0) {
        lastError = "Tidak ada stream audio";
        cleanup();
        return false;
    }
    stream = fmt->streams[streamIndex];
    const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!dec) {
        lastError = "Codec tidak didukung";
        cleanup();
        return false;
    }
    codec = avcodec_alloc_context3(dec);
    avcodec_parameters_to_context(codec, stream->codecpar);
    if (avcodec_open2(codec, dec, nullptr) < 0) {
        lastError = "Gagal membuka decoder";
        cleanup();
        return false;
    }

    // metadata
    std::string tagTitle, tagArtist;
    if (AVDictionaryEntry *e = av_dict_get(fmt->metadata, "title", nullptr, 0))
        tagTitle = e->value;
    if (AVDictionaryEntry *e = av_dict_get(fmt->metadata, "artist", nullptr, 0))
        tagArtist = e->value;
    if (tagTitle.empty() && stream->metadata) {
        if (AVDictionaryEntry *e = av_dict_get(stream->metadata, "title", nullptr, 0))
            tagTitle = e->value;
    }
    if (tagArtist.empty() && stream->metadata) {
        if (AVDictionaryEntry *e = av_dict_get(stream->metadata, "artist", nullptr, 0))
            tagArtist = e->value;
    }

    std::string base;
    if (local) {
        base = fileNameWithoutExtension(path);
        std::replace(base.begin(), base.end(), '_', ' ');
        // collapse whitespace
        std::string collapsed;
        bool prevSpace = true;
        for (char c : base) {
            const bool sp = std::isspace((unsigned char)c) != 0;
            if (sp) {
                if (!prevSpace)
                    collapsed.push_back(' ');
                prevSpace = true;
            } else {
                collapsed.push_back(c);
                prevSpace = false;
            }
        }
        base = collapsed;
        // strip leading "01 - " / "01. " track numbers
        size_t i = 0;
        while (i < base.size() && std::isspace((unsigned char)base[i]))
            ++i;
        size_t j = i;
        while (j < base.size() && std::isdigit((unsigned char)base[j]))
            ++j;
        if (j > i && j - i <= 3) {
            size_t k = j;
            while (k < base.size() && std::isspace((unsigned char)base[k]))
                ++k;
            if (k < base.size() && (base[k] == '.' || base[k] == '-' || base[k] == ')')) {
                ++k;
                while (k < base.size() && (std::isspace((unsigned char)base[k]) || base[k] == '.'))
                    ++k;
                base = base.substr(k);
            }
        }
    }
    std::string fallbackArtist, fallbackTitle = base;
    const size_t sep = base.find(" - ");
    if (sep != std::string::npos && sep > 0) {
        fallbackArtist = base.substr(0, sep);
        fallbackTitle = base.substr(sep + 3);
        auto trim = [](std::string &s) {
            while (!s.empty() && std::isspace((unsigned char)s.front()))
                s.erase(s.begin());
            while (!s.empty() && std::isspace((unsigned char)s.back()))
                s.pop_back();
        };
        trim(fallbackArtist);
        trim(fallbackTitle);
    }
    title = tagTitle.empty() ? fallbackTitle : tagTitle;
    artist = tagArtist.empty() ? fallbackArtist : tagArtist;
    if (title.empty())
        title = "UNTITLED";

    // duration
    if (fmt->duration > 0)
        durationMs = fmt->duration / 1000;
    else if (stream->duration > 0)
        durationMs = av_rescale_q(stream->duration, stream->time_base, AV_TIME_BASE_Q) / 1000;
    if (durationMs < 0)
        durationMs = 0;

    bitrateKbps = int(stream->codecpar->bit_rate / 1000);
    if (bitrateKbps <= 0 && durationMs > 0 && local) {
        const uint64_t bytes = util::fileSize(path);
        if (bytes > 0)
            bitrateKbps = int(double(bytes) * 8 / (double(durationMs) / 1000.0) / 1000.0);
    }
    sampleRate = codec->sample_rate;
    channels = codec->ch_layout.nb_channels;

    // resampler -> float stereo 48k
    AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
    if (swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_FLT, outRate, &codec->ch_layout,
                            codec->sample_fmt, codec->sample_rate, 0, nullptr) < 0 ||
        !swr) {
        lastError = "Gagal menyiapkan resampler";
        cleanup();
        return false;
    }
    if (swr_init(swr) < 0) {
        lastError = "Gagal inisialisasi resampler";
        cleanup();
        return false;
    }

    ring = new RingBuffer(outRate * RING_SECONDS);
    processor = new PcmProcessor(ring, &dsp, &analysis, &framesReal, outRate);

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = OUT_CHANNELS;
    spec.freq = outRate;
    audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                           audioCallback, processor);
    if (!audioStream) {
        lastError = std::string("Tidak ada output audio: ") + SDL_GetError();
        cleanup();
        return false;
    }
    deviceId = SDL_GetAudioStreamDevice(audioStream);

    positionMs = 0;
    source = url;
    seekBaseMs.store(0);
    framesReal.store(0);
    finishedFlag.store(false);
    started = true;
    metadataFlag.store(true);

    decoder = new DecoderThread(fmt, codec, swr, streamIndex, ring, outRate);
    decoder->start();
    return true;
}

/* ==================================================================
   AudioEngine
   ================================================================== */
AudioEngine::AudioEngine() : d(new Private)
{
    d->q = this;
}

AudioEngine::~AudioEngine()
{
    d->cleanup();
    delete d;
}

const std::string &AudioEngine::title() const { return d->title; }
const std::string &AudioEngine::artist() const { return d->artist; }
const std::string &AudioEngine::source() const { return d->source; }
const std::string &AudioEngine::lastError() const { return d->lastError; }
int64_t AudioEngine::duration() const { return d->durationMs; }
int64_t AudioEngine::position() const { return d->positionMs; }
bool AudioEngine::playing() const { return d->playing; }
int AudioEngine::bitrate() const { return d->bitrateKbps; }
int AudioEngine::sampleRate() const { return d->sampleRate; }
int AudioEngine::channels() const { return d->channels; }
bool AudioEngine::hasSource() const { return d->audioStream != nullptr; }

double AudioEngine::volume() const
{
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    return d->dsp.volume;
}
double AudioEngine::balance() const
{
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    return d->dsp.balance;
}
bool AudioEngine::eqOn() const
{
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    return d->dsp.eqOn;
}
bool AudioEngine::eqAuto() const
{
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    return d->dsp.eqAuto;
}
double AudioEngine::preamp() const
{
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    return d->dsp.preampDb;
}

std::array<double, EQ_BANDS> AudioEngine::bands() const
{
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    std::array<double, EQ_BANDS> out{};
    for (int i = 0; i < EQ_BANDS; ++i)
        out[size_t(i)] = d->dsp.gains[i];
    return out;
}

std::vector<float> AudioEngine::spectrum() const
{
    std::lock_guard<std::mutex> lock(d->analysis.mutex);
    return d->analysis.spectrum;
}

std::vector<float> AudioEngine::waveform() const
{
    std::lock_guard<std::mutex> lock(d->analysis.mutex);
    return d->analysis.waveform;
}

void AudioEngine::open(const std::string &url)
{
    if (url.empty())
        return;
    d->started = false;
    d->setPlaying(false);
    d->openFile(url);
}

void AudioEngine::play()
{
    if (!d->audioStream && d->source.empty())
        return;
    if (d->finishedFlag.load() && !d->source.empty()) {
        const std::string src = d->source;
        open(src);
    }
    if (!d->started || !d->audioStream)
        return;
    SDL_ResumeAudioStreamDevice(d->audioStream);
    d->finishPendingAt = 0;
    d->setPlaying(true);
}

void AudioEngine::pause()
{
    if (!d->audioStream)
        return;
    SDL_PauseAudioStreamDevice(d->audioStream);
    d->setPlaying(false);
}

void AudioEngine::togglePlay()
{
    if (d->playing)
        pause();
    else
        play();
}

void AudioEngine::stop()
{
    if (!d->audioStream)
        return;
    SDL_PauseAudioStreamDevice(d->audioStream);
    d->setPlaying(false);
    seek(0);
}

void AudioEngine::seek(int64_t ms)
{
    if (!d->started || !d->decoder)
        return;
    if (d->durationMs > 0)
        ms = std::clamp<int64_t>(ms, 0, d->durationMs);
    else
        ms = std::max<int64_t>(0, ms);

    const bool wasPlaying = d->playing;
    d->seekBaseMs.store(ms);
    d->framesReal.store(0);
    d->positionMs = ms;
    d->finishedFlag.store(false);
    d->finishPendingAt = 0;
    if (d->ring)
        d->ring->clear();
    if (d->decoder) {
        d->decoder->eos.store(false);
        d->decoder->totalFrames.store(0);
        d->decoder->requestSeek(ms);
    }
    if (d->audioStream) {
        if (wasPlaying)
            SDL_ResumeAudioStreamDevice(d->audioStream);
        else
            SDL_PauseAudioStreamDevice(d->audioStream);
    }
}

void AudioEngine::setVolume(double v)
{
    v = std::clamp(v, 0.0, 1.0);
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    d->dsp.volume = v;
}

void AudioEngine::setBalance(double v)
{
    v = std::clamp(v, -1.0, 1.0);
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    d->dsp.balance = v;
}

void AudioEngine::setEqOn(bool on)
{
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    if (d->dsp.eqOn == on)
        return;
    d->dsp.eqOn = on;
    ++d->dsp.generation;
}

void AudioEngine::setEqAuto(bool on)
{
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    if (d->dsp.eqAuto == on)
        return;
    d->dsp.eqAuto = on;
    ++d->dsp.generation;
}

void AudioEngine::setPreamp(double db)
{
    db = std::clamp(db, -12.0, 12.0);
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    if (std::fabs(d->dsp.preampDb - db) < 1e-6)
        return;
    d->dsp.preampDb = db;
    ++d->dsp.generation;
}

void AudioEngine::setBand(int index, double db)
{
    if (index < 0 || index >= EQ_BANDS)
        return;
    db = std::clamp(db, -12.0, 12.0);
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    if (std::fabs(d->dsp.gains[index] - db) < 1e-6)
        return;
    d->dsp.gains[index] = db;
    ++d->dsp.generation;
}

void AudioEngine::resetEq()
{
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    for (int i = 0; i < EQ_BANDS; ++i)
        d->dsp.gains[i] = 0;
    d->dsp.preampDb = 0;
    ++d->dsp.generation;
}

void AudioEngine::shutdown()
{
    d->cleanup();
}

void AudioEngine::tick()
{
    d->tick();
}

bool AudioEngine::takeFinishedFlag()
{
    return d->finishedFlag.exchange(false);
}

bool AudioEngine::takeMetadataFlag()
{
    return d->metadataFlag.exchange(false);
}

void AudioEngine::loadSettings(double volume, double balance, bool eqOn, bool eqAuto, double preamp,
                               const std::array<double, EQ_BANDS> &bands)
{
    std::lock_guard<std::mutex> lock(d->dsp.mutex);
    d->dsp.volume = std::clamp(volume, 0.0, 1.0);
    d->dsp.balance = std::clamp(balance, -1.0, 1.0);
    d->dsp.eqOn = eqOn;
    d->dsp.eqAuto = eqAuto;
    d->dsp.preampDb = std::clamp(preamp, -12.0, 12.0);
    for (int i = 0; i < EQ_BANDS; ++i)
        d->dsp.gains[i] = std::clamp(bands[size_t(i)], -12.0, 12.0);
    ++d->dsp.generation;
}

void AudioEngine::saveSettings()
{
    // the app persists these through settings.cpp
}
