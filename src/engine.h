/* ------------------------------------------------------------------
   Linamp — audio engine (SDL3 edition)
   FFmpeg decode → EQ (10 biquad) → volume/balance → SDL audio stream
   + FFT spectrum & waveform for the visualizer.
   Ported from the Qt original; the DSP path is unchanged.
   ------------------------------------------------------------------ */
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct SDL_AudioStream;

constexpr int EQ_BANDS = 10;
constexpr int SPECTRUM_BANDS = 31;
constexpr int WAVEFORM_POINTS = 128;

extern const double EQ_FREQS[EQ_BANDS];

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    /* ---- metadata / state ---- */
    const std::string &title() const;
    const std::string &artist() const;
    const std::string &source() const;
    const std::string &lastError() const;
    int64_t duration() const;
    int64_t position() const;
    bool playing() const;
    int bitrate() const;
    int sampleRate() const;
    int channels() const;

    /* ---- dsp params ---- */
    double volume() const;
    double balance() const;
    bool eqOn() const;
    bool eqAuto() const;
    double preamp() const;
    std::array<double, EQ_BANDS> bands() const;

    void setVolume(double v);
    void setBalance(double v);
    void setEqOn(bool on);
    void setEqAuto(bool on);
    void setPreamp(double db);
    void setBand(int index, double db);
    void resetEq();

    /* ---- transport ---- */
    void open(const std::string &url);
    void play();
    void pause();
    void togglePlay();
    void stop();
    void seek(int64_t ms);
    bool hasSource() const;

    /* ---- driven by the main loop (~25 Hz) ---- */
    void tick();

    /* Release the SDL audio device; must run before SDL_Quit(). */
    void shutdown();

    /* ---- analysis for the visualizer ---- */
    std::vector<float> spectrum() const;
    std::vector<float> waveform() const;

    /* ---- settings (JSON persisted by the app) ---- */
    void loadSettings(double volume, double balance, bool eqOn, bool eqAuto, double preamp,
                      const std::array<double, EQ_BANDS> &bands);
    void saveSettings();

    /* ---- events consumed by the app ---- */
    bool takeFinishedFlag();
    bool takeMetadataFlag();

private:
    class Private;
    Private *d;
};
