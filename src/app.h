/* ------------------------------------------------------------------
   Linamp — application state, controller and panel rendering.
   This is the SDL3 port of qml/Main.qml + the QML panels.
   ------------------------------------------------------------------ */
#pragma once

#include "engine.h"
#include "json.h"
#include "prober.h"
#include "text.h"
#include "ui.h"

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct Track {
    std::string url;
    std::string name;
    std::string title;
    std::string artist;
    int64_t duration = 0;
};

struct PanelBox {
    int x = 0;
    int y = 0;
    bool shaded = false;
    int height = 238; // playlist total height
};

enum PanelId { PANEL_MAIN = 0, PANEL_PLAYLIST = 1, PANEL_EQ = 2 };

class App {
public:
    bool init();
    void shutdown();
    void frame();
    bool running() const { return m_running; }
    bool wantsFastFrames() const;

    void handleEvent(const SDL_Event &e);
    SDL_HitTestResult hitTest(int x, int y) const;

    SDL_Window *window() const { return m_win; }
    /* used by the async file-dialog callbacks */
    void pushDialogFiles(std::vector<std::string> files);
    void pushDialogFolder(std::vector<std::string> folders);

    /* dev helpers: dump the current frame / inspect state (LINAMP_SCREENSHOT) */
    bool saveScreenshot(const std::string &path);
    void setPlaylistForTest(const std::string &scene = "");
    std::string debugState() const;
    void debugPlayFirst() { playIndex(0); }

private:
    /* ---- panels ---- */
    void drawMainPanel();
    void drawPlaylistPanel();
    void drawEqPanel();
    void drawFrame();
    void drawHelpOverlay();
    void drawDropOverlay();
    void drawPanelFrame(int panel, const std::string &title, const std::string &version,
                        bool showLogo);
    Rect panelRect(int panel) const;
    int topPanelAt(float x, float y) const;
    void raisePanel(int panel);

    /* ---- controller (ported from Main.qml) ---- */
    void layoutPanels();
    void clampPanels();
    void snapPanel(int panel, int &nx, int &ny);
    void bump();
    void scheduleSave();
    void saveStateNow();
    void restoreState();
    void applyLayout(const Json &layout);
    void addDemos();
    void addUrls(const std::vector<std::string> &urls, bool autoplay);
    void playIndex(int i);
    std::string displayName(const Track &t) const;
    void updateMarquee();
    void ensureTrackVisible(int index);
    /* keyboard navigation in the playlist (arrows / page keys / home / end) */
    int focusedPanel() const;
    void playlistMove(int delta, bool extend);
    void togglePlay();
    void stopPlayback();
    void next(bool autoNext);
    void prev();
    void selectRow(int i, bool shift, bool ctrl);
    void selectAll();
    void selectNone();
    void invertSelection();
    void removeSelected();
    void clearPlaylist();
    void sortBy(const std::string &mode);
    void toggleEq() { m_eqOpen = !m_eqOpen; scheduleSave(); }
    void togglePlaylist() { m_playlistOpen = !m_playlistOpen; scheduleSave(); }
    void toggleShuffle() { m_shuffleOn = !m_shuffleOn; scheduleSave(); }
    void toggleRepeat() { m_repeatOn = !m_repeatOn; scheduleSave(); }
    void toggleRemaining() { m_remaining = !m_remaining; scheduleSave(); }
    void toggleVis();
    void toggleZoom();
    void applyZoom();
    /* Canvas matches the render target so fractional HiDPI stays sharp. */
    void updateRenderMetrics();
    void toggleMute();
    void requestFiles();
    void requestFolder();
    void showHelp() { m_helpOpen = true; }
    void applyPreset(const char *name, const std::array<double, EQ_BANDS> &bands, double preamp);

    void openMainMenu();
    void openPlaylistButtonMenu(int which);
    void openPlaylistContextMenu(int row, int mx, int my);
    void openPresetMenu();

    /* ---- input ---- */
    void handleTitlebarDrag();
    void handlePlaylistInput();
    void handleKeyboard(const SDL_KeyboardEvent &e);
    void handleDialogResults();

    /* ---- rendering ---- */
    void present();
    void drawSpectrum(Rect r);
    void drawMarquee(Rect box);
    void drawStatusText(Rect box);
    void drawTimeLcd(Rect r);

    /* ---- state ---- */
    SDL_Window *m_win = nullptr;
    SDL_Renderer *m_ren = nullptr;
    SDL_Texture *m_tex = nullptr;
    Canvas m_canvas;
    Ui m_ui;
    UiText m_uiFont;
    Menu m_menu;
    int m_zoom = 1; // Winamp-style "double size"

    int m_winW = 640; // logical (UI layout) size
    int m_winH = 440;
    float m_pixelScale = 1.f;

    AudioEngine m_engine;
    Prober m_prober;

    std::vector<Track> m_tracks;
    std::vector<bool> m_selected;
    int m_anchor = -1;
    int m_currentIndex = -1;
    int64_t m_totalDuration = 0;
    int m_selectionCount = 0;

    bool m_eqOpen = false;
    bool m_playlistOpen = true;
    bool m_helpOpen = false;
    bool m_shuffleOn = false;
    bool m_repeatOn = false;
    bool m_remaining = false;
    std::string m_visMode = "bars";
    std::string m_marqueeText = std::string("LINAMP ") + theme::versionShort() + "  -  DROP FILES HERE";
    double m_savedVolume = 0.82;
    std::string m_dragInfo;
    std::string m_lastSort;
    std::string m_front = "mainPanel";

    PanelBox m_main, m_playlist, m_eq;
    std::vector<int> m_zorder{PANEL_MAIN, PANEL_EQ, PANEL_PLAYLIST};

    std::array<double, EQ_BANDS> m_eqGains{};
    std::string m_eqPreset = "FLAT";
    std::string m_eqReadout;
    double m_eqReadoutUntil = 0;

    int m_plScroll = 0;
    int m_plHover = -1;
    int m_focusPanel = PANEL_MAIN;

    int m_dragPanel = -1;
    int m_dragOffX = 0, m_dragOffY = 0;
    bool m_dragMoved = false;
    bool m_resizingPlaylist = false;
    int m_resizeStartH = 0;
    float m_resizeStartY = 0;

    bool m_dragVol = false, m_dragBal = false, m_dragSeek = false, m_dragPreamp = false;
    std::array<bool, EQ_BANDS> m_dragBand{};
    double m_seekPreview = 0;

    std::array<float, SPECTRUM_BANDS> m_visVals{};
    std::array<float, SPECTRUM_BANDS> m_visPeaks{};

    double m_marqueeOffset = 0;

    double m_now = 0;
    double m_lastEngineTick = 0;
    double m_lastFrame = 0;
    double m_saveAt = -1;
    bool m_running = true;
    bool m_stateReady = false;
    bool m_hadLayout = false;

    bool m_inputDirty = true;
    double m_lastClickTime = -10;
    float m_lastClickX = 0, m_lastClickY = 0;

    std::mutex m_dialogMutex;
    std::vector<std::string> m_dialogFiles;
    std::vector<std::string> m_dialogFolders;

    Json m_audioSettings;
};
