/* ------------------------------------------------------------------
   Linamp — tiny immediate-mode widget layer on top of Canvas.
   Widgets mirror the original QML components pixel for pixel.
   ------------------------------------------------------------------ */
#pragma once

#include "font.h"
#include "gfx.h"
#include "text.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

constexpr uint64_t fnv(const char *s, uint64_t h = 1469598103934665603ull)
{
    return *s ? fnv(s + 1, (h ^ uint64_t(uint8_t(*s))) * 1099511628211ull) : h;
}

struct UiInput {
    float mx = -1.f, my = -1.f;
    bool down = false;
    bool pressed = false;
    bool released = false;
    bool doubleClick = false;
    bool rightPressed = false;
    bool rightReleased = false;
    bool shift = false;
    bool ctrl = false;
    float wheel = 0.f;
    bool mouseInside = false;
};

struct ButtonDesc {
    std::string icon;
    std::string trailingIcon;
    std::string label;
    std::string tip;
    bool active = false;
    bool showLed = false;
    int labelScale = 1;
};

struct ButtonResult {
    bool clicked = false;
    bool rightClicked = false;
    bool hovered = false;
};

struct SeekResult {
    bool seek = false;
    double seekTo = 0;
};

struct Tip {
    std::string text;
    Rect anchor;
    int z = 0;
};

class Ui {
public:
    Canvas *cv = nullptr;
    UiText *font = nullptr; // system UI font for menus / tooltips (optional)
    UiInput in;

    uint64_t hot = 0;
    uint64_t active = 0;
    bool blockPanels = false; // a menu / overlay owns the input

    std::vector<Tip> tips;
    int tipZ = 0;

    void beginRegion(Rect r, bool allowInput)
    {
        m_region = r;
        m_regionSet = true;
        m_allowInput = allowInput && !blockPanels;
    }
    void endRegion()
    {
        m_regionSet = false;
        m_allowInput = !blockPanels; // overlays / menus are never gated
    }

    bool allowedAt(float x, float y) const
    {
        if (!m_allowInput)
            return false;
        if (!m_regionSet)
            return true;
        return m_region.contains(int(x), int(y));
    }

    bool hovering(Rect r) const { return r.contains(int(in.mx), int(in.my)) && allowedAt(in.mx, in.my); }

    /* Id helpers for id arrays */
    static uint64_t id(const char *name) { return fnv(name); }

    ButtonResult button(uint64_t id, Rect r, const ButtonDesc &desc);
    bool hSlider(uint64_t id, Rect r, double &value, double from, double to, double def,
                 bool &dragging);
    bool vSlider(uint64_t id, Rect r, double &value, double from, double to, double def,
                 bool &dragging);
    SeekResult seekBar(uint64_t id, Rect r, double pos, double dur, bool &dragging,
                       double &preview);

    void tooltip(Rect anchor, std::string text);
    void drawTooltips();

private:
    Rect m_region;
    bool m_regionSet = false;
    bool m_allowInput = true;
};

namespace widgets {

void icon(Canvas &cv, Rect r, const std::string &name, Color color);
void lcdBox(Canvas &cv, Rect r);
void panelChrome(Canvas &cv, Rect r, const std::string &title, const std::string &version,
                 bool showLogo, bool compactLogo);
void metalButtonLook(Canvas &cv, Rect r, bool down, bool hover);
void scanlines(Canvas &cv, Rect r, int strength = 30);

} // namespace widgets

/* ---------------- menus ---------------- */
struct MenuItem {
    std::string label;
    std::string hint;
    bool sep = false;
    bool checked = false;
    bool disabled = false;
    std::function<void()> action = {};
};

struct Menu {
    bool open = false;
    int x = 0, y = 0, w = 180, columns = 1;
    int hover = -1;
    std::vector<MenuItem> items;

    void show(int px, int py, int pw, int cols, std::vector<MenuItem> it);
    void close();
    Rect bounds() const;
    /* returns true when the menu consumed the input */
    bool handle(Ui &ui);
    void draw(Ui &ui);
    /* true if the point is inside the menu */
    bool contains(float px, float py) const;
};
