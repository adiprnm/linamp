#include "audioengine.h"

#include <QAudioSink>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QIODevice>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSettings>
#include <QRegularExpression>
#include <QUrl>
#include <QDateTime>
#include <QDebug>

#include <atomic>
#include <cmath>
#include <complex>
#include <cstring>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

namespace {

constexpr int OUT_CHANNELS = 2;
constexpr int TARGET_RATE = 48000;
constexpr int EQ_BANDS = 10;
constexpr double EQ_FREQS[EQ_BANDS] = {60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000};
constexpr int FFT_SIZE = 2048;
constexpr int SPECTRUM_BANDS = 31;
constexpr int RING_SECONDS = 3;

/* ---------------- ring buffer (float interleaved) ---------------- */
class RingBuffer {
public:
    explicit RingBuffer(int frames) : m_buf(frames * OUT_CHANNELS, 0.f), m_cap(frames) {}

    int available() const { return int(m_write - m_read); }
    int capacity() const { return m_cap; }

    void clear()
    {
        QMutexLocker lock(&m_mutex);
        m_read = m_write = 0;
        m_notFull.wakeAll();
    }

    void abort()
    {
        m_aborted.store(true);
        QMutexLocker lock(&m_mutex);
        m_notFull.wakeAll();
        m_notEmpty.wakeAll();
    }

    void wakeAll()
    {
        QMutexLocker lock(&m_mutex);
        m_notFull.wakeAll();
        m_notEmpty.wakeAll();
    }

    int read(float *dst, int frames)
    {
        QMutexLocker lock(&m_mutex);
        const int n = qMin(frames, available());
        for (int i = 0; i < n; ++i) {
            const qint64 idx = (m_read + i) % m_cap;
            std::memcpy(dst + i * OUT_CHANNELS, m_buf.constData() + idx * OUT_CHANNELS,
                        OUT_CHANNELS * sizeof(float));
        }
        m_read += n;
        if (n > 0)
            m_notFull.wakeAll();
        return n;
    }

    void writeBlocking(const float *src, int frames)
    {
        int done = 0;
        while (done < frames && !m_aborted.load()) {
            int chunk = 0;
            {
                QMutexLocker lock(&m_mutex);
                while (available() >= m_cap && !m_aborted.load())
                    m_notFull.wait(&m_mutex, 50);
                if (m_aborted.load())
                    return;
                chunk = qMin(frames - done, m_cap - available());
                for (int i = 0; i < chunk; ++i) {
                    const qint64 idx = (m_write + i) % m_cap;
                    std::memcpy(m_buf.data() + idx * OUT_CHANNELS,
                                src + (done + i) * OUT_CHANNELS,
                                OUT_CHANNELS * sizeof(float));
                }
                m_write += chunk;
                m_notEmpty.wakeAll();
            }
            done += chunk;
        }
    }

private:
    QMutex m_mutex;
    QWaitCondition m_notFull;
    QWaitCondition m_notEmpty;
    QVector<float> m_buf;
    int m_cap;
    qint64 m_read = 0;
    qint64 m_write = 0;
    std::atomic<bool> m_aborted{false};
};

/* ---------------- DSP settings shared with the audio thread ---------------- */
struct DspState {
    QMutex mutex;
    double volume = 0.82;
    double balance = 0.0;
    double preampDb = 0.0;
    bool eqOn = true;
    bool eqAuto = true;
    double gains[EQ_BANDS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    quint64 generation = 1;
};

struct DspSnapshot {
    double volume = 0.82;
    double balance = 0.0;
    double preampDb = 0.0;
    bool eqOn = true;
    bool eqAuto = true;
    double gains[EQ_BANDS] = {};
    quint64 generation = 0;
};

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

/* ---------------- audio device (pull mode) ---------------- */
class PcmDevice : public QIODevice {
    Q_OBJECT
public:
    PcmDevice(RingBuffer *ring, DspState *dsp, std::atomic<qint64> *framesReal,
              int outRate, QObject *parent = nullptr)
        : QIODevice(parent), m_ring(ring), m_dsp(dsp), m_framesReal(framesReal), m_rate(outRate)
    {
        m_fftBuf.reserve(FFT_SIZE * 2);
        m_waveformBuf.resize(1024, 0.f);
        m_smooth.resize(SPECTRUM_BANDS, 0.f);
        m_waveOut.resize(128, 0.f);
    }

    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override
    {
        return qint64(m_ring->available()) * OUT_CHANNELS * qint64(sizeof(float));
    }
    qint64 bytesToWrite() const override { return 0; }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 frameBytes = OUT_CHANNELS * qint64(sizeof(float));
        int frames = int(maxSize / frameBytes);
        if (frames <= 0)
            return 0;

        float *out = reinterpret_cast<float *>(data);
        const int got = m_ring->read(out, frames);
        if (got < frames)
            std::memset(out + got * OUT_CHANNELS, 0,
                        size_t(frames - got) * OUT_CHANNELS * sizeof(float));
        if (got > 0)
            m_framesReal->fetch_add(got);

        process(out, frames);

        // only real frames are reported as played
        return qint64(frames) * frameBytes;
    }

    qint64 writeData(const char *, qint64) override { return 0; }

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
            m_coeff[i] = { float(b0 / a0), float(b1 / a0), float(b2 / a0),
                           float(a1 / a0), float(a2 / a0) };
        }
        for (int b = 0; b < EQ_BANDS; ++b)
            for (int c = 0; c < 2; ++c)
                m_z1[b][c] = m_z2[b][c] = 0.f;
    }

    void process(float *data, int frames)
    {
        DspSnapshot s;
        {
            QMutexLocker lock(&m_dsp->mutex);
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
                maxBoost = qMax(maxBoost, s.gains[i]);
            double preDb = s.preampDb;
            if (s.eqAuto)
                preDb -= maxBoost * 0.55;
            pre = std::pow(10.0, preDb / 20.0);
        }

        const double vol = std::pow(qBound(0.0, s.volume, 1.0), 1.8) * 0.95;
        const double bal = qBound(-1.0, s.balance, 1.0);
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

            data[i * 2] = float(qBound(-1.0f, l * float(gainL), 1.0f));
            data[i * 2 + 1] = float(qBound(-1.0f, r * float(gainR), 1.0f));
        }

        maybeEmitAnalysis(frames);
    }

    void analyzeSample(float v)
    {
        m_fftBuf.push_back(v);
        if (m_fftBuf.size() > FFT_SIZE)
            m_fftBuf.erase(m_fftBuf.begin(), m_fftBuf.begin() + (m_fftBuf.size() - FFT_SIZE));

        m_waveformBuf[m_wavePos] = v;
        m_wavePos = (m_wavePos + 1) % int(m_waveformBuf.size());
    }

    void maybeEmitAnalysis(int)
    {
        if (m_fftBuf.size() < FFT_SIZE)
            return;
        if (m_emitClock.isValid() && m_emitClock.elapsed() < 33)
            return;
        m_emitClock.restart();

        std::vector<std::complex<float>> spec(FFT_SIZE);
        for (int i = 0; i < FFT_SIZE; ++i) {
            const float w = 0.5f * (1.f - std::cos(2.f * float(M_PI) * i / (FFT_SIZE - 1)));
            spec[size_t(i)] = std::complex<float>(m_fftBuf[size_t(i)] * w, 0.f);
        }
        fft(spec);

        const double df = double(m_rate) / FFT_SIZE;
        QList<float> out;
        out.reserve(SPECTRUM_BANDS);
        for (int b = 0; b < SPECTRUM_BANDS; ++b) {
            const double f0 = 32.0 * std::pow(16500.0 / 32.0, double(b) / SPECTRUM_BANDS);
            const double f1 = 32.0 * std::pow(16500.0 / 32.0, double(b + 1) / SPECTRUM_BANDS);
            int k0 = qMax(1, int(f0 / df));
            int k1 = qMin(FFT_SIZE / 2 - 1, qMax(k0 + 1, int(f1 / df)));
            float mx = 0.f;
            for (int k = k0; k <= k1; ++k)
                mx = qMax(mx, std::abs(spec[size_t(k)]));
            const double db = 20.0 * std::log10(double(mx) / (FFT_SIZE / 2.0) + 1e-9);
            double norm = (db + 74.0) / 64.0;
            norm *= 1.0 + (double(b) / SPECTRUM_BANDS) * 0.25;
            norm = qBound(0.0, norm, 1.0);
            const float smoothed = qMax(float(norm), m_smooth[b] * 0.82f);
            m_smooth[b] = smoothed;
            out.append(smoothed);
        }

        QList<float> wave;
        wave.reserve(128);
        for (int i = 0; i < 128; ++i) {
            const int idx = (m_wavePos + int(m_waveformBuf.size()) - 1024 + i * 8) % int(m_waveformBuf.size());
            wave.append(m_waveformBuf[size_t(idx)]);
        }
        emit analysisReady(out, wave);

        // overlap: keep the last half window for the next FFT
        if (m_fftBuf.size() >= FFT_SIZE)
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

signals:
    void analysisReady(QList<float> spectrum, QList<float> waveform);

private:
    RingBuffer *m_ring;
    DspState *m_dsp;
    std::atomic<qint64> *m_framesReal;
    int m_rate;
    quint64 m_lastGen = 0;
    Biquad m_coeff[EQ_BANDS];
    float m_z1[EQ_BANDS][2] = {};
    float m_z2[EQ_BANDS][2] = {};
    std::vector<float> m_fftBuf;
    QVector<float> m_waveformBuf;
    int m_wavePos = 0;
    QVector<float> m_smooth;
    QVector<float> m_waveOut;
    QElapsedTimer m_emitClock;
};

/* ---------------- decoder thread ---------------- */
class DecoderThread : public QThread {
public:
    DecoderThread(AVFormatContext *fmt, AVCodecContext *codec, SwrContext *swr,
                  int streamIndex, RingBuffer *ring, int outRate, QObject *parent = nullptr)
        : QThread(parent), m_fmt(fmt), m_codec(codec), m_swr(swr),
          m_streamIndex(streamIndex), m_ring(ring), m_outRate(outRate) {}

    ~DecoderThread() override
    {
        abort();
        wait();
    }

    void abort() { m_abort.store(true); if (m_ring) m_ring->abort(); }
    void requestSeek(qint64 ms) { m_seekTarget.store(ms); m_seekRequested.store(true); m_ring->wakeAll(); }
    std::atomic<qint64> totalFrames{0};
    std::atomic<bool> eos{false};

protected:
    void run() override
    {
        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        QVector<float> pcm;

        while (!m_abort.load()) {
            if (m_seekRequested.exchange(false)) {
                const qint64 ms = m_seekTarget.load();
                m_ring->clear();
                const int ret = av_seek_frame(m_fmt, -1, ms * 1000, AVSEEK_FLAG_BACKWARD);
                if (ret < 0)
                    qWarning() << "seek failed" << ret;
                avcodec_flush_buffers(m_codec);
                swr_init(m_swr);
                totalFrames.store(0);
                eos.store(false);
            }

            const int ret = av_read_frame(m_fmt, packet);
            if (ret < 0) {
                // drain decoder
                avcodec_send_packet(m_codec, nullptr);
                while (avcodec_receive_frame(m_codec, frame) >= 0) {
                    writeFrame(frame, pcm);
                    av_frame_unref(frame);
                }
                // flush resampler
                int outN = swr_get_out_samples(m_swr, 0);
                if (outN > 0) {
                    pcm.resize(outN * OUT_CHANNELS);
                    uint8_t *dst = reinterpret_cast<uint8_t *>(pcm.data());
                    const int got = swr_convert(m_swr, &dst, outN, nullptr, 0);
                    if (got > 0) {
                        m_ring->writeBlocking(pcm.constData(), got);
                        totalFrames.fetch_add(got);
                    }
                }
                eos.store(true);
                break;
            }

            if (packet->stream_index == m_streamIndex) {
                int sendRet = avcodec_send_packet(m_codec, packet);
                if (sendRet >= 0) {
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

private:
    void writeFrame(AVFrame *frame, QVector<float> &pcm)
    {
        if (!frame || frame->nb_samples <= 0)
            return;
        const int outN = swr_get_out_samples(m_swr, frame->nb_samples);
        if (outN <= 0)
            return;
        pcm.resize(outN * OUT_CHANNELS);
        uint8_t *dst = reinterpret_cast<uint8_t *>(pcm.data());
        const int got = swr_convert(m_swr, &dst, outN,
                                    const_cast<const uint8_t **>(frame->extended_data),
                                    frame->nb_samples);
        if (got > 0) {
            m_ring->writeBlocking(pcm.constData(), got);
            totalFrames.fetch_add(got);
        }
    }

    AVFormatContext *m_fmt;
    AVCodecContext *m_codec;
    SwrContext *m_swr;
    int m_streamIndex;
    RingBuffer *m_ring;
    int m_outRate;
    std::atomic<bool> m_abort{false};
    std::atomic<bool> m_seekRequested{false};
    std::atomic<qint64> m_seekTarget{0};
};

} // namespace

Q_DECLARE_METATYPE(QList<float>)

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
    PcmDevice *device = nullptr;
    QAudioSink *sink = nullptr;
    DecoderThread *decoder = nullptr;

    DspState dsp;
    std::atomic<qint64> framesReal{0};
    std::atomic<qint64> seekBaseMs{0};
    std::atomic<bool> finishedFlag{false};
    qint64 finishPendingAt = 0;

    QString title, artist, source, lastError;
    qint64 durationMs = 0;
    qint64 positionMs = 0;
    int bitrateKbps = 0;
    int sampleRate = 0;
    int channels = 0;
    int outRate = TARGET_RATE;
    bool playing = false;
    bool started = false;

    QVariantList spectrum;
    QVariantList waveform;

    QTimer *ticker = nullptr;
    QTimer *saveTimer = nullptr;

    void cleanup(bool keepMeta = false)
    {
        if (sink) {
            sink->stop();
            delete sink;
            sink = nullptr;
        }
        if (device) {
            device->close();
            delete device;
            device = nullptr;
        }
        if (decoder) {
            decoder->abort();
            decoder->wait();
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
        emit q->playingChanged();
    }

    void scheduleSave()
    {
        if (saveTimer)
            saveTimer->start();
    }

    void saveNow()
    {
        QSettings s;
        s.beginGroup("audio");
        s.setValue("volume", dsp.volume);
        s.setValue("balance", dsp.balance);
        s.setValue("eqOn", dsp.eqOn);
        s.setValue("eqAuto", dsp.eqAuto);
        s.setValue("preamp", dsp.preampDb);
        QVariantList bands;
        {
            QMutexLocker lock(&dsp.mutex);
            for (int i = 0; i < EQ_BANDS; ++i)
                bands.append(dsp.gains[i]);
        }
        s.setValue("bands", bands);
        s.endGroup();
        s.sync();
    }

    void loadSettings()
    {
        QSettings s;
        s.beginGroup("audio");
        dsp.volume = qBound(0.0, s.value("volume", 0.82).toDouble(), 1.0);
        dsp.balance = qBound(-1.0, s.value("balance", 0.0).toDouble(), 1.0);
        dsp.eqOn = s.value("eqOn", true).toBool();
        dsp.eqAuto = s.value("eqAuto", true).toBool();
        dsp.preampDb = qBound(-12.0, s.value("preamp", 0.0).toDouble(), 12.0);
        const QVariantList bands = s.value("bands").toList();
        for (int i = 0; i < EQ_BANDS && i < bands.size(); ++i)
            dsp.gains[i] = qBound(-12.0, bands[i].toDouble(), 12.0);
        s.endGroup();
    }

    void tick()
    {
        if (!started)
            return;
        const qint64 frames = framesReal.load();
        qint64 pos = seekBaseMs.load() + frames * 1000 / outRate;
        if (durationMs > 0)
            pos = qMin(pos, durationMs);
        if (pos != positionMs) {
            positionMs = pos;
            emit q->positionChanged();
        }

        if (decoder && decoder->eos.load() && !finishedFlag.load()) {
            const qint64 total = decoder->totalFrames.load();
            const bool consumed = frames >= total - outRate / 100; // ~10ms toleransi
            if (consumed) {
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                if (finishPendingAt == 0)
                    finishPendingAt = now + 250;
                else if (now >= finishPendingAt) {
                    finishedFlag.store(true);
                    playing = false;
                    if (sink)
                        sink->stop();
                    if (durationMs > 0)
                        positionMs = durationMs;
                    emit q->positionChanged();
                    emit q->playingChanged();
                    emit q->finished();
                }
            } else {
                finishPendingAt = 0;
            }
        }
    }

    bool openFile(const QString &url)
    {
        cleanup();

        const bool fileUrl = url.startsWith("file:", Qt::CaseInsensitive);
        const bool remote = url.contains("://") && !fileUrl;
        const bool local = !remote;
        const QString path = fileUrl ? QUrl(url).toLocalFile() : url;
        const QByteArray pathBytes = path.toUtf8();

        if (avformat_open_input(&fmt, pathBytes.constData(), nullptr, nullptr) < 0) {
            lastError = QStringLiteral("Tidak bisa membuka file");
            emit q->errorOccurred(lastError);
            return false;
        }
        if (avformat_find_stream_info(fmt, nullptr) < 0) {
            lastError = QStringLiteral("Tidak bisa membaca stream audio");
            emit q->errorOccurred(lastError);
            cleanup();
            return false;
        }
        streamIndex = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (streamIndex < 0) {
            lastError = QStringLiteral("Tidak ada stream audio");
            emit q->errorOccurred(lastError);
            cleanup();
            return false;
        }
        stream = fmt->streams[streamIndex];
        const AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!dec) {
            lastError = QStringLiteral("Codec tidak didukung");
            emit q->errorOccurred(lastError);
            cleanup();
            return false;
        }
        codec = avcodec_alloc_context3(dec);
        avcodec_parameters_to_context(codec, stream->codecpar);
        if (avcodec_open2(codec, dec, nullptr) < 0) {
            lastError = QStringLiteral("Gagal membuka decoder");
            emit q->errorOccurred(lastError);
            cleanup();
            return false;
        }

        // metadata
        QString tagTitle, tagArtist;
        AVDictionaryEntry *e = nullptr;
        e = av_dict_get(fmt->metadata, "title", nullptr, 0);
        if (e) tagTitle = QString::fromUtf8(e->value);
        e = av_dict_get(fmt->metadata, "artist", nullptr, 0);
        if (e) tagArtist = QString::fromUtf8(e->value);
        if (tagTitle.isEmpty() && stream->metadata) {
            e = av_dict_get(stream->metadata, "title", nullptr, 0);
            if (e) tagTitle = QString::fromUtf8(e->value);
        }
        if (tagArtist.isEmpty() && stream->metadata) {
            e = av_dict_get(stream->metadata, "artist", nullptr, 0);
            if (e) tagArtist = QString::fromUtf8(e->value);
        }
        QString base;
        if (local) {
            base = QFileInfo(path).completeBaseName();
            base.replace('_', ' ');
            base = base.simplified();
            base.remove(QRegularExpression(QStringLiteral("^\\s*\\d{1,3}\\s*[.\\-)]\\s+")));
        }
        QString fallbackArtist, fallbackTitle = base;
        const int sep = base.indexOf(QStringLiteral(" - "));
        if (sep > 0) {
            fallbackArtist = base.left(sep).trimmed();
            fallbackTitle = base.mid(sep + 3).trimmed();
        }
        title = tagTitle.isEmpty() ? fallbackTitle : tagTitle;
        artist = tagArtist.isEmpty() ? fallbackArtist : tagArtist;
        if (title.isEmpty())
            title = QStringLiteral("UNTITLED");

        // duration
        if (fmt->duration > 0)
            durationMs = fmt->duration / 1000;
        else if (stream->duration > 0)
            durationMs = av_rescale_q(stream->duration, stream->time_base, AV_TIME_BASE_Q) / 1000;
        if (durationMs < 0)
            durationMs = 0;

        bitrateKbps = int(stream->codecpar->bit_rate / 1000);
        if (bitrateKbps <= 0 && durationMs > 0 && local) {
            const qint64 bytes = QFileInfo(path).size();
            if (bytes > 0)
                bitrateKbps = int(bytes * 8 / (durationMs / 1000.0) / 1000.0);
        }
        sampleRate = codec->sample_rate;
        channels = codec->ch_layout.nb_channels;

        // resampler -> float stereo 48k
        AVChannelLayout outLayout = AV_CHANNEL_LAYOUT_STEREO;
        if (swr_alloc_set_opts2(&swr, &outLayout, AV_SAMPLE_FMT_FLT, outRate,
                                &codec->ch_layout, codec->sample_fmt,
                                codec->sample_rate, 0, nullptr) < 0 || !swr) {
            lastError = QStringLiteral("Gagal menyiapkan resampler");
            emit q->errorOccurred(lastError);
            cleanup();
            return false;
        }
        if (swr_init(swr) < 0) {
            lastError = QStringLiteral("Gagal inisialisasi resampler");
            emit q->errorOccurred(lastError);
            cleanup();
            return false;
        }

        // audio output
        const QAudioDevice audioDev = QMediaDevices::defaultAudioOutput();
        if (audioDev.isNull()) {
            lastError = QStringLiteral("Tidak ada output audio");
            emit q->errorOccurred(lastError);
            cleanup();
            return false;
        }
        QAudioFormat format;
        format.setSampleFormat(QAudioFormat::Float);
        format.setChannelCount(OUT_CHANNELS);
        format.setSampleRate(TARGET_RATE);
        if (!audioDev.isFormatSupported(format)) {
            format.setSampleRate(audioDev.preferredFormat().sampleRate());
            if (!audioDev.isFormatSupported(format)) {
                lastError = QStringLiteral("Format audio output tidak didukung");
                emit q->errorOccurred(lastError);
                cleanup();
                return false;
            }
        }
        outRate = format.sampleRate();

        ring = new RingBuffer(outRate * RING_SECONDS);
        device = new PcmDevice(ring, &dsp, &framesReal, outRate);
        device->open(QIODevice::ReadOnly);
        QObject::connect(device, &PcmDevice::analysisReady, q, [this](QList<float> spec, QList<float> wave) {
            spectrum.clear();
            spectrum.reserve(spec.size());
            for (float v : spec)
                spectrum.append(double(v));
            waveform.clear();
            waveform.reserve(wave.size());
            for (float v : wave)
                waveform.append(double(v));
            emit q->spectrumChanged();
            emit q->waveformChanged();
        }, Qt::QueuedConnection);

        sink = new QAudioSink(audioDev, format, q);
        sink->setBufferSize(format.bytesForDuration(120000));

        positionMs = 0;
        source = url;
        seekBaseMs.store(0);
        framesReal.store(0);
        finishedFlag.store(false);
        started = true;

        decoder = new DecoderThread(fmt, codec, swr, streamIndex, ring, outRate);
        decoder->start();

        emit q->sourceChanged();
        emit q->metadataChanged();
        emit q->durationChanged();
        emit q->positionChanged();
        return true;
    }
};

/* ==================================================================
   AudioEngine
   ================================================================== */
AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent), d(new Private)
{
    d->q = this;
    qRegisterMetaType<QList<float>>("QList<float>");
    d->loadSettings();

    d->ticker = new QTimer(this);
    d->ticker->setInterval(40);
    connect(d->ticker, &QTimer::timeout, this, [this] { d->tick(); });
    d->ticker->start();

    d->saveTimer = new QTimer(this);
    d->saveTimer->setSingleShot(true);
    d->saveTimer->setInterval(350);
    connect(d->saveTimer, &QTimer::timeout, this, [this] { d->saveNow(); });
}

AudioEngine::~AudioEngine()
{
    d->saveNow();
    d->cleanup();
    delete d;
}

QString AudioEngine::title() const { return d->title; }
QString AudioEngine::artist() const { return d->artist; }
QString AudioEngine::source() const { return d->source; }
qint64 AudioEngine::duration() const { return d->durationMs; }
qint64 AudioEngine::position() const { return d->positionMs; }
bool AudioEngine::playing() const { return d->playing; }
int AudioEngine::bitrate() const { return d->bitrateKbps; }
int AudioEngine::sampleRate() const { return d->sampleRate; }
int AudioEngine::channels() const { return d->channels; }
QVariantList AudioEngine::spectrum() const { return d->spectrum; }
QVariantList AudioEngine::waveform() const { return d->waveform; }
QString AudioEngine::lastError() const { return d->lastError; }

double AudioEngine::volume() const { QMutexLocker lock(&d->dsp.mutex); return d->dsp.volume; }
double AudioEngine::balance() const { QMutexLocker lock(&d->dsp.mutex); return d->dsp.balance; }
bool AudioEngine::eqOn() const { QMutexLocker lock(&d->dsp.mutex); return d->dsp.eqOn; }
bool AudioEngine::eqAuto() const { QMutexLocker lock(&d->dsp.mutex); return d->dsp.eqAuto; }
double AudioEngine::preamp() const { QMutexLocker lock(&d->dsp.mutex); return d->dsp.preampDb; }

QVariantList AudioEngine::bands() const
{
    QMutexLocker lock(&d->dsp.mutex);
    QVariantList out;
    for (int i = 0; i < EQ_BANDS; ++i)
        out.append(d->dsp.gains[i]);
    return out;
}

void AudioEngine::open(const QString &url)
{
    if (url.isEmpty())
        return;
    d->started = false;
    d->setPlaying(false);
    d->openFile(url);
}

void AudioEngine::play()
{
    if (!d->sink && d->source.isEmpty())
        return;
    if (d->finishedFlag.load() && !d->source.isEmpty()) {
        const QString src = d->source;
        open(src);
    }
    if (!d->started || !d->sink)
        return;
    if (d->sink->state() == QAudio::SuspendedState) {
        d->sink->resume();
    } else if (d->sink->state() == QAudio::StoppedState) {
        d->sink->start(d->device);
        if (d->sink->error() != QAudio::NoError) {
            d->lastError = QStringLiteral("Gagal memulai output audio");
            emit errorOccurred(d->lastError);
            return;
        }
    }
    d->finishPendingAt = 0;
    d->setPlaying(true);
}

void AudioEngine::pause()
{
    if (!d->sink)
        return;
    d->sink->suspend();
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
    if (!d->sink)
        return;
    d->sink->suspend();
    d->setPlaying(false);
    seek(0);
}

void AudioEngine::seek(qint64 ms)
{
    if (!d->started || !d->decoder)
        return;
    if (d->durationMs > 0)
        ms = qBound<qint64>(0, ms, d->durationMs);
    else
        ms = qMax<qint64>(0, ms);

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
    if (d->sink) {
        d->sink->reset();
        if (wasPlaying) {
            d->sink->start(d->device);
        }
    }
    emit positionChanged();
}

void AudioEngine::setVolume(double v)
{
    v = qBound(0.0, v, 1.0);
    {
        QMutexLocker lock(&d->dsp.mutex);
        if (qFuzzyCompare(d->dsp.volume + 1.0, v + 1.0))
            return;
        d->dsp.volume = v;
    }
    d->scheduleSave();
    emit volumeChanged();
}

void AudioEngine::setBalance(double v)
{
    v = qBound(-1.0, v, 1.0);
    {
        QMutexLocker lock(&d->dsp.mutex);
        if (qFuzzyCompare(d->dsp.balance + 2.0, v + 2.0))
            return;
        d->dsp.balance = v;
    }
    d->scheduleSave();
    emit balanceChanged();
}

void AudioEngine::setEqOn(bool on)
{
    {
        QMutexLocker lock(&d->dsp.mutex);
        if (d->dsp.eqOn == on)
            return;
        d->dsp.eqOn = on;
        ++d->dsp.generation;
    }
    d->scheduleSave();
    emit eqChanged();
}

void AudioEngine::setEqAuto(bool on)
{
    {
        QMutexLocker lock(&d->dsp.mutex);
        if (d->dsp.eqAuto == on)
            return;
        d->dsp.eqAuto = on;
        ++d->dsp.generation;
    }
    d->scheduleSave();
    emit eqChanged();
}

void AudioEngine::setPreamp(double db)
{
    db = qBound(-12.0, db, 12.0);
    {
        QMutexLocker lock(&d->dsp.mutex);
        if (qFuzzyCompare(d->dsp.preampDb + 24.0, db + 24.0))
            return;
        d->dsp.preampDb = db;
        ++d->dsp.generation;
    }
    d->scheduleSave();
    emit eqChanged();
}

void AudioEngine::setBand(int index, double db)
{
    if (index < 0 || index >= EQ_BANDS)
        return;
    db = qBound(-12.0, db, 12.0);
    {
        QMutexLocker lock(&d->dsp.mutex);
        if (qFuzzyCompare(d->dsp.gains[index] + 24.0, db + 24.0))
            return;
        d->dsp.gains[index] = db;
        ++d->dsp.generation;
    }
    d->scheduleSave();
    emit bandsChanged();
}

void AudioEngine::setBands(const QVariantList &bands)
{
    {
        QMutexLocker lock(&d->dsp.mutex);
        for (int i = 0; i < EQ_BANDS && i < bands.size(); ++i)
            d->dsp.gains[i] = qBound(-12.0, bands[i].toDouble(), 12.0);
        ++d->dsp.generation;
    }
    d->scheduleSave();
    emit bandsChanged();
}

void AudioEngine::resetEq()
{
    {
        QMutexLocker lock(&d->dsp.mutex);
        for (int i = 0; i < EQ_BANDS; ++i)
            d->dsp.gains[i] = 0;
        d->dsp.preampDb = 0;
        ++d->dsp.generation;
    }
    d->scheduleSave();
    emit eqChanged();
    emit bandsChanged();
}

void AudioEngine::saveSettings()
{
    d->saveNow();
}

#include "audioengine.moc"
