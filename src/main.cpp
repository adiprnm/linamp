/* ------------------------------------------------------------------
   Linamp — SDL3 entry point.

   Native Winamp 2.x-style player: FFmpeg decode → biquad EQ → SDL audio,
   and a hand-drawn pixel UI rendered into a single ARGB texture.
   ------------------------------------------------------------------ */
#include "app.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

/* --- dev helpers (used by CI and for offline UI work) ------------------- */
void pushMouse(float x, float y, int type)
{
    SDL_Event e{};
    if (type == 0) {
        e.type = SDL_EVENT_MOUSE_MOTION;
        e.motion.x = x;
        e.motion.y = y;
    } else {
        e.type = type == 1 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.button = SDL_BUTTON_LEFT;
        e.button.down = type == 1;
        e.button.x = x;
        e.button.y = y;
        e.button.clicks = 1;
    }
    SDL_PushEvent(&e);
}

void pump(App &app)
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
        app.handleEvent(ev);
}

void click(App &app, float x, float y)
{
    pushMouse(x, y, 0);
    pushMouse(x, y, 1);
    pump(app);
    app.frame();
    pushMouse(x, y, 2);
    pump(app);
    app.frame();
    pushMouse(x, y, 0);
    pump(app);
    app.frame();
    std::fprintf(stderr, "click(%.0f,%.0f) -> %s\n", x, y, app.debugState().c_str());
}

void key(App &app, SDL_Keycode k, SDL_Keymod mod = SDL_KMOD_NONE);

/* Renders a few frames and dumps the result to $LINAMP_SCREENSHOT. */
int runScreenshotMode(App &app)
{
    const char *path = std::getenv("LINAMP_SCREENSHOT");
    const char *sceneEnv = std::getenv("LINAMP_SCENE");
    const std::string scene = sceneEnv ? sceneEnv : "";

    if (!std::getenv("LINAMP_KEEP_STATE"))
        app.setPlaylistForTest(scene);
    if (scene == "playing" && std::getenv("LINAMP_KEEP_STATE"))
        app.debugPlayFirst();

    const int frames = (scene == "playing" || scene == "showcase") ? 190 : 3;
    for (int i = 0; i < frames; ++i) {
        app.frame();
        SDL_Delay(16);
    }
    const bool ok = app.saveScreenshot(path);
    std::fprintf(stderr, "screenshot %s: %s\n", path, ok ? "ok" : SDL_GetError());
    return ok ? 0 : 1;
}

/* Drives the UI with synthetic input and prints the resulting state. */
int runClickTest(App &app)
{
    app.setPlaylistForTest();
    app.frame();
    std::fprintf(stderr, "initial -> %s\n", app.debugState().c_str());

    click(app, 41 + 8 + 24 + 12, 98 + 14 + 86 + 9);   // play
    click(app, 324 + 100, 98 + 14 + 12 + 12 + 6);     // playlist row 2

    // pointer over the playlist -> arrows navigate the list, not the volume
    pushMouse(324 + 100, 98 + 14 + 12 + 12 + 6, 0);
    pump(app);
    app.frame();
    key(app, SDLK_DOWN);
    key(app, SDLK_DOWN);
    key(app, SDLK_UP);
    key(app, SDLK_END);
    key(app, SDLK_HOME);
    key(app, SDLK_DOWN, SDL_KMOD_SHIFT);
    key(app, SDLK_PAGEDOWN);
    key(app, SDLK_RETURN);

    click(app, 41 + 8 + 16 + 40, 98 + 14 + 58 + 7);   // volume slider
    click(app, 41 + 8 + 8 + 130, 98 + 14 + 74 + 4);   // seek bar
    click(app, 41 + 8 + 210 + 13, 98 + 14 + 58 + 7);  // EQ button
    click(app, 41 + 8 + 238 + 10, 98 + 14 + 58 + 7);  // PL button (closes)
    click(app, 41 + 8 + 238 + 10, 98 + 14 + 58 + 7);  // PL button (reopens)
    click(app, 294, 105);                             // shade
    click(app, 294, 105);                             // unshade
    for (int i = 0; i < 3; ++i) {                     // rapid track switching
        click(app, 41 + 8 + 96 + 12, 98 + 14 + 86 + 9);  // next
        click(app, 41 + 8 + 0 + 12, 98 + 14 + 86 + 9);   // previous
    }
    click(app, 50, 104);                              // logo -> main menu
    {
        SDL_Event key2{};
        key2.type = SDL_EVENT_KEY_DOWN;
        key2.key.key = SDLK_D;
        key2.key.mod = SDL_KMOD_CTRL;
        SDL_PushEvent(&key2);
        pump(app);
        app.frame();
        std::fprintf(stderr, "ctrl+d -> %s\n", app.debugState().c_str());
        // toggle back, close the menu and hover the play button (shows a tooltip)
        SDL_PushEvent(&key2);
        SDL_Event esc{};
        esc.type = SDL_EVENT_KEY_DOWN;
        esc.key.key = SDLK_ESCAPE;
        SDL_PushEvent(&esc);
        pump(app);
        app.frame();
    }

    // pointer over the player panel -> arrows change the volume again
    pushMouse(41 + 100, 98 + 14 + 58 + 7, 0);
    pump(app);
    app.frame();
    key(app, SDLK_DOWN);

    pushMouse(41 + 8 + 24 + 12, 98 + 14 + 86 + 9, 0);
    pump(app);
    app.frame();
    app.saveScreenshot("/tmp/linamp_test.bmp");
    return 0;
}

void key(App &app, SDL_Keycode k, SDL_Keymod mod)
{
    SDL_Event e{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = k;
    e.key.mod = mod;
    SDL_PushEvent(&e);
    pump(app);
    app.frame();
    std::fprintf(stderr, "key(%s) -> %s\n", SDL_GetKeyName(k), app.debugState().c_str());
}

} // namespace

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("Linamp", "1.1.0", "local.linamp.app");
    SDL_SetHint(SDL_HINT_APP_ID, "linamp");
    SDL_SetHint(SDL_HINT_APP_NAME, "Linamp");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int exitCode = 0;
    {
        App app;
        if (!app.init()) {
            std::fprintf(stderr, "Linamp: startup failed: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }

        if (std::getenv("LINAMP_SCREENSHOT")) {
            exitCode = runScreenshotMode(app);
        } else if (std::getenv("LINAMP_TEST")) {
            exitCode = runClickTest(app);
        } else {
            SDL_Event event;
            double lastDraw = 0;
            while (app.running()) {
                while (SDL_PollEvent(&event))
                    app.handleEvent(event);
                if (!app.running())
                    break;

                // 60 Hz while something animates, slower when the UI is idle.
                const double now = double(SDL_GetTicksNS()) / 1e9;
                const double interval = app.wantsFastFrames() ? 1.0 / 60.0 : 1.0 / 20.0;
                if (now - lastDraw >= interval) {
                    app.frame();
                    lastDraw = now;
                } else {
                    SDL_Delay(4);
                }
            }
        }

        app.shutdown();
    } // App (and its audio device) destructed before SDL_Quit()

    SDL_Quit();
    return exitCode;
}
