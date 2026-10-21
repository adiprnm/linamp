#include "app.h"

#include "theme.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>

namespace {

constexpr int PANEL_W = 275;
constexpr int MAIN_H = 118;
constexpr int EQ_H = 118;
constexpr int PL_MIN_H = 120;
constexpr int PL_DEFAULT_H = 238;
constexpr int TITLE_H = 14;
constexpr int ROW_H = 12;

/* EQ presets — ported from qml/Presets.js */
struct Preset {
    const char *name;
    std::array<double, EQ_BANDS> bands;
    double preamp;
};

const Preset PRESETS[] = {
    {"FLAT", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0},
    {"CLASSICAL", {0, 0, 0, 0, 0, 0, -4, -4, -4, -6}, 0},
    {"CLUB", {0, 0, 8, 5, 5, 5, 3, 0, 0, 0}, 0},
    {"DANCE", {5, 7, 6, 0, 0, -3, -4, -4, 0, 0}, 0},
    {"FULL BASS", {9, 9, 9, 5, 1, -4, -8, -11, -11, -11}, 0},
    {"FULL BASS+TREBLE", {6, 6, 6, 2, -2, -6, -8, -8, -8, -8}, 0},
    {"FULL TREBLE", {-9, -9, -9, -4, 2, 8, 11, 12, 12, 12}, 0},
    {"LAPTOP", {4, 3, 1, 1, 0, -1, -2, -3, -4, -5}, 0},
    {"LARGE HALL", {6, 6, 3, 3, 0, -2, -3, -3, -3, -3}, 0},
    {"LIVE", {-2, 0, 2, 3, 3, 3, 2, 1, 1, 1}, 0},
    {"PARTY", {6, 6, 0, 0, 0, 0, 0, 0, 6, 6}, 0},
    {"POP", {-2, -1, 0, 2, 4, 4, 2, 0, -1, -2}, 0},
    {"REGGAE", {0, 0, 0, -4, 0, 4, 4, 0, 0, 0}, 0},
    {"ROCK", {7, 5, 2, -1, -2, 2, 5, 7, 7, 7}, 0},
    {"SKA", {-1, -3, -3, 0, 3, 6, 6, 5, 3, 1}, 0},
    {"SOFT", {3, 1, 0, -2, -2, -1, 0, 0, 1, 1}, 0},
    {"SOFT ROCK", {3, 3, 1, 0, -1, -1, 0, 1, 2, 2}, 0},
    {"TECHNO", {5, 5, 2, -2, -3, -1, 3, 6, 6, 6}, 0},
    {"VOCAL", {-2, -2, -1, 2, 4, 4, 3, 1, 0, -1}, 0},
};
constexpr int PRESET_COUNT = int(sizeof(PRESETS) / sizeof(PRESETS[0]));

const char *BAND_LABELS[EQ_BANDS] = {"60", "170", "310", "600", "1K", "3K", "6K", "12K", "14K", "16K"};

/* Clickable region that draws nothing (used for the logo / list / panels). */
bool clickArea(Ui &ui, uint64_t id, Rect r)
{
    if (!ui.hovering(r))
        return false;
    if (ui.in.pressed) {
        ui.active = id;
        return false;
    }
    if (ui.active == id && ui.in.released) {
        ui.active = 0;
        return true;
    }
    return false;
}

std::string statePath()
{
    return util::configDir() + "/state.json";
}

/* --- tiny demo-track generator (replaces the embedded mp3s) ------------- */
bool writeWav(const std::string &path, const std::vector<float> &interleaved, int rate)
{
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    const uint32_t dataBytes = uint32_t(interleaved.size() * sizeof(int16_t));
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f);
    u32(36 + dataBytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);                              // PCM
    u16(2);                              // stereo
    u32(uint32_t(rate));
    u32(uint32_t(rate) * 2 * 2);         // byte rate
    u16(4);                              // block align
    u16(16);                             // bits
    std::fwrite("data", 1, 4, f);
    u32(dataBytes);
    for (float s : interleaved) {
        const int v = int(std::lround(std::clamp(s, -1.f, 1.f) * 32000.f));
        const int16_t s16 = int16_t(v);
        std::fwrite(&s16, 2, 1, f);
    }
    std::fclose(f);
    return true;
}

std::vector<float> synthDemo(int variant, int rate)
{
    const double seconds = 7.0;
    const int frames = int(seconds * rate);
    std::vector<float> out(size_t(frames) * 2, 0.f);

    // A minor pentatonic / C major pentatonic arpeggio
    static const int scaleA[] = {220, 261, 293, 329, 392, 440, 523, 587};
    static const int scaleB[] = {196, 246, 261, 329, 392, 493, 523, 659};
    const int *scale = variant == 2 ? scaleB : scaleA;
    const double bpm = variant == 2 ? 112.0 : 96.0;
    const double beat = 60.0 / bpm;
    const double note = beat / 2.0;

    for (int i = 0; i < frames; ++i) {
        const double t = double(i) / rate;
        const int step = int(t / note);
        const double local = t - step * note;
        const double hz = scale[step % 8] * (variant == 2 ? 0.5 : 0.25) * 4.0;
        const double env = std::exp(-local * 9.0);
        // lead (square-ish)
        double lead = 0.0;
        const double ph = std::fmod(t * hz, 1.0);
        lead = (ph < 0.5 ? 1.0 : -1.0) * 0.16 * env;
        // bass on every beat
        const int beatIdx = int(t / beat);
        const double bassLocal = t - beatIdx * beat;
        const double bassHz = (beatIdx % 4 == 0 ? 55.0 : (beatIdx % 4 == 2 ? 73.4 : 65.4));
        const double bassEnv = std::exp(-bassLocal * 6.0);
        const double bass = std::sin(2.0 * M_PI * bassHz * t) * 0.30 * bassEnv;
        // kick
        const double kickEnv = std::exp(-bassLocal * 24.0);
        const double kick = std::sin(2.0 * M_PI * (90.0 - 60.0 * bassLocal) * t) * 0.35 * kickEnv;
        // hat on off-beats
        const double hatLocal = std::fmod(t, beat / 2.0);
        const double hat = (std::rand() % 2 ? 1.0 : -1.0) * 0.0; // deterministic silence
        (void)hat;
        const double hatEnv = std::exp(-hatLocal * 90.0) * 0.05;
        const double noise = (std::sin(t * 39017.0) * 0.5 + std::sin(t * 12345.0) * 0.5) * hatEnv;

        const double s = lead + bass + kick + noise;
        const double fade = std::min(1.0, std::min(t * 4.0, (seconds - t) * 4.0));
        out[size_t(i) * 2 + 0] = float(s * fade);
        out[size_t(i) * 2 + 1] = float(s * fade);
    }
    return out;
}

} // namespace

/* ==================================================================
   init / shutdown
   ================================================================== */
static SDL_HitTestResult SDLCALL hitTestCb(SDL_Window *, const SDL_Point *area, void *data)
{
    return static_cast<App *>(data)->hitTest(area->x, area->y);
}

bool App::init()
{
    m_uiFont.load();
    m_ui.font = &m_uiFont;

    // restore state first: it decides the zoom factor and the panel layout
    restoreState();
    if (!m_hadLayout)
        layoutPanels();

    m_win = SDL_CreateWindow("LINAMP", m_winW * m_zoom, m_winH * m_zoom,
                             SDL_WINDOW_TRANSPARENT | SDL_WINDOW_BORDERLESS |
                                 SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!m_win) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetWindowMinimumSize(m_win, 580 * m_zoom, 400 * m_zoom);
    SDL_SetWindowHitTest(m_win, hitTestCb, this);

    m_ren = SDL_CreateRenderer(m_win, nullptr);
    if (!m_ren) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(m_ren, 1);

    updateRenderMetrics();

    clampPanels();

    m_stateReady = true;
    bump();
    saveStateNow();

    m_lastFrame = double(SDL_GetTicksNS()) / 1e9;
    m_lastEngineTick = m_lastFrame;
    m_now = m_lastFrame;
    return true;
}

void App::updateRenderMetrics()
{
    int devW = m_winW;
    int devH = m_winH;
    if (m_ren) {
        int ow = 0, oh = 0;
        if (SDL_GetRenderOutputSize(m_ren, &ow, &oh) && ow > 0 && oh > 0) {
            devW = ow;
            devH = oh;
        }
    }
    float sx = float(devW) / float(std::max(1, m_winW));
    float sy = float(devH) / float(std::max(1, m_winH));

    // manual override (also used to emulate a HiDPI display in tests)
    if (const char *env = std::getenv("LINAMP_PIXEL_SCALE"); env && *env) {
        const float v = float(std::atof(env));
        if (v > 0.1f) {
            sx = sy = v;
            devW = std::max(1, int(std::lround(m_winW * v)));
            devH = std::max(1, int(std::lround(m_winH * v)));
        }
    }
    if (sx < 0.5f)
        sx = 1.f;
    if (sy < 0.5f)
        sy = 1.f;

    m_pixelScale = sx;
    m_canvas.reset(devW, devH);
    m_canvas.setScale(sx, sy);
    m_ui.cv = &m_canvas;
    m_uiFont.setPixelScale(sx);

    if (m_tex)
        SDL_DestroyTexture(m_tex);
    m_tex = nullptr;
    if (m_ren) {
        m_tex = SDL_CreateTexture(m_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                  devW, devH);
        if (m_tex) {
            SDL_SetTextureBlendMode(m_tex, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(m_tex, SDL_SCALEMODE_NEAREST);
        }
    }
}

void App::shutdown()
{
    saveStateNow();
    m_stateReady = false;
    m_engine.shutdown(); // stop audio before SDL_Quit() tears the subsystem down
    if (m_tex)
        SDL_DestroyTexture(m_tex);
    m_tex = nullptr;
    if (m_ren)
        SDL_DestroyRenderer(m_ren);
    m_ren = nullptr;
    if (m_win)
        SDL_DestroyWindow(m_win);
    m_win = nullptr;
}

/* ==================================================================
   panels geometry
   ================================================================== */
Rect App::panelRect(int panel) const
{
    switch (panel) {
    case PANEL_MAIN:
        return {m_main.x, m_main.y, PANEL_W, m_main.shaded ? TITLE_H : MAIN_H};
    case PANEL_EQ:
        return {m_eq.x, m_eq.y, PANEL_W, m_eq.shaded ? TITLE_H : EQ_H};
    case PANEL_PLAYLIST:
        return {m_playlist.x, m_playlist.y, PANEL_W, m_playlist.shaded ? TITLE_H : m_playlist.height};
    }
    return {};
}

bool panelVisible(const App *app, int panel); // fwd

int App::topPanelAt(float x, float y) const
{
    for (auto it = m_zorder.rbegin(); it != m_zorder.rend(); ++it) {
        const int p = *it;
        if (p == PANEL_EQ && !m_eqOpen)
            continue;
        if (p == PANEL_PLAYLIST && !m_playlistOpen)
            continue;
        if (panelRect(p).contains(int(x), int(y)))
            return p;
    }
    return -1;
}

void App::raisePanel(int panel)
{
    auto it = std::find(m_zorder.begin(), m_zorder.end(), panel);
    if (it == m_zorder.end())
        return;
    m_zorder.erase(it);
    m_zorder.push_back(panel);
    m_focusPanel = panel;
    m_front = panel == PANEL_MAIN ? "mainPanel" : (panel == PANEL_EQ ? "eqPanel" : "playlistPanel");
    scheduleSave();
}

void App::layoutPanels()
{
    const int gap = 8;
    const int groupW = PANEL_W * 2 + gap;
    const int groupH = MAIN_H + gap + MAIN_H;
    const int left = std::max(8, (m_winW - groupW) / 2);
    const int top = std::max(28, (m_winH - groupH) / 2);

    m_main.x = left;
    m_main.y = top;
    m_playlist.x = left + PANEL_W + gap;
    m_playlist.y = top;
    m_eq.x = left;
    m_eq.y = top + MAIN_H + gap;
}

void App::clampPanels()
{
    const int panels[3] = {PANEL_MAIN, PANEL_PLAYLIST, PANEL_EQ};
    for (int p : panels) {
        Rect r = panelRect(p);
        int x = theme::clampInt(r.x, 0, std::max(0, m_winW - r.w));
        int y = theme::clampInt(r.y, 0, std::max(0, m_winH - r.h));
        if (p == PANEL_MAIN) {
            m_main.x = x;
            m_main.y = y;
        } else if (p == PANEL_EQ) {
            m_eq.x = x;
            m_eq.y = y;
        } else {
            m_playlist.x = x;
            m_playlist.y = y;
        }
    }
    scheduleSave();
}

void App::snapPanel(int panel, int &nx, int &ny)
{
    const int th = 10;
    Rect self = panelRect(panel);
    std::vector<int> xs = {0, m_winW - self.w};
    std::vector<int> ys = {0, m_winH - self.h};

    const int panels[3] = {PANEL_MAIN, PANEL_PLAYLIST, PANEL_EQ};
    for (int p : panels) {
        if (p == panel)
            continue;
        if (p == PANEL_EQ && !m_eqOpen)
            continue;
        if (p == PANEL_PLAYLIST && !m_playlistOpen)
            continue;
        const Rect o = panelRect(p);
        xs.push_back(o.x);
        xs.push_back(o.right() - self.w);
        xs.push_back(o.x - self.w);
        xs.push_back(o.right());
        ys.push_back(o.y);
        ys.push_back(o.bottom() - self.h);
        ys.push_back(o.y - self.h);
        ys.push_back(o.bottom());
    }

    int bx = nx, by = ny;
    int dx = th + 1, dy = th + 1;
    for (int x : xs) {
        const int d = std::abs(x - nx);
        if (d < dx) {
            dx = d;
            bx = x;
        }
    }
    for (int y : ys) {
        const int d = std::abs(y - ny);
        if (d < dy) {
            dy = d;
            by = y;
        }
    }
    bx = theme::clampInt(bx, 0, std::max(0, m_winW - self.w));
    by = theme::clampInt(by, 0, std::max(0, m_winH - self.h));
    nx = bx;
    ny = by;
}

/* ==================================================================
   hit test (window move / resize handled by the compositor)
   ================================================================== */
SDL_HitTestResult App::hitTest(int wx, int wy) const
{
    if (m_helpOpen)
        return SDL_HITTEST_NORMAL;

    const int x = wx / std::max(1, m_zoom);
    const int y = wy / std::max(1, m_zoom);

    const int edge = 5;
    if (x < edge && y < edge)
        return SDL_HITTEST_RESIZE_TOPLEFT;
    if (x > m_winW - edge && y < edge)
        return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (x < edge && y > m_winH - edge)
        return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (x > m_winW - edge && y > m_winH - edge)
        return SDL_HITTEST_RESIZE_BOTTOMRIGHT;
    if (y < edge)
        return SDL_HITTEST_RESIZE_TOP;
    if (y > m_winH - edge)
        return SDL_HITTEST_RESIZE_BOTTOM;
    if (x < edge)
        return SDL_HITTEST_RESIZE_LEFT;
    if (x > m_winW - edge)
        return SDL_HITTEST_RESIZE_RIGHT;

    // over a panel -> normal interaction, empty desktop -> move the window
    if (topPanelAt(float(x), float(y)) >= 0)
        return SDL_HITTEST_NORMAL;
    return SDL_HITTEST_DRAGGABLE;
}

/* ==================================================================
   frame
   ================================================================== */
bool App::wantsFastFrames() const
{
    if (m_inputDirty || m_ui.in.down || m_dragPanel >= 0 || m_resizingPlaylist)
        return true;
    if (m_engine.playing())
        return true;
    if (m_now < m_eqReadoutUntil)
        return true;
    if (pf::measure(m_marqueeText, 1, 1) > 177)
        return true;
    if (m_visMode == "bars") {
        for (float v : m_visVals)
            if (v > 0.01f)
                return true;
    }
    return false;
}

void App::frame()
{
    m_inputDirty = false;
    const double now = double(SDL_GetTicksNS()) / 1e9;
    const double dt = std::min(0.1, now - m_lastFrame);
    m_lastFrame = now;
    m_now = now;

    // engine tick (~25 Hz)
    if (now - m_lastEngineTick >= 0.04) {
        m_lastEngineTick = now;
        m_engine.tick();
        if (m_engine.takeMetadataFlag() && m_currentIndex >= 0 &&
            m_currentIndex < int(m_tracks.size())) {
            Track &t = m_tracks[size_t(m_currentIndex)];
            if (!m_engine.title().empty())
                t.title = m_engine.title();
            if (!m_engine.artist().empty())
                t.artist = m_engine.artist();
            if (m_engine.duration() > 0)
                t.duration = m_engine.duration();
            updateMarquee();
            bump();
        }
        if (m_engine.takeFinishedFlag())
            next(true);
    }

    // prober results
    for (const ProbeResult &r : m_prober.takeResults()) {
        if (r.durationMs <= 0)
            continue;
        for (size_t i = 0; i < m_tracks.size(); ++i) {
            if (m_tracks[i].url == r.url) {
                m_tracks[i].duration = r.durationMs;
                bump();
                break;
            }
        }
    }

    handleDialogResults();

    if (m_saveAt > 0 && now >= m_saveAt)
        saveStateNow();

    m_marqueeOffset += dt * 28.0; // px per second (QML: 0.028 px/ms)

    drawFrame();
    present();

    // reset one-shot input
    m_ui.in.pressed = false;
    m_ui.in.released = false;
    m_ui.in.rightPressed = false;
    m_ui.in.rightReleased = false;
    m_ui.in.doubleClick = false;
    m_ui.in.wheel = 0.f;
}

/* ==================================================================
   frame composition
   ================================================================== */
void App::drawFrame()
{
    m_canvas.reset(m_canvas.width(), m_canvas.height());
    m_ui.cv = &m_canvas;

    const int top = topPanelAt(m_ui.in.mx, m_ui.in.my);

    if (m_menu.open)
        m_menu.handle(m_ui);

    m_ui.blockPanels = m_menu.open || m_helpOpen || !m_dragInfo.empty();
    m_ui.tipZ = 0;

    handleTitlebarDrag();

    for (int p : m_zorder) {
        if (p == PANEL_EQ && !m_eqOpen)
            continue;
        if (p == PANEL_PLAYLIST && !m_playlistOpen)
            continue;
        m_ui.beginRegion(panelRect(p), p == top);
        if (p == PANEL_MAIN)
            drawMainPanel();
        else if (p == PANEL_EQ)
            drawEqPanel();
        else
            drawPlaylistPanel();
        m_ui.endRegion();
    }

    if (m_helpOpen)
        drawHelpOverlay();
    if (!m_dragInfo.empty())
        drawDropOverlay();

    m_menu.draw(m_ui);
    m_ui.drawTooltips();
}

std::string App::debugState() const
{
    char buf[512];
    int ww = 0, wh = 0;
    if (m_win)
        SDL_GetWindowSize(m_win, &ww, &wh);
    std::snprintf(buf, sizeof buf,
                  "zoom=%d win=%dx%d anchor=%d focus=%d playing=%d pos=%lld dur=%lld tracks=%zu "
                  "current=%d "
                  "eqOpen=%d plOpen=%d "
                  "helpOpen=%d menuOpen=%d shade=%d sel=%d vol=%.2f eqPreset=%s main=(%d,%d)",
                  m_zoom, ww, wh, m_anchor, focusedPanel(), m_engine.playing() ? 1 : 0,
                  (long long)m_engine.position(),
                  (long long)m_engine.duration(), m_tracks.size(), m_currentIndex,
                  m_eqOpen ? 1 : 0, m_playlistOpen ? 1 : 0, m_helpOpen ? 1 : 0,
                  m_menu.open ? 1 : 0, m_main.shaded ? 1 : 0, m_selectionCount, m_engine.volume(),
                  m_eqPreset.c_str(), m_main.x, m_main.y);
    return buf;
}

bool App::saveScreenshot(const std::string &path)
{
    const int w = m_canvas.width();
    const int h = m_canvas.height();
    SDL_Surface *surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
    if (!surf)
        return false;
    for (int y = 0; y < h; ++y)
        std::memcpy(static_cast<uint8_t *>(surf->pixels) + size_t(y) * size_t(surf->pitch),
                    m_canvas.data() + size_t(y) * size_t(w), size_t(w) * 4);
    const bool ok = SDL_SaveBMP(surf, path.c_str());
    SDL_DestroySurface(surf);
    return ok;
}

/* Adds the demo tracks synchronously (used by the screenshot mode). */
void App::setPlaylistForTest(const std::string &scene)
{
    m_tracks.clear();
    m_selected.clear();
    m_eqOpen = false;
    m_playlistOpen = true;
    m_helpOpen = false;
    m_eqPreset = "ROCK";
    const std::string dir = util::cacheDir() + "/demos";
    util::ensureDir(dir);
    for (int v = 1; v <= 2; ++v) {
        const std::string path =
            dir + "/LINAMP - DEMO LOOP " + std::to_string(v) + ".wav";
        if (!util::fileExists(path))
            writeWav(path, synthDemo(v, 44100), 44100);
        Track t;
        t.url = "file://" + util::percentEncodePath(path);
        t.name = std::string("LINAMP - DEMO LOOP ") + char('0' + v);
        t.title = std::string("DEMO LOOP ") + char('0' + v);
        t.artist = "LINAMP";
        t.duration = 7000;
        m_tracks.push_back(t);
        m_selected.push_back(v == 1);
    }
    m_currentIndex = 0;
    m_totalDuration = 14000;
    m_selectionCount = 1;
    updateMarquee();

    if (scene == "eq") {
        m_eqOpen = true;
        for (int i = 0; i < EQ_BANDS; ++i)
            m_eqGains[size_t(i)] = PRESETS[13].bands[size_t(i)];
        m_playlistOpen = false;
    } else if (scene == "help") {
        m_helpOpen = true;
    } else if (scene == "menu") {
        openMainMenu();
    } else if (scene == "drop") {
        m_dragInfo = "DROP AUDIO FILES TO PLAYLIST";
    } else if (scene == "playing") {
        playIndex(0);
    } else if (scene == "showcase") {
        m_eqOpen = true;
        m_playlistOpen = true;
        m_eqPreset = "FLAT";
        m_eqGains.fill(0.0);
        playIndex(0);
    } else if (scene == "playlist-menu") {
        m_menu.show(m_playlist.x + 165, m_playlist.y + m_playlist.height - 13, 200, 1,
                    {{"Sort by title", "", false, true, false, nullptr},
                     {"Sort by duration", "", false, false, false, nullptr},
                     {"Reverse order", "", false, false, false, nullptr},
                     {"", "", true},
                     {"Shuffle list", "", false, false, false, nullptr}});
    }
}

void App::present()
{
    if (!m_tex || !m_ren)
        return;
    SDL_UpdateTexture(m_tex, nullptr, m_canvas.data(), m_canvas.pitch());
    SDL_SetRenderDrawColor(m_ren, 0, 0, 0, 0);
    SDL_RenderClear(m_ren);
    SDL_RenderTexture(m_ren, m_tex, nullptr, nullptr);
    SDL_RenderPresent(m_ren);
}

/* ==================================================================
   panel chrome / title bar
   ================================================================== */
void App::drawPanelFrame(int panel, const std::string &title, const std::string &version,
                         bool showLogo)
{
    Canvas &cv = *m_ui.cv;
    const Rect r = panelRect(panel);
    widgets::panelChrome(cv, r, title, version, showLogo, !showLogo);

    Rect close{r.right() - 3 - 12, r.y + 1, 12, 12};
    Rect shade = close.offset(-13, 0);
    Rect mini = shade.offset(-13, 0);

    ButtonDesc d;
    d.icon = "min";
    d.tip = "Minimize";
    if (m_ui.button(fnv("tbmin") + uint64_t(panel), mini, d).clicked) {
        if (panel == PANEL_MAIN) {
            SDL_MinimizeWindow(m_win);
        } else {
            if (panel == PANEL_EQ)
                m_eqOpen = false;
            else
                m_playlistOpen = false;
            scheduleSave();
        }
    }
    d.icon = "shade";
    d.tip = "Shade";
    if (m_ui.button(fnv("tbshade") + uint64_t(panel), shade, d).clicked) {
        bool &sh = panel == PANEL_MAIN   ? m_main.shaded
                   : panel == PANEL_EQ   ? m_eq.shaded
                                         : m_playlist.shaded;
        sh = !sh;
        scheduleSave();
    }
    d.icon = "close";
    d.tip = "Close";
    if (m_ui.button(fnv("tbclose") + uint64_t(panel), close, d).clicked) {
        if (panel == PANEL_MAIN) {
            m_running = false;
        } else if (panel == PANEL_EQ) {
            m_eqOpen = false;
            scheduleSave();
        } else {
            m_playlistOpen = false;
            scheduleSave();
        }
    }

    if (panel == PANEL_MAIN) {
        const Rect logo{r.x + 3, r.y + 1, 14, 12};
        if (clickArea(m_ui, fnv("logo"), logo))
            openMainMenu();
    }
}

/* ==================================================================
   main panel
   ================================================================== */
void App::drawTimeLcd(Rect r)
{
    Canvas &cv = *m_ui.cv;
    widgets::lcdBox(cv, r);

    std::string txt;
    if (m_remaining && m_engine.duration() > 0) {
        const double left = std::max(0.0, double(m_engine.duration() - m_engine.position()));
        txt = "-" + theme::fmt(left);
    } else {
        txt = theme::fmt(double(m_engine.position()));
    }

    int scale = 3;
    while (scale > 2 && pf::measure(txt, scale, 2) > r.w - 4)
        --scale;
    const int w = pf::measure(txt, scale, 2);
    const int h = pf::CH_H * scale;
    const Rect inner = r.inset(1);
    pf::draw(cv, txt, inner.x + (inner.w - w) / 2, inner.y + (inner.h - h) / 2, scale,
             m_engine.playing() ? theme::lcdGreen : theme::lcdDim, 2);
}

void App::drawSpectrum(Rect r)
{
    Canvas &cv = *m_ui.cv;
    const int W = r.w, H = r.h;

    // grid
    for (int yy = H - 8; yy > 0; yy -= 8)
        cv.fillRect({r.x, r.y + yy, W, 1}, rgb(0x28ff50, 14));
    for (int xx = 0; xx < W; xx += 5)
        cv.fillRect({r.x + xx, r.y, 1, H}, rgb(0x28ff50, 14));

    const bool playing = m_engine.playing();

    if (m_visMode == "osc") {
        const std::vector<float> wave = m_engine.waveform();
        if (wave.size() < 2)
            return;
        const int n = int(wave.size());
        int px = 0, py = 0;
        for (int i = 0; i < n; ++i) {
            const int x = int(i * (W - 1) / (n - 1));
            const int y = int(H / 2.0 - wave[size_t(i)] * (H / 2.0 - 1) * 0.9);
            if (i > 0)
                cv.line(r.x + px, r.y + py, r.x + x, r.y + y, rgb(0x3bff5a));
            px = x;
            py = y;
        }
        return;
    }

    const std::vector<float> spec = m_engine.spectrum();
    const float step = float(W) / float(SPECTRUM_BANDS);
    const int bw = std::max(2, int(std::floor(step)) - 1);
    const int gEnd = int(std::lround(H - H * 0.58));
    const int yEnd = int(std::lround(H - H * 0.83));

    for (int b = 0; b < SPECTRUM_BANDS; ++b) {
        float target = 0.f;
        if (playing && int(spec.size()) > b)
            target = spec[size_t(b)];
        const float prev = m_visVals[size_t(b)];
        float val = playing ? std::max(target, prev * 0.85f) : prev * 0.90f;
        if (!playing && target > val)
            val = target;
        m_visVals[size_t(b)] = val;
        const float peak = std::max(val, m_visPeaks[size_t(b)] - 0.02f);
        m_visPeaks[size_t(b)] = peak;

        const int bh = int(std::lround(val * (H - 1)));
        if (bh <= 0)
            continue;
        const int x = int(std::lround(b * step));
        const int yTop = H - bh;

        const int y0 = std::max(yTop, gEnd);
        if (H - y0 > 0)
            cv.fillRect({r.x + x, r.y + y0, bw, std::min(bh, H - y0)}, rgb(0x1fe23c));
        if (yTop < gEnd) {
            const int y1 = std::max(yTop, yEnd);
            if (gEnd - y1 > 0)
                cv.fillRect({r.x + x, r.y + y1, bw, gEnd - y1}, rgb(0xdfe321));
        }
        if (yTop < yEnd)
            cv.fillRect({r.x + x, r.y + yTop, bw, yEnd - yTop}, rgb(0xff3b2f));

        const int ph = int(std::lround(peak * (H - 1)));
        if (peak > 0.04f)
            cv.fillRect({r.x + x, r.y + H - ph - 2, bw, 1}, rgb(0xdcffe1, 217));
    }
}

void App::drawMarquee(Rect box)
{
    Canvas &cv = *m_ui.cv;
    const std::string txt = pf::norm(m_marqueeText);
    if (txt.empty())
        return;
    const int full = pf::measure(txt, 1, 1);
    const Color color = m_engine.playing() ? rgb(0x3bff5a) : rgb(0x1f9a34);

    cv.pushClip(box);
    if (full <= box.w - 4) {
        pf::draw(cv, txt, box.x + 2, box.y + 2, 1, color);
    } else {
        const int gap = 24;
        const double period = double(full + gap);
        int offset = int(std::fmod(m_marqueeOffset, period));
        pf::draw(cv, txt, box.x + 2 - offset, box.y + 2, 1, color);
        pf::draw(cv, txt, box.x + 2 - offset + full + gap, box.y + 2, 1, color);
    }
    cv.popClip();
}

void App::drawStatusText(Rect box)
{
    Canvas &cv = *m_ui.cv;
    std::string txt;
    if (m_engine.bitrate() > 0) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%3dK %dK %s", m_engine.bitrate(),
                      int(std::lround(m_engine.sampleRate() / 1000.0)),
                      m_engine.channels() > 1 ? "ST" : "MO");
        txt = buf;
    } else {
        txt = "---K --K --";
    }
    const int w = pf::measure(txt, 1, 1);
    pf::draw(cv, txt, box.right() - w, box.y, 1, rgb(0x1f9a34));
}

void App::drawMainPanel()
{
    Canvas &cv = *m_ui.cv;
    drawPanelFrame(PANEL_MAIN, "LINAMP", "1.0", true);
    if (m_main.shaded)
        return;

    const Rect r = panelRect(PANEL_MAIN);
    const int bx = r.x;
    const int by = r.y + TITLE_H;

    /* ---- display ---- */
    const Rect disp{bx + 8, by + 2, 259, 38};
    cv.fillRect(disp, rgb(0x202020));
    cv.vGrad(disp, {{0.f, rgb(0x303030)}, {1.f, rgb(0x1e1e1e)}});
    cv.outline(disp, rgb(0x0a0a0a));
    cv.hLine(disp.x + 1, disp.y + 1, disp.w - 2, rgb(0x4c4c4c));
    cv.hLine(disp.x + 1, disp.bottom() - 2, disp.w - 2, rgb(0x0d0d0d));

    const Rect timeBox{disp.x + 2, disp.y + 2, 92, 32};
    drawTimeLcd(timeBox);
    if (clickArea(m_ui, fnv("timelcd"), timeBox))
        toggleRemaining();

    const Rect visBox{disp.x + 98, disp.y + 2, 155, 32};
    widgets::lcdBox(cv, visBox);
    drawSpectrum(visBox.inset(1));
    if (m_ui.hovering(visBox) && m_ui.in.doubleClick)
        toggleVis();

    /* ---- marquee + stream info ---- */
    const Rect mq{bx + 8, by + 42, 259, 14};
    widgets::lcdBox(cv, mq);
    const Rect mqContent = mq.inset(1);
    drawMarquee({mqContent.x + 3, mqContent.y, 181, 12});
    drawStatusText({mqContent.x + 188, mqContent.y + 3, 66, 7});

    /* ---- volume / balance ---- */
    const Rect row{bx + 8, by + 58, 259, 14};
    widgets::icon(cv, {row.x + 1, row.y + 1, 12, 12}, "speaker", rgb(0xcfcfcf));

    double vol = m_engine.volume();
    if (m_ui.hSlider(fnv("vol"), {row.x + 16, row.y, 76, 14}, vol, 0, 1, 0.82, m_dragVol))
        m_engine.setVolume(vol);
    pf::draw(cv, "L", row.x + 102, row.y + 4, 1, rgb(0x9a9a9a));

    double bal = m_engine.balance();
    if (m_ui.hSlider(fnv("bal"), {row.x + 112, row.y, 84, 14}, bal, -1, 1, 0, m_dragBal))
        m_engine.setBalance(bal);
    pf::draw(cv, "R", row.x + 198, row.y + 4, 1, rgb(0x9a9a9a));

    ButtonDesc d;
    d.label = "EQ";
    d.showLed = true;
    d.active = m_eqOpen;
    d.tip = "Equalizer (E)";
    if (m_ui.button(fnv("btnEQ"), {row.x + 210, row.y, 26, 14}, d).clicked)
        toggleEq();
    d = ButtonDesc{};
    d.label = "PL";
    d.showLed = true;
    d.active = m_playlistOpen;
    d.tip = "Playlist (L)";
    if (m_ui.button(fnv("btnPL"), {row.x + 238, row.y, 21, 14}, d).clicked)
        togglePlaylist();

    /* ---- seek ---- */
    SeekResult sr = m_ui.seekBar(fnv("seek"), {bx + 8, by + 74, 259, 8},
                                 double(m_engine.position()), double(m_engine.duration()),
                                 m_dragSeek, m_seekPreview);
    if (sr.seek)
        m_engine.seek(int64_t(sr.seekTo));

    /* ---- transport ---- */
    const Rect tr{bx + 8, by + 86, 259, 18};
    d = ButtonDesc{};
    d.icon = "prev";
    d.tip = "Previous (Z)";
    if (m_ui.button(fnv("trprev"), {tr.x + 0, tr.y, 24, 18}, d).clicked)
        prev();
    d = ButtonDesc{};
    d.icon = "play";
    d.tip = "Play (X)";
    d.active = m_engine.playing();
    if (m_ui.button(fnv("trplay"), {tr.x + 24, tr.y, 24, 18}, d).clicked)
        togglePlay();
    d = ButtonDesc{};
    d.icon = "pause";
    d.tip = "Pause (C)";
    if (m_ui.button(fnv("trpause"), {tr.x + 48, tr.y, 24, 18}, d).clicked)
        m_engine.pause();
    d = ButtonDesc{};
    d.icon = "stop";
    d.tip = "Stop (V)";
    if (m_ui.button(fnv("trstop"), {tr.x + 72, tr.y, 24, 18}, d).clicked)
        stopPlayback();
    d = ButtonDesc{};
    d.icon = "next";
    d.tip = "Next (B)";
    if (m_ui.button(fnv("trnext"), {tr.x + 96, tr.y, 24, 18}, d).clicked)
        next(false);
    d = ButtonDesc{};
    d.icon = "eject";
    d.tip = "Open files (L)";
    if (m_ui.button(fnv("treject"), {tr.x + 120, tr.y, 24, 18}, d).clicked)
        requestFiles();
    d = ButtonDesc{};
    d.icon = "shuffle";
    d.tip = "Shuffle (S)";
    d.active = m_shuffleOn;
    if (m_ui.button(fnv("trshuffle"), {tr.x + 150, tr.y, 47, 18}, d).clicked)
        toggleShuffle();
    d = ButtonDesc{};
    d.icon = "repeat";
    d.tip = "Repeat (R)";
    d.active = m_repeatOn;
    if (m_ui.button(fnv("trrepeat"), {tr.x + 204, tr.y, 47, 18}, d).clicked)
        toggleRepeat();
}

/* ==================================================================
   equalizer panel
   ================================================================== */
void App::drawEqPanel()
{
    Canvas &cv = *m_ui.cv;
    drawPanelFrame(PANEL_EQ, "EQUALIZER", "", false);
    if (m_eq.shaded)
        return;

    const Rect r = panelRect(PANEL_EQ);
    const int bx = r.x;
    const int by = r.y + TITLE_H;

    /* ---- toolbar ---- */
    ButtonDesc d;
    d.label = "ON";
    d.showLed = true;
    d.active = m_engine.eqOn();
    d.tip = "EQ on/off";
    if (m_ui.button(fnv("eqon"), {bx + 8, by + 2, 32, 14}, d).clicked)
        m_engine.setEqOn(!m_engine.eqOn());

    d = ButtonDesc{};
    d.label = "AUTO";
    d.showLed = true;
    d.active = m_engine.eqAuto();
    d.tip = "Auto gain";
    if (m_ui.button(fnv("eqauto"), {bx + 43, by + 2, 38, 14}, d).clicked)
        m_engine.setEqAuto(!m_engine.eqAuto());

    d = ButtonDesc{};
    d.label = m_eqPreset;
    d.trailingIcon = "caret";
    d.tip = "Presets";
    if (m_ui.button(fnv("eqpreset"), {bx + 85, by + 2, 135, 14}, d).clicked)
        openPresetMenu();

    d = ButtonDesc{};
    d.label = "RESET";
    d.tip = "Reset ke flat";
    if (m_ui.button(fnv("eqreset"), {bx + 224, by + 2, 35, 14}, d).clicked)
        applyPreset("FLAT", PRESETS[0].bands, 0.0);

    /* ---- bands ---- */
    const int rowW = 24 + EQ_BANDS * 22;
    const int rowX = bx + 8 + (259 - rowW) / 2;
    const int rowY = by + 20;

    // preamp
    cv.fillRect({rowX + 3, rowY + 29, 18, 1}, rgb(0x5c5c5c));
    double preamp = m_engine.preamp();
    if (m_ui.vSlider(fnv("preamp"), {rowX, rowY, 24, 58}, preamp, -12, 12, 0, m_dragPreamp)) {
        m_engine.setPreamp(preamp);
        m_eqPreset = "CUSTOM";
        m_eqReadout = "PREAMP " + theme::signedNum(preamp) + " DB";
        m_eqReadoutUntil = m_now + 1.6;
    }
    {
        const int w = pf::measure("PRE", 1, 1);
        pf::draw(cv, "PRE", rowX + (24 - w) / 2, rowY + 60, 1, rgb(0x8a9c8a));
    }

    for (int i = 0; i < EQ_BANDS; ++i) {
        const int itemX = rowX + 24 + i * 22;
        cv.fillRect({itemX + 3, rowY + 29, 16, 1}, rgb(0x5c5c5c));
        double gain = m_eqGains[size_t(i)];
        if (m_ui.vSlider(fnv("band") + uint64_t(i), {itemX, rowY, 22, 58}, gain, -12, 12, 0,
                         m_dragBand[size_t(i)])) {
            m_eqGains[size_t(i)] = gain;
            m_engine.setBand(i, gain);
            m_eqPreset = "CUSTOM";
            m_eqReadout = std::string(BAND_LABELS[i]) + " " + theme::signedNum(gain) + " DB";
            m_eqReadoutUntil = m_now + 1.6;
        }
        const char *label = BAND_LABELS[i];
        const int w = pf::measure(label, 1, 1);
        pf::draw(cv, label, itemX + (22 - w) / 2, rowY + 60, 1, rgb(0x8a9c8a));
    }

    /* ---- readout ---- */
    const Rect readout{bx + 8, by + 92, 259, 12};
    widgets::lcdBox(cv, readout);

    const std::string pre = "PREAMP " + theme::signedNum(m_engine.preamp()) + " DB";
    pf::draw(cv, pre, readout.x + 4, readout.y + 2, 1, theme::lcdSoft);

    const std::string center = m_now < m_eqReadoutUntil ? m_eqReadout : std::string();
    if (!center.empty()) {
        const int w = pf::measure(center, 1, 1);
        pf::draw(cv, center, readout.x + (readout.w - w) / 2, readout.y + 2, 1, rgb(0x8fe09a));
    }

    const std::string right = m_engine.eqOn() ? "EQ ON" : "EQ OFF";
    const int rw = pf::measure(right, 1, 1);
    pf::draw(cv, right, readout.right() - 4 - rw, readout.y + 2, 1,
             m_engine.eqOn() ? rgb(0x43ff5f) : rgb(0x2a5c32));
}

/* ==================================================================
   playlist panel
   ================================================================== */
void App::drawPlaylistPanel()
{
    Canvas &cv = *m_ui.cv;
    drawPanelFrame(PANEL_PLAYLIST, "PLAYLIST", "", false);
    if (m_playlist.shaded)
        return;

    const Rect r = panelRect(PANEL_PLAYLIST);
    const int bx = r.x;
    const int by = r.y + TITLE_H;
    const int bodyH = r.h - TITLE_H;
    const int listH = bodyH - 34;
    const int viewH = listH - 12;
    const int n = int(m_tracks.size());

    const int maxScroll = std::max(0, n * ROW_H - viewH);
    m_plScroll = theme::clampInt(m_plScroll, 0, maxScroll);

    const Rect listArea{bx, by, PANEL_W, listH};

    /* ---- rows ---- */
    cv.fillRect(listArea, rgb(0x040704));
    cv.fillRect({listArea.x, listArea.y, listArea.w, 12}, rgb(0x0b130b));
    cv.fillRect({listArea.x, listArea.y + 11, listArea.w, 1}, rgb(0x1e2a1e));
    pf::draw(cv, "#", bx + 8, by + 3, 1, rgb(0x517051));
    pf::draw(cv, "TITLE", bx + 30, by + 3, 1, rgb(0x517051));
    {
        const std::string t = "TIME";
        pf::draw(cv, t, bx + PANEL_W - 10 - pf::measure(t, 1, 1), by + 3, 1, rgb(0x517051));
    }

    const bool hoveringList = m_ui.hovering(listArea);
    auto rowAt = [&](float localY) {
        if (localY < 12)
            return -1;
        const int i = int(std::floor((localY - 12 + m_plScroll) / ROW_H));
        return (i >= 0 && i < n) ? i : -1;
    };
    m_plHover = hoveringList ? rowAt(m_ui.in.my - float(by)) : -1;

    const int first = m_plScroll / ROW_H;
    const int off = m_plScroll % ROW_H;

    cv.pushClip({listArea.x, listArea.y + 12, listArea.w, viewH});
    for (int i = first; i < n; ++i) {
        const int y = by + 12 + (i - first) * ROW_H - off;
        if (y > by + listH)
            break;
        const Track &track = m_tracks[size_t(i)];
        const bool isCur = i == m_currentIndex;
        const bool isSel = m_selected[size_t(i)];
        const bool isHover = i == m_plHover;

        if (isCur) {
            cv.fillRect({bx, y, PANEL_W - 8, ROW_H}, rgb(0x26ff50, 26));
            cv.fillRect({bx, y, 2, ROW_H}, rgb(0x3dff5e));
        } else if (isSel) {
            cv.fillRect({bx, y, PANEL_W - 8, ROW_H}, rgb(0x173a72));
        } else if (isHover) {
            cv.fillRect({bx, y, PANEL_W - 8, ROW_H}, rgb(0xc8ffd2, 13));
        }

        const Color textColor = isCur ? rgb(0x57ff74) : (isSel ? rgb(0xe6f0ff) : rgb(0x96b79a));
        const Color dimColor = isCur ? rgb(0x39d957) : (isSel ? rgb(0x9db8e0) : rgb(0x4a6a4c));

        const std::string num = std::to_string(i + 1) + ".";
        pf::draw(cv, num, bx + 22 - pf::measure(num, 1, 1), y + 3, 1, dimColor);

        const std::string title = pf::ellipsize(displayName(track), PANEL_W - 8 - 30 - 46);
        pf::draw(cv, title, bx + 30, y + 3, 1, textColor);

        const std::string dur = track.duration > 0 ? theme::fmt(double(track.duration)) : "--:--";
        pf::draw(cv, dur, bx + PANEL_W - 12 - pf::measure(dur, 1, 1), y + 3, 1, dimColor);

        if (isCur) {
            cv.fillRect({bx + 2, y + 4, 2, 4}, rgb(0x46ff63));
            cv.fillRect({bx + 4, y + 5, 2, 2}, rgb(0x46ff63));
            cv.fillRect({bx + 6, y + 6, 1, 1}, rgb(0x46ff63));
        }
    }
    cv.popClip();

    if (n == 0) {
        const std::string l1 = "PLAYLIST IS EMPTY";
        const std::string l2 = "DROP FILES OR PRESS ADD";
        pf::draw(cv, l1, bx + (PANEL_W - pf::measure(l1, 1, 1)) / 2, by + listH / 2 - 12, 1,
                 rgb(0x1f5c2a));
        pf::draw(cv, l2, bx + (PANEL_W - pf::measure(l2, 1, 1)) / 2, by + listH / 2 + 2, 1,
                 rgb(0x143c1b));
    }

    /* ---- scrollbar ---- */
    if (n * ROW_H > viewH) {
        const int thumbH = std::max(18, int(std::lround(double(viewH) * viewH / (n * ROW_H))));
        const int maxTop = viewH - thumbH;
        const int top = maxScroll > 0 ? int(std::lround(double(m_plScroll) / maxScroll * maxTop)) : 0;
        cv.fillRect({bx + PANEL_W - 8, by + 12, 8, viewH}, rgb(0x030603));
        cv.fillRect({bx + PANEL_W - 8, by + 12, 1, viewH}, rgb(0x0d1c0f));
        cv.fillRect({bx + PANEL_W - 7, by + 12 + top, 7, thumbH}, rgb(0x5b5b5b));
        cv.fillRect({bx + PANEL_W - 7, by + 12 + top, 1, thumbH}, rgb(0x8f8f8f));
        cv.fillRect({bx + PANEL_W - 2, by + 12 + top, 1, thumbH}, rgb(0x333333));
    }

    /* ---- list input ---- */
    if (hoveringList && m_ui.in.wheel != 0.f) {
        m_plScroll -= int(m_ui.in.wheel * 72.f);
        m_plScroll = theme::clampInt(m_plScroll, 0, maxScroll);
    }
    if (hoveringList) {
        if (m_ui.in.rightPressed) {
            const int i = rowAt(m_ui.in.my - float(by));
            if (i >= 0) {
                if (!m_selected[size_t(i)])
                    selectRow(i, false, false);
                openPlaylistContextMenu(i, int(m_ui.in.mx), int(m_ui.in.my));
            }
        } else if (m_ui.in.pressed) {
            const int i = rowAt(m_ui.in.my - float(by));
            if (m_ui.in.doubleClick && i >= 0)
                playIndex(i);
        }
        if (m_ui.in.released && !m_ui.in.doubleClick) {
            const int i = rowAt(m_ui.in.my - float(by));
            if (i >= 0)
                selectRow(i, m_ui.in.shift, m_ui.in.ctrl);
            else
                selectNone();
        }
    }

    /* ---- buttons ---- */
    const int btnY = r.y + r.h - 34;
    ButtonDesc d;
    d.label = "ADD";
    d.tip = "Add files (klik kanan: opsi lain)";
    ButtonResult br = m_ui.button(fnv("pladd"), {bx + 9, btnY, 49, 16}, d);
    if (br.clicked)
        requestFiles();
    if (br.rightClicked)
        openPlaylistButtonMenu(0);

    d = ButtonDesc{};
    d.label = "REM";
    d.tip = "Remove selected";
    if (m_ui.button(fnv("plrem"), {bx + 61, btnY, 49, 16}, d).clicked)
        removeSelected();

    d = ButtonDesc{};
    d.label = "SEL";
    d.tip = "Selection";
    if (m_ui.button(fnv("plsel"), {bx + 113, btnY, 49, 16}, d).clicked)
        openPlaylistButtonMenu(1);

    d = ButtonDesc{};
    d.label = "SORT";
    d.tip = "Sort / shuffle list";
    if (m_ui.button(fnv("plsort"), {bx + 165, btnY, 49, 16}, d).clicked)
        openPlaylistButtonMenu(2);

    d = ButtonDesc{};
    d.label = "CLR";
    d.tip = "Remove all";
    if (m_ui.button(fnv("plclr"), {bx + 217, btnY, 49, 16}, d).clicked)
        openPlaylistButtonMenu(3);

    /* ---- status ---- */
    const Rect status{bx, r.y + r.h - 12, PANEL_W, 12};
    cv.fillRect(status, rgb(0x050805));
    std::string left = std::to_string(n) + (n == 1 ? " TRACK" : " TRACKS");
    if (m_selectionCount > 1)
        left += " / " + std::to_string(m_selectionCount) + " SEL";
    pf::draw(cv, left, bx + 8, status.y + 3, 1, rgb(0x4d7a4d));
    const std::string total = theme::fmt(double(m_totalDuration));
    pf::draw(cv, total, status.right() - 10 - pf::measure(total, 1, 1), status.y + 3, 1,
             rgb(0x4d7a4d));

    /* ---- resize grip ---- */
    for (int i = 3; i < 12; i += 3) {
        cv.fillRect({r.right() - i, r.bottom() - 1, 1, 1}, rgb(0xb4b4b4, 115));
        cv.fillRect({r.right() - 1, r.bottom() - i, 1, 1}, rgb(0xb4b4b4, 115));
    }

    const Rect grip{r.right() - 15, r.bottom() - 15, 14, 14};
    if (m_ui.hovering(grip) && m_ui.in.pressed) {
        m_resizingPlaylist = true;
        m_resizeStartH = m_playlist.height;
        m_resizeStartY = m_ui.in.my;
    }
    if (m_resizingPlaylist) {
        if (m_ui.in.down) {
            const int maxH = std::max(PL_MIN_H, std::min(700, m_winH - 20));
            const int nh = theme::clampInt(m_resizeStartH + int(m_ui.in.my - m_resizeStartY),
                                          PL_MIN_H, maxH);
            m_playlist.height = nh;
        } else {
            m_resizingPlaylist = false;
            scheduleSave();
        }
    }
}

/* ==================================================================
   overlays
   ================================================================== */
void App::drawHelpOverlay()
{
    Canvas &cv = *m_ui.cv;
    cv.fillRect({0, 0, m_winW, m_winH}, rgb(0x020603, 179));

    const Rect box{(m_winW - 440) / 2, (m_winH - 360) / 2, 440, 360};
    cv.fillRect(box, rgb(0x1c1c1c));
    cv.outline(box, rgb(0x000000));
    cv.outline(box.inset(1), rgb(0x6d6d6d));

    const Rect bar{box.x + 1, box.y + 1, box.w - 2, 15};
    cv.vGrad(bar, {{0.f, rgb(0x6d6d6d)}, {1.f, rgb(0x3d3d3d)}});
    pf::draw(cv, "LINAMP", bar.x + 8, bar.y + 4, 1, theme::lcdGreen);

    ButtonDesc d;
    d.icon = "close";
    if (m_ui.button(fnv("helpclose"), {bar.right() - 16 + 1, bar.y + 2, 12, 12}, d).clicked)
        m_helpOpen = false;

    static const char *rows[][2] = {
        {"SPACE", "Play / pause"},
        {"X / C", "Play / pause"},
        {"V", "Stop"},
        {"Z / B", "Previous / next"},
        {"LEFT / RIGHT", "Seek +/-5 detik (Shift: 30)"},
        {"UP / DOWN", "Volume / playlist (kalau fokus di playlist)"},
        {"SHIFT + UP/DOWN", "Perluas seleksi playlist"},
        {"PGUP / PGDN", "Playlist per halaman"},
        {"HOME / END", "Awal / akhir playlist"},
        {"ENTER", "Play track terpilih"},
        {"M", "Mute"},
        {"S / R", "Shuffle / repeat"},
        {"T", "Elapsed / remaining"},
        {"Y", "Bars / oscilloscope"},
        {"E / L", "Equalizer / playlist"},
        {"CTRL+D", "Double size"},
        {"DEL", "Remove track terpilih"},
        {"CTRL+Q / F11", "Quit / fullscreen"},
    };
    int y = box.y + 30;
    for (const auto &row : rows) {
        if (m_uiFont.ok()) {
            m_uiFont.drawIn(cv, row[0], {box.x + 24, y - 3, 110, 14}, 0, 12, theme::lcdGreen);
            m_uiFont.drawIn(cv, row[1], {box.x + 24 + 118, y - 3, box.w - 150, 14}, 0, 12,
                            rgb(0xcfe4d2));
        } else {
            pf::draw(cv, row[0], box.x + 24, y, 1, theme::lcdGreen);
            pf::draw(cv, row[1], box.x + 24 + 118, y, 1, rgb(0xcfe4d2));
        }
        y += 14;
    }

    pf::draw(cv, "DRAG TITLEBAR - DOBEL-KLIK UNTUK SHADE", box.x + 24, box.bottom() - 24, 1,
             rgb(0x5d7a62));

    // click on the dimmed background closes the dialog
    const bool insideDialog = box.contains(int(m_ui.in.mx), int(m_ui.in.my));
    const bool onClose = Rect{bar.right() - 15, bar.y + 2, 12, 12}.contains(int(m_ui.in.mx),
                                                                           int(m_ui.in.my));
    if (m_ui.in.released && !insideDialog)
        m_helpOpen = false;
    if (onClose && m_ui.in.released)
        m_helpOpen = false;
}

void App::drawDropOverlay()
{
    Canvas &cv = *m_ui.cv;
    cv.fillRect({0, 0, m_winW, m_winH}, rgb(0x030905, 179));

    const Rect box{(m_winW - 430) / 2, (m_winH - 150) / 2, 430, 150};
    cv.fillRect(box, rgb(0x07110a));
    cv.outline(box, rgb(0x2f8b3f));
    cv.outline(box.inset(1), rgb(0x2f8b3f));

    const std::string l1 = "DROP AUDIO FILES TO PLAYLIST";
    pf::draw(cv, l1, box.x + (box.w - pf::measure(l1, 2, 1)) / 2, box.y + 44, 2, theme::lcdGreen);
    const std::string l2 = "MP3 / WAV / OGG / FLAC / M4A";
    pf::draw(cv, l2, box.x + (box.w - pf::measure(l2, 1, 1)) / 2, box.y + 90, 1, rgb(0x5d8a63));
}

/* ==================================================================
   controller
   ================================================================== */
void App::bump()
{
    m_totalDuration = 0;
    int sel = 0;
    for (size_t i = 0; i < m_tracks.size(); ++i) {
        m_totalDuration += m_tracks[i].duration;
        if (m_selected[i])
            ++sel;
    }
    m_selectionCount = sel;
    scheduleSave();
}

void App::scheduleSave()
{
    if (m_stateReady)
        m_saveAt = m_now + 0.4;
}

void App::applyZoom()
{
    if (!m_win)
        return;
    SDL_SetWindowMinimumSize(m_win, 580 * m_zoom, 400 * m_zoom);
    SDL_SetWindowSize(m_win, m_winW * m_zoom, m_winH * m_zoom);
    updateRenderMetrics();
}

void App::toggleZoom()
{
    m_zoom = m_zoom == 1 ? 2 : 1;
    applyZoom();
    scheduleSave();
}

void App::toggleVis()
{
    m_visMode = m_visMode == "bars" ? "osc" : "bars";
    scheduleSave();
}

void App::toggleMute()
{
    if (m_engine.volume() > 0.001) {
        m_savedVolume = m_engine.volume();
        m_engine.setVolume(0.0);
    } else {
        m_engine.setVolume(m_savedVolume > 0.05 ? m_savedVolume : 0.82);
    }
    scheduleSave();
}

std::string App::displayName(const Track &t) const
{
    std::string artist = t.artist;
    std::string title = t.title.empty() ? t.name : t.title;
    while (!artist.empty() && std::isspace((unsigned char)artist.front()))
        artist.erase(artist.begin());
    while (!artist.empty() && std::isspace((unsigned char)artist.back()))
        artist.pop_back();
    return artist.empty() ? title : artist + " - " + title;
}

void App::updateMarquee()
{
    if (m_currentIndex >= 0 && m_currentIndex < int(m_tracks.size()))
        m_marqueeText = displayName(m_tracks[size_t(m_currentIndex)]);
    else
        m_marqueeText = "LINAMP 1.0  -  DROP FILES HERE";
}

void App::ensureTrackVisible(int index)
{
    if (index < 0)
        return;
    const Rect r = panelRect(PANEL_PLAYLIST);
    const int listH = (m_playlist.shaded ? TITLE_H : m_playlist.height) - TITLE_H - 34;
    const int viewH = std::max(1, listH - 12);
    const int top = index * ROW_H;
    const int bottom = top + ROW_H;
    if (top < m_plScroll)
        m_plScroll = top;
    else if (bottom > m_plScroll + viewH)
        m_plScroll = bottom - viewH;
    const int maxScroll = std::max(0, int(m_tracks.size()) * ROW_H - viewH);
    m_plScroll = theme::clampInt(m_plScroll, 0, maxScroll);
    (void)r;
}

int App::focusedPanel() const
{
    const int under = topPanelAt(m_ui.in.mx, m_ui.in.my);
    if (under >= 0)
        return under;
    if (m_focusPanel == PANEL_EQ && !m_eqOpen)
        return PANEL_MAIN;
    if (m_focusPanel == PANEL_PLAYLIST && !m_playlistOpen)
        return PANEL_MAIN;
    return m_focusPanel;
}

/* Moves the playlist selection by `delta` rows (keeps it visible). */
void App::playlistMove(int delta, bool extend)
{
    const int n = int(m_tracks.size());
    if (n == 0)
        return;

    int base = m_anchor;
    if (base < 0 || base >= n) {
        // start from the first selected row, else the playing track, else the edge
        base = -1;
        for (int i = 0; i < n; ++i) {
            if (m_selected[size_t(i)]) {
                base = i;
                break;
            }
        }
        if (base < 0)
            base = m_currentIndex >= 0 && m_currentIndex < n ? m_currentIndex : (delta > 0 ? -1 : n);
    }

    const int target = theme::clampInt(base + delta, 0, n - 1);
    selectRow(target, extend, false);
    ensureTrackVisible(target);
}

void App::playIndex(int i)
{
    if (i < 0 || i >= int(m_tracks.size()))
        return;
    m_currentIndex = i;
    ensureTrackVisible(i);
    m_engine.open(m_tracks[size_t(i)].url);
    m_engine.play();
    updateMarquee();
    bump();
}

void App::togglePlay()
{
    if (m_engine.playing()) {
        m_engine.pause();
        return;
    }
    if (!m_engine.source().empty() && m_engine.duration() > 0 &&
        m_engine.position() < m_engine.duration() - 200) {
        m_engine.play();
        return;
    }
    if (m_currentIndex >= 0)
        playIndex(m_currentIndex);
    else if (!m_tracks.empty())
        playIndex(0);
}

void App::stopPlayback()
{
    m_engine.stop();
}

void App::next(bool autoNext)
{
    const int n = int(m_tracks.size());
    if (n == 0)
        return;
    if (m_shuffleOn && n > 1) {
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(0, n - 1);
        int r = dist(rng);
        while (r == m_currentIndex)
            r = dist(rng);
        playIndex(r);
        return;
    }
    if (m_currentIndex + 1 < n)
        playIndex(m_currentIndex + 1);
    else if (m_repeatOn)
        playIndex(0);
    else if (!autoNext)
        playIndex(0);
    else
        m_engine.stop();
}

void App::prev()
{
    if (m_tracks.empty())
        return;
    if (m_engine.position() > 3000) {
        m_engine.seek(0);
        return;
    }
    playIndex(m_currentIndex > 0 ? m_currentIndex - 1 : int(m_tracks.size()) - 1);
}

void App::selectRow(int i, bool shift, bool ctrl)
{
    const int n = int(m_tracks.size());
    if (i < 0 || i >= n)
        return;
    m_focusPanel = PANEL_PLAYLIST;
    if (shift && m_anchor >= 0) {
        std::fill(m_selected.begin(), m_selected.end(), false);
        const int a = std::min(m_anchor, i);
        const int b = std::max(m_anchor, i);
        for (int k = a; k <= b; ++k)
            m_selected[size_t(k)] = true;
    } else if (ctrl) {
        m_selected[size_t(i)] = !m_selected[size_t(i)];
        m_anchor = i;
    } else {
        std::fill(m_selected.begin(), m_selected.end(), false);
        m_selected[size_t(i)] = true;
        m_anchor = i;
    }
    bump();
}

void App::selectAll()
{
    std::fill(m_selected.begin(), m_selected.end(), true);
    bump();
}

void App::selectNone()
{
    std::fill(m_selected.begin(), m_selected.end(), false);
    m_anchor = -1;
    bump();
}

void App::invertSelection()
{
    for (size_t i = 0; i < m_selected.size(); ++i)
        m_selected[i] = !m_selected[i];
    bump();
}

void App::removeSelected()
{
    std::vector<int> indices;
    for (int i = 0; i < int(m_tracks.size()); ++i)
        if (m_selected[size_t(i)])
            indices.push_back(i);
    if (indices.empty())
        return;

    int before = 0;
    bool removedCurrent = false;
    for (int idx : indices) {
        if (idx < m_currentIndex)
            ++before;
        if (idx == m_currentIndex)
            removedCurrent = true;
    }
    for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
        m_tracks.erase(m_tracks.begin() + *it);
        m_selected.erase(m_selected.begin() + *it);
    }
    if (removedCurrent) {
        m_engine.stop();
        m_currentIndex = -1;
        m_selected.assign(m_tracks.size(), false);
    } else if (m_currentIndex >= 0) {
        m_currentIndex -= before;
    }
    m_anchor = -1;
    updateMarquee();
    bump();
}

void App::clearPlaylist()
{
    m_engine.stop();
    m_tracks.clear();
    m_selected.clear();
    m_currentIndex = -1;
    m_anchor = -1;
    updateMarquee();
    bump();
}

void App::sortBy(const std::string &mode)
{
    const std::string curUrl =
        (m_currentIndex >= 0 && m_currentIndex < int(m_tracks.size()))
            ? m_tracks[size_t(m_currentIndex)].url
            : std::string();

    if (mode == "shuffle") {
        static std::mt19937 rng{std::random_device{}()};
        std::shuffle(m_tracks.begin(), m_tracks.end(), rng);
    } else if (mode == "reverse") {
        std::reverse(m_tracks.begin(), m_tracks.end());
    } else if (mode == "duration") {
        std::stable_sort(m_tracks.begin(), m_tracks.end(), [](const Track &a, const Track &b) {
            const int64_t da = a.duration > 0 ? a.duration : INT64_MAX;
            const int64_t db = b.duration > 0 ? b.duration : INT64_MAX;
            return da < db;
        });
    } else {
        std::stable_sort(m_tracks.begin(), m_tracks.end(), [this](const Track &a, const Track &b) {
            return util::naturalCompare(displayName(a), displayName(b)) < 0;
        });
    }

    m_currentIndex = -1;
    if (!curUrl.empty()) {
        for (size_t i = 0; i < m_tracks.size(); ++i) {
            if (m_tracks[i].url == curUrl) {
                m_currentIndex = int(i);
                break;
            }
        }
    }
    m_selected.assign(m_tracks.size(), false);
    m_anchor = -1;
    updateMarquee();
    m_lastSort = mode;
    bump();
}

void App::applyPreset(const char *name, const std::array<double, EQ_BANDS> &bands, double preamp)
{
    m_eqPreset = name;
    m_eqReadout.clear();
    m_eqGains = bands;
    for (int i = 0; i < EQ_BANDS; ++i)
        m_engine.setBand(i, bands[size_t(i)]);
    m_engine.setPreamp(preamp);
    scheduleSave();
}

void App::addDemos()
{
    const std::string dir = util::cacheDir() + "/demos";
    util::ensureDir(dir);
    struct Demo {
        int variant;
        const char *artist;
        const char *title;
    };
    const Demo demos[2] = {{1, "LINAMP", "DEMO LOOP 1"}, {2, "LINAMP", "DEMO LOOP 2"}};

    for (const Demo &demo : demos) {
        const std::string path =
            dir + "/LINAMP - DEMO LOOP " + std::to_string(demo.variant) + ".wav";
        if (!util::fileExists(path)) {
            const std::vector<float> pcm = synthDemo(demo.variant, 44100);
            writeWav(path, pcm, 44100);
        }
        const std::string url = "file://" + util::percentEncodePath(path);

        bool exists = false;
        for (const Track &t : m_tracks)
            if (t.url == url)
                exists = true;
        if (exists)
            continue;

        Track t;
        t.url = url;
        t.name = std::string(demo.artist) + " - " + demo.title;
        t.title = demo.title;
        t.artist = demo.artist;
        t.duration = 0;
        m_tracks.push_back(t);
        m_selected.push_back(false);
        m_prober.probe(url);
    }
    if (m_currentIndex < 0 && !m_tracks.empty())
        m_currentIndex = 0;
    updateMarquee();
    bump();
}

void App::addUrls(const std::vector<std::string> &urls, bool autoplay)
{
    int first = -1;
    int added = 0;
    for (const std::string &raw : urls) {
        if (!util::hasAudioExtension(raw))
            continue;
        std::string url = raw;
        if (!util::isFileUrl(url) && url.find("://") == std::string::npos)
            url = "file://" + util::percentEncodePath(url);

        bool exists = false;
        for (const Track &t : m_tracks)
            if (t.url == url)
                exists = true;
        if (exists)
            continue;

        std::string base = util::completeBaseName(url);
        std::replace(base.begin(), base.end(), '_', ' ');
        while (!base.empty() && std::isspace((unsigned char)base.back()))
            base.pop_back();

        Track t;
        t.url = url;
        t.name = base;
        t.title = base;
        t.artist.clear();
        const size_t sep = base.find(" - ");
        if (sep != std::string::npos && sep > 0) {
            t.artist = base.substr(0, sep);
            t.title = base.substr(sep + 3);
        }
        m_tracks.push_back(t);
        m_selected.push_back(false);
        m_prober.probe(url);
        if (first < 0)
            first = int(m_tracks.size()) - 1;
        ++added;
    }
    if (added > 0) {
        if (m_currentIndex < 0)
            m_currentIndex = first;
        updateMarquee();
        bump();
        if (autoplay && !m_engine.playing())
            playIndex(first);
    }
}

void App::requestFiles()
{
    static const SDL_DialogFileFilter filters[] = {
        {"Audio", "mp3;wav;ogg;oga;flac;m4a;aac;opus;wma;aiff;aif"},
        {"All files", "*"},
    };
    SDL_ShowOpenFileDialog(
        [](void *userdata, const char *const *filelist, int) {
            auto *app = static_cast<App *>(userdata);
            std::vector<std::string> files;
            if (filelist)
                for (int i = 0; filelist[i]; ++i)
                    files.emplace_back(filelist[i]);
            app->pushDialogFiles(std::move(files));
        },
        this, m_win, filters, 2, nullptr, true);
}

void App::requestFolder()
{
    SDL_ShowOpenFolderDialog(
        [](void *userdata, const char *const *filelist, int) {
            auto *app = static_cast<App *>(userdata);
            std::vector<std::string> folders;
            if (filelist)
                for (int i = 0; filelist[i]; ++i)
                    folders.emplace_back(filelist[i]);
            app->pushDialogFolder(std::move(folders));
        },
        this, m_win, nullptr, false);
}

void App::pushDialogFiles(std::vector<std::string> files)
{
    std::lock_guard<std::mutex> lock(m_dialogMutex);
    for (auto &f : files)
        m_dialogFiles.push_back(std::move(f));
}

void App::pushDialogFolder(std::vector<std::string> folders)
{
    std::lock_guard<std::mutex> lock(m_dialogMutex);
    for (auto &f : folders)
        m_dialogFolders.push_back(std::move(f));
}

void App::handleDialogResults()
{
    std::vector<std::string> files, folders;
    {
        std::lock_guard<std::mutex> lock(m_dialogMutex);
        files.swap(m_dialogFiles);
        folders.swap(m_dialogFolders);
    }
    if (!files.empty())
        addUrls(files, true);
    for (const std::string &dir : folders) {
        const std::string local = util::isFileUrl(dir) ? util::urlToPath(dir) : dir;
        addUrls(util::audioFilesInFolder(local), true);
    }
}

/* ==================================================================
   menus
   ================================================================== */
void App::openMainMenu()
{
    std::vector<MenuItem> items;
    items.push_back({m_engine.playing() ? "Pause" : "Play", "SPACE", false, false, false,
                     [this] { togglePlay(); }});
    items.push_back({"Stop", "V", false, false, false, [this] { stopPlayback(); }});
    items.push_back({"Previous", "Z", false, false, false, [this] { prev(); }});
    items.push_back({"Next", "B", false, false, false, [this] { next(false); }});
    items.push_back({"", "", true});
    items.push_back({"Add files...", "L", false, false, false, [this] { requestFiles(); }});
    items.push_back({"Add folder...", "", false, false, false, [this] { requestFolder(); }});
    items.push_back({"", "", true});
    items.push_back({"Equalizer", "E", false, m_eqOpen, false, [this] { toggleEq(); }});
    items.push_back({"Playlist", "L", false, m_playlistOpen, false, [this] { togglePlaylist(); }});
    items.push_back({"Elapsed / Remaining", "T", false, m_remaining, false,
                     [this] { toggleRemaining(); }});
    items.push_back({"Visualization: " + std::string(m_visMode == "bars" ? "Bars" : "Oscilloscope"),
                     "Y", false, false, false, [this] { toggleVis(); }});
    items.push_back({"Double size", "CTRL+D", false, m_zoom == 2, false,
                     [this] { toggleZoom(); }});
    items.push_back({"", "", true});
    items.push_back({"Load demo tracks", "", false, false, false, [this] { addDemos(); }});
    items.push_back({"Keyboard shortcuts...", "F1", false, false, false, [this] { showHelp(); }});
    items.push_back({"", "", true});
    items.push_back({"Quit Linamp", "CTRL+Q", false, false, false, [this] { m_running = false; }});

    const Rect r = panelRect(PANEL_MAIN);
    m_menu.show(r.x + 4, r.y + 18, 222, 1, std::move(items));
}

void App::openPlaylistButtonMenu(int which)
{
    const Rect r = panelRect(PANEL_PLAYLIST);
    std::vector<MenuItem> items;
    int x = r.x;
    switch (which) {
    case 0:
        items.push_back({"Add files...", "", false, false, false, [this] { requestFiles(); }});
        items.push_back({"Add folder...", "", false, false, false, [this] { requestFolder(); }});
        x = r.x + 9;
        break;
    case 1:
        items.push_back({"Select all", "CTRL+A", false, false, false, [this] { selectAll(); }});
        items.push_back({"Select none", "", false, false, false, [this] { selectNone(); }});
        items.push_back({"Invert selection", "", false, false, false,
                         [this] { invertSelection(); }});
        x = r.x + 113;
        break;
    case 2:
        items.push_back({"Sort by title", "", false, m_lastSort == "title", false,
                         [this] { sortBy("title"); }});
        items.push_back({"Sort by duration", "", false, m_lastSort == "duration", false,
                         [this] { sortBy("duration"); }});
        items.push_back({"Reverse order", "", false, m_lastSort == "reverse", false,
                         [this] { sortBy("reverse"); }});
        items.push_back({"", "", true});
        items.push_back({"Shuffle list", "", false, m_lastSort == "shuffle", false,
                         [this] { sortBy("shuffle"); }});
        x = r.x + 165;
        break;
    default:
        items.push_back({"Remove all tracks", "", false, false, false, [this] { clearPlaylist(); }});
        items.push_back({"Remove selected", "", false, false, false, [this] { removeSelected(); }});
        x = r.x + 217;
        break;
    }
    m_menu.show(x, r.y + r.h - 13, which == 1 || which == 2 ? 200 : 200, 1, std::move(items));
}

void App::openPlaylistContextMenu(int row, int mx, int my)
{
    (void)row;
    std::vector<MenuItem> items;
    items.push_back({"Play", "", false, false, false, [this, row] { playIndex(row); }});
    items.push_back({"Remove selected", "", false, false, false, [this] { removeSelected(); }});
    items.push_back({"", "", true});
    items.push_back({"Remove all tracks", "", false, false, false, [this] { clearPlaylist(); }});
    m_menu.show(mx, my + 4, 190, 1, std::move(items));
}

void App::openPresetMenu()
{
    std::vector<MenuItem> items;
    for (int i = 0; i < PRESET_COUNT; ++i) {
        const Preset &p = PRESETS[i];
        const std::string name = p.name;
        std::array<double, EQ_BANDS> bands = p.bands;
        items.push_back({name, "", false, name == m_eqPreset, false,
                         [this, name, bands] { applyPreset(name.c_str(), bands, 0.0); }});
    }
    items.push_back({"RESET TO FLAT", "", false, false, false,
                     [this] {
                         const std::array<double, EQ_BANDS> flat = PRESETS[0].bands;
                         applyPreset("FLAT", flat, 0.0);
                     }});
    const Rect r = panelRect(PANEL_EQ);
    m_menu.show(r.x + 85, r.y + 10, 330, 2, std::move(items));
}

/* ==================================================================
   persistence
   ================================================================== */
void App::saveStateNow()
{
    if (!m_stateReady)
        return;
    m_saveAt = -1;

    Json root = Json::makeObject();
    root.set("version", Json::makeNumber(2));
    root.set("currentIndex", Json::makeNumber(m_currentIndex));
    root.set("lastSort", Json::makeString(m_lastSort));
    root.set("visMode", Json::makeString(m_visMode));
    root.set("remaining", Json::makeBool(m_remaining));
    root.set("shuffle", Json::makeBool(m_shuffleOn));
    root.set("repeat", Json::makeBool(m_repeatOn));
    root.set("eqOpen", Json::makeBool(m_eqOpen));
    root.set("playlistOpen", Json::makeBool(m_playlistOpen));
    root.set("zoom", Json::makeNumber(m_zoom));

    Json layout = Json::makeObject();
    auto panelJson = [](int x, int y, bool shaded, int height) {
        Json p = Json::makeObject();
        p.set("x", Json::makeNumber(x));
        p.set("y", Json::makeNumber(y));
        p.set("shaded", Json::makeBool(shaded));
        p.set("height", Json::makeNumber(height));
        return p;
    };
    layout.set("main", panelJson(m_main.x, m_main.y, m_main.shaded, MAIN_H));
    layout.set("eq", panelJson(m_eq.x, m_eq.y, m_eq.shaded, EQ_H));
    layout.set("playlist", panelJson(m_playlist.x, m_playlist.y, m_playlist.shaded,
                                     m_playlist.height));
    layout.set("front", Json::makeString(m_front));
    root.set("layout", std::move(layout));

    Json tracks = Json::makeArray();
    for (const Track &t : m_tracks) {
        Json item = Json::makeObject();
        item.set("url", Json::makeString(t.url));
        item.set("name", Json::makeString(t.name));
        item.set("title", Json::makeString(t.title));
        item.set("artist", Json::makeString(t.artist));
        item.set("duration", Json::makeNumber(double(t.duration)));
        tracks.array.push_back(std::move(item));
    }
    root.set("tracks", std::move(tracks));

    Json audio = Json::makeObject();
    audio.set("volume", Json::makeNumber(m_engine.volume()));
    audio.set("balance", Json::makeNumber(m_engine.balance()));
    audio.set("eqOn", Json::makeBool(m_engine.eqOn()));
    audio.set("eqAuto", Json::makeBool(m_engine.eqAuto()));
    audio.set("preamp", Json::makeNumber(m_engine.preamp()));
    audio.set("preset", Json::makeString(m_eqPreset));
    Json bands = Json::makeArray();
    for (int i = 0; i < EQ_BANDS; ++i)
        bands.array.push_back(Json::makeNumber(m_eqGains[size_t(i)]));
    audio.set("bands", std::move(bands));
    root.set("audio", std::move(audio));

    util::writeFile(statePath(), root.dump(1));
}

void App::applyLayout(const Json &layout)
{
    const Json &main = layout.at("main");
    if (main.isObject()) {
        m_main.x = int(main.num("x", m_main.x));
        m_main.y = int(main.num("y", m_main.y));
        m_main.shaded = main.flag("shaded", m_main.shaded);
    }
    const Json &eq = layout.at("eq");
    if (eq.isObject()) {
        m_eq.x = int(eq.num("x", m_eq.x));
        m_eq.y = int(eq.num("y", m_eq.y));
        m_eq.shaded = eq.flag("shaded", m_eq.shaded);
    }
    const Json &pl = layout.at("playlist");
    if (pl.isObject()) {
        m_playlist.x = int(pl.num("x", m_playlist.x));
        m_playlist.y = int(pl.num("y", m_playlist.y));
        m_playlist.shaded = pl.flag("shaded", m_playlist.shaded);
        const double h = pl.num("height", m_playlist.height);
        if (h > 0) {
            const int maxH = std::max(PL_MIN_H, std::min(700, m_winH - 20));
            m_playlist.height = theme::clampInt(int(h), PL_MIN_H, maxH);
        }
    }
    const std::string front = layout.str("front", m_front);
    if (front == "mainPanel") {
        raisePanel(PANEL_MAIN);
    } else if (front == "eqPanel" && m_eqOpen) {
        raisePanel(PANEL_EQ);
    } else if (front == "playlistPanel" && m_playlistOpen) {
        raisePanel(PANEL_PLAYLIST);
    }
}

void App::restoreState()
{
    const std::string raw = util::readFile(statePath());
    bool ok = false;
    Json root = Json::parse(raw, &ok);
    if (ok && root.isObject()) {
        m_currentIndex = int(root.num("currentIndex", -1));
        m_lastSort = root.str("lastSort", "");
        m_visMode = root.str("visMode", "bars") == "osc" ? "osc" : "bars";
        m_remaining = root.flag("remaining", false);
        m_shuffleOn = root.flag("shuffle", false);
        m_repeatOn = root.flag("repeat", false);
        m_eqOpen = root.flag("eqOpen", false);
        m_playlistOpen = root.flag("playlistOpen", true);
        m_zoom = root.num("zoom", 1) >= 2 ? 2 : 1;

        m_eqPreset = root.at("audio").str("preset", "FLAT");
        double vol = root.at("audio").num("volume", 0.82);
        double bal = root.at("audio").num("balance", 0.0);
        bool eqOn = root.at("audio").flag("eqOn", true);
        bool eqAuto = root.at("audio").flag("eqAuto", true);
        double preamp = root.at("audio").num("preamp", 0.0);
        std::array<double, EQ_BANDS> bands{};
        const Json &bandArr = root.at("audio").at("bands");
        for (int i = 0; i < EQ_BANDS; ++i) {
            if (int(bandArr.array.size()) > i && bandArr.array[size_t(i)].type == Json::Type::Number)
                bands[size_t(i)] = bandArr.array[size_t(i)].number;
        }
        m_engine.loadSettings(vol, bal, eqOn, eqAuto, preamp, bands);
        m_eqGains = bands;
        m_savedVolume = vol > 0.05 ? vol : 0.82;

        const Json &tracks = root.at("tracks");
        if (tracks.isArray()) {
            for (const Json &item : tracks.array) {
                if (!item.isObject())
                    continue;
                Track t;
                t.url = item.str("url");
                if (t.url.empty())
                    continue;
                t.name = item.str("name", "UNTITLED");
                t.title = item.str("title", t.name);
                t.artist = item.str("artist", "");
                t.duration = int64_t(item.num("duration", 0));
                m_tracks.push_back(t);
                m_selected.push_back(false);
                if (t.duration <= 0)
                    m_prober.probe(t.url);
            }
        }

        if (m_currentIndex < 0 || m_currentIndex >= int(m_tracks.size()))
            m_currentIndex = m_tracks.empty() ? -1 : 0;

        const Json &layout = root.at("layout");
        if (layout.isObject()) {
            m_hadLayout = true;
            applyLayout(layout);
        }
        updateMarquee();
        return;
    }

    addDemos();
    if (m_currentIndex < 0 && !m_tracks.empty())
        m_currentIndex = 0;
    updateMarquee();
}

/* ==================================================================
   title bar dragging
   ================================================================== */
void App::handleTitlebarDrag()
{
    if (m_dragPanel >= 0) {
        if (m_ui.in.down) {
            int nx = int(m_ui.in.mx) - m_dragOffX;
            int ny = int(m_ui.in.my) - m_dragOffY;
            m_dragMoved = true;
            snapPanel(m_dragPanel, nx, ny);
            if (m_dragPanel == PANEL_MAIN) {
                m_main.x = nx;
                m_main.y = ny;
            } else if (m_dragPanel == PANEL_EQ) {
                m_eq.x = nx;
                m_eq.y = ny;
            } else {
                m_playlist.x = nx;
                m_playlist.y = ny;
            }
        } else {
            m_dragPanel = -1;
            m_ui.active = 0;
            if (m_dragMoved)
                scheduleSave();
            m_dragMoved = false;
        }
    }

    if (!m_ui.in.pressed || m_dragPanel >= 0 || m_ui.blockPanels)
        return;

    const int p = topPanelAt(m_ui.in.mx, m_ui.in.my);
    if (p < 0)
        return;

    const Rect r = panelRect(p);
    const Rect bar{r.x, r.y, r.w, TITLE_H};
    const Rect buttons{r.right() - 41, r.y + 1, 38, 12};
    const Rect logo{r.x + 3, r.y + 1, 14, 12};
    const int mx = int(m_ui.in.mx), my = int(m_ui.in.my);

    if (!bar.contains(mx, my) || buttons.contains(mx, my) || logo.contains(mx, my))
        return;

    if (m_ui.in.doubleClick) {
        bool &sh = p == PANEL_MAIN ? m_main.shaded : (p == PANEL_EQ ? m_eq.shaded : m_playlist.shaded);
        sh = !sh;
        scheduleSave();
        return;
    }

    m_dragPanel = p;
    m_dragOffX = mx - r.x;
    m_dragOffY = my - r.y;
    m_dragMoved = false;
    m_ui.active = fnv("titledrag");
    raisePanel(p);
}

/* ==================================================================
   keyboard
   ================================================================== */
void App::handleKeyboard(const SDL_KeyboardEvent &e)
{
    const SDL_Keycode key = e.key;
    const SDL_Keymod mod = e.mod;

    if (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) {
        if (key == SDLK_Q) {
            m_running = false;
            return;
        }
        if (key == SDLK_M) {
            SDL_MinimizeWindow(m_win);
            return;
        }
        if (key == SDLK_A) {
            selectAll();
            return;
        }
        if (key == SDLK_D) {
            toggleZoom();
            return;
        }
    }

    switch (key) {
    case SDLK_F11: {
        const bool fs = (SDL_GetWindowFlags(m_win) & SDL_WINDOW_FULLSCREEN) != 0;
        SDL_SetWindowFullscreen(m_win, !fs);
        return;
    }
    case SDLK_F1:
        m_helpOpen = !m_helpOpen;
        return;
    case SDLK_ESCAPE:
        m_helpOpen = false;
        m_menu.close();
        return;
    default:
        break;
    }

    if (m_helpOpen)
        return;

    switch (key) {
    case SDLK_SPACE:
    case SDLK_X:
        togglePlay();
        break;
    case SDLK_C:
        m_engine.pause();
        break;
    case SDLK_V:
        stopPlayback();
        break;
    case SDLK_B:
        next(false);
        break;
    case SDLK_Z:
        prev();
        break;
    case SDLK_T:
        toggleRemaining();
        break;
    case SDLK_Y:
        toggleVis();
        break;
    case SDLK_E:
        toggleEq();
        break;
    case SDLK_L:
        togglePlaylist();
        break;
    case SDLK_S:
        toggleShuffle();
        break;
    case SDLK_R:
        toggleRepeat();
        break;
    case SDLK_M:
        toggleMute();
        break;
    case SDLK_DELETE:
        removeSelected();
        break;
    case SDLK_LEFT:
        m_engine.seek(m_engine.position() - ((mod & SDL_KMOD_SHIFT) ? 30000 : 5000));
        break;
    case SDLK_RIGHT:
        m_engine.seek(m_engine.position() + ((mod & SDL_KMOD_SHIFT) ? 30000 : 5000));
        break;
    case SDLK_UP:
        if (focusedPanel() == PANEL_PLAYLIST)
            playlistMove(-1, (mod & SDL_KMOD_SHIFT) != 0);
        else {
            m_engine.setVolume(theme::clamp(m_engine.volume() + 0.05, 0.0, 1.0));
            scheduleSave();
        }
        break;
    case SDLK_DOWN:
        if (focusedPanel() == PANEL_PLAYLIST)
            playlistMove(1, (mod & SDL_KMOD_SHIFT) != 0);
        else {
            m_engine.setVolume(theme::clamp(m_engine.volume() - 0.05, 0.0, 1.0));
            scheduleSave();
        }
        break;
    case SDLK_PAGEUP:
    case SDLK_PAGEDOWN: {
        if (focusedPanel() != PANEL_PLAYLIST)
            break;
        const int listH = (m_playlist.shaded ? TITLE_H : m_playlist.height) - TITLE_H - 34;
        const int viewRows = std::max(1, (listH - 12) / ROW_H);
        playlistMove(key == SDLK_PAGEUP ? -viewRows : viewRows, (mod & SDL_KMOD_SHIFT) != 0);
        break;
    }
    case SDLK_HOME:
    case SDLK_END: {
        if (focusedPanel() != PANEL_PLAYLIST)
            break;
        const int n = int(m_tracks.size());
        if (n == 0)
            break;
        const int target = key == SDLK_HOME ? 0 : n - 1;
        const bool extend = (mod & SDL_KMOD_SHIFT) != 0;
        selectRow(target, extend, false);
        ensureTrackVisible(target);
        break;
    }
    case SDLK_RETURN:
    case SDLK_KP_ENTER: {
        if (focusedPanel() != PANEL_PLAYLIST)
            break;
        if (m_anchor >= 0)
            playIndex(m_anchor);
        else if (m_currentIndex >= 0)
            playIndex(m_currentIndex);
        break;
    }
    default:
        break;
    }
}

/* ==================================================================
   SDL events
   ================================================================== */
void App::handleEvent(const SDL_Event &e)
{
    switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_KEY_DOWN:
        m_inputDirty = true;
        break;
    default:
        break;
    }

    switch (e.type) {
    case SDL_EVENT_MOUSE_MOTION:
        m_ui.in.mx = e.motion.x / float(std::max(1, m_zoom));
        m_ui.in.my = e.motion.y / float(std::max(1, m_zoom));
        m_ui.in.mouseInside = true;
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        const SDL_Keymod mods = SDL_GetModState();
        m_ui.in.shift = (mods & SDL_KMOD_SHIFT) != 0;
        m_ui.in.ctrl = (mods & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
        const float lx = e.button.x / float(std::max(1, m_zoom));
        const float ly = e.button.y / float(std::max(1, m_zoom));
        if (const int panel = topPanelAt(lx, ly); panel >= 0)
            m_focusPanel = panel;
        if (e.button.button == SDL_BUTTON_LEFT) {
            m_ui.in.mx = lx;
            m_ui.in.my = ly;
            m_ui.in.down = true;
            m_ui.in.pressed = true;
            const double now = double(SDL_GetTicks()) / 1000.0;
            const float dx = lx - m_lastClickX;
            const float dy = ly - m_lastClickY;
            if (now - m_lastClickTime < 0.42 && dx * dx + dy * dy < 36.f)
                m_ui.in.doubleClick = true;
            m_lastClickTime = now;
            m_lastClickX = lx;
            m_lastClickY = ly;
        } else if (e.button.button == SDL_BUTTON_RIGHT) {
            m_ui.in.mx = lx;
            m_ui.in.my = ly;
            m_ui.in.rightPressed = true;
        }
        break;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT) {
            m_ui.in.mx = e.button.x / float(std::max(1, m_zoom));
            m_ui.in.my = e.button.y / float(std::max(1, m_zoom));
            m_ui.in.down = false;
            m_ui.in.released = true;
        } else if (e.button.button == SDL_BUTTON_RIGHT) {
            m_ui.in.rightReleased = true;
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL:
        m_ui.in.wheel += e.wheel.y;
        break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        m_ui.in.mouseInside = false;
        break;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
        const int z = std::max(1, m_zoom);
        int ww = m_winW, wh = m_winH;
        SDL_GetWindowSize(m_win, &ww, &wh);
        m_winW = std::max(580, ww / z);
        m_winH = std::max(400, wh / z);
        updateRenderMetrics();
        clampPanels();
        break;
    }
    case SDL_EVENT_KEY_DOWN:
        if (!e.key.repeat)
            handleKeyboard(e.key);
        break;
    case SDL_EVENT_DROP_BEGIN:
        m_dragInfo = "DROP AUDIO FILES TO PLAYLIST";
        break;
    case SDL_EVENT_DROP_FILE: {
        m_dragInfo.clear();
        if (e.drop.data) {
            std::vector<std::string> urls{e.drop.data};
            addUrls(urls, true);
        }
        break;
    }
    case SDL_EVENT_DROP_COMPLETE:
    case SDL_EVENT_DROP_POSITION:
        if (e.type != SDL_EVENT_DROP_POSITION)
            m_dragInfo.clear();
        break;
    case SDL_EVENT_QUIT:
        m_running = false;
        break;
    default:
        break;
    }
}
