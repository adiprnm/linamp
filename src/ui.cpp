#include "ui.h"

#include <algorithm>
#include <cmath>

namespace {

struct IconPainter {
    Canvas &cv;
    Rect r;
    Color color;

    float sx() const { return float(r.w) / 12.f; }
    float sy() const { return float(r.h) / 12.f; }

    void rect(float x, float y, float w, float h) const
    {
        const int x0 = int(std::lround(r.x + x * sx()));
        const int y0 = int(std::lround(r.y + y * sy()));
        const int x1 = int(std::lround(r.x + (x + w) * sx()));
        const int y1 = int(std::lround(r.y + (y + h) * sy()));
        cv.fillRect({x0, y0, std::max(1, x1 - x0), std::max(1, y1 - y0)}, color);
    }
    void tri(float x1, float y1, float x2, float y2, float x3, float y3) const
    {
        cv.fillTriangle(r.x + x1 * sx(), r.y + y1 * sy(), r.x + x2 * sx(), r.y + y2 * sy(),
                        r.x + x3 * sx(), r.y + y3 * sy(), color);
    }
    void line(float x1, float y1, float x2, float y2) const
    {
        cv.line(int(std::lround(r.x + x1 * sx())), int(std::lround(r.y + y1 * sy())),
                int(std::lround(r.x + x2 * sx())), int(std::lround(r.y + y2 * sy())), color);
    }
    void arc(float cx, float cy, float rad, float a0, float a1) const
    {
        cv.strokeArc(r.x + cx * sx(), r.y + cy * sy(), rad * sx(), a0, a1, color);
    }
};

} // namespace

namespace widgets {

void icon(Canvas &cv, Rect r, const std::string &name, Color color)
{
    IconPainter p{cv, r, color};

    if (name == "prev") {
        p.rect(1, 1, 2, 10);
        p.tri(11, 1, 5, 6, 11, 11);
    } else if (name == "play") {
        p.tri(3, 1, 11, 6, 3, 11);
    } else if (name == "pause") {
        p.rect(3, 1, 2, 10);
        p.rect(7, 1, 2, 10);
    } else if (name == "stop") {
        p.rect(2, 2, 8, 8);
    } else if (name == "next") {
        p.tri(1, 1, 7, 6, 1, 11);
        p.rect(9, 1, 2, 10);
    } else if (name == "eject") {
        p.tri(1, 5, 6, 1, 11, 5);
        p.rect(1, 7, 10, 2);
    } else if (name == "shuffle") {
        p.line(1, 3, 4, 3);
        p.line(4, 3, 8, 9);
        p.line(8, 9, 11, 9);
        p.line(1, 9, 4, 9);
        p.line(4, 9, 8, 3);
        p.line(8, 3, 11, 3);
        p.tri(9, 1.5f, 11.5f, 3, 9, 4.5f);
        p.tri(9, 7.5f, 11.5f, 9, 9, 10.5f);
    } else if (name == "repeat") {
        p.line(3, 2, 9, 2);
        p.line(9, 2, 9, 5);
        p.line(10, 3, 10, 7);
        p.line(9, 10, 3, 10);
        p.line(3, 10, 3, 7);
        p.line(2, 9, 2, 5);
        p.tri(8, 0.5f, 10.5f, 2, 8, 3.5f);
        p.tri(4, 8.5f, 1.5f, 10, 4, 11.5f);
    } else if (name == "min") {
        p.rect(2, 8, 8, 1);
    } else if (name == "shade") {
        p.rect(2, 2, 8, 1);
        p.rect(2, 2, 1, 7);
        p.rect(2, 8, 8, 1);
        p.rect(9, 2, 1, 7);
        p.rect(2, 5, 8, 1);
    } else if (name == "close") {
        p.line(2.5f, 2.5f, 8.5f, 8.5f);
        p.line(8.5f, 2.5f, 2.5f, 8.5f);
    } else if (name == "caret") {
        p.tri(1, 2, 9, 2, 5, 6);
    } else if (name == "speaker") {
        p.rect(1, 4, 2, 4);
        p.tri(3, 4, 6, 1, 6, 11);
        p.rect(3, 4, 2, 4);
        p.arc(7.5f, 6, 2.5f, -1.0f, 1.0f);
        p.arc(7.5f, 6, 4.5f, -0.9f, 0.9f);
    } else if (name == "led") {
        p.rect(0, 0, 4, 4);
    }
}

void metalButtonLook(Canvas &cv, Rect r, bool down, bool hover)
{
    cv.fillRect(r, rgb(0x070707));
    const Rect inner = r.inset(1);
    if (inner.w > 0 && inner.h > 0) {
        if (down) {
            cv.vGrad(inner, {{0.f, rgb(0x333333)}, {0.42f, rgb(0x2b2b2b)},
                             {0.60f, rgb(0x262626)}, {1.f, rgb(0x303030)}});
        } else {
            cv.vGrad(inner, {{0.f, rgb(0x9a9a9a)}, {0.42f, rgb(0x727272)},
                             {0.60f, rgb(0x4d4d4d)}, {1.f, rgb(0x666666)}});
        }
    }
    const Color hi = down ? rgb(0x121212) : rgb(0xd2d2d2);
    const Color lo = down ? rgb(0x6d6d6d) : rgb(0x2a2a2a);
    cv.hLine(r.x + 1, r.y + 1, r.w - 2, hi);
    cv.vLine(r.x + 1, r.y + 1, r.h - 2, down ? rgb(0x121212) : rgb(0xbdbdbd));
    cv.hLine(r.x + 1, r.bottom() - 2, r.w - 2, lo);
    cv.vLine(r.right() - 2, r.y + 1, r.h - 2, lo);
    if (hover && !down)
        cv.fillRect(r, rgb(0xffffff, 18));
}

void scanlines(Canvas &cv, Rect r, int strength)
{
    const Color c = rgb(0x000000, uint32_t(strength));
    for (int y = r.y; y < r.bottom(); y += 3)
        cv.fillRect({r.x, y, r.w, 1}, c);
}

void lcdBox(Canvas &cv, Rect r)
{
    cv.fillRect(r, rgb(0x000000));
    const Rect inner = r.inset(1);
    if (inner.w <= 0 || inner.h <= 0)
        return;
    cv.vGrad(inner, {{0.f, rgb(0x08170b)}, {0.55f, rgb(0x040b06)}, {1.f, rgb(0x020503)}});
    cv.outline(inner, rgb(0x12351a, 178));
    scanlines(cv, inner, 30); // Scanlines opacity 0.5 * fill alpha 0.30 -> ~0.15
}

void panelChrome(Canvas &cv, Rect r, const std::string &title, const std::string &version,
                 bool showLogo, bool compactLogo)
{
    cv.fillRect(r, rgb(0x0b0b0b));
    const Rect inner = r.inset(1);
    cv.fillRect(inner, rgb(0x2a2a2a));
    cv.outline(inner, rgb(0x5a5a5a));

    // title bar
    const Rect bar{r.x, r.y, r.w, 14};
    cv.vGrad(bar, {{0.f, rgb(0x7d7d7d)}, {0.42f, rgb(0x5e5e5e)},
                   {0.58f, rgb(0x444444)}, {1.f, rgb(0x303030)}});
    cv.hLine(bar.x, bar.y, bar.w, rgb(0xbdbdbd, 140));
    cv.hLine(bar.x, bar.bottom() - 1, bar.w, rgb(0x080808));

    if (showLogo) {
        const Rect logo{r.x + 4, r.y + 2, 10, 10};
        cv.fillRect(logo, rgb(0x111111));
        cv.outline(logo, rgb(0x050505));
        cv.fillRect(logo.inset(1), rgb(0x111111));
        icon(cv, logo.inset(1), "play", rgb(0x2dff4d));
    } else if (compactLogo) {
        // dashed 10x10 placeholder ("shaded" look)
        for (int yy = 1; yy < 10; yy += 4)
            for (int xx = 1; xx < 10; xx += 4)
                cv.fillRect({r.x + 4 + xx, r.y + 3 + yy, 1, 1}, rgb(0x9a9a9a));
    }

    if (!title.empty()) {
        pf::draw(cv, title, r.x + 18, r.y + 4, 1, rgb(0xe4e4e4));
        if (!version.empty()) {
            const int w = pf::measure(title, 1, 1);
            pf::draw(cv, version, r.x + 18 + w + 4, r.y + 4, 1, rgb(0x9a9a9a));
        }
    }

    // body
    if (r.h > 14) {
        const Rect body{r.x, r.y + 14, r.w, r.h - 14};
        cv.vGrad(body, {{0.f, rgb(0x2c2c2c)}, {0.5f, rgb(0x262626)}, {1.f, rgb(0x1e1e1e)}});
        cv.hLine(body.x, body.y, body.w, rgb(0x111111));
        cv.vLine(body.x, body.y + 1, body.h - 2, rgb(0x4a4a4a, 128));
        cv.vLine(body.right() - 1, body.y + 1, body.h - 2, rgb(0x111111));
        cv.hLine(body.x, body.bottom() - 1, body.w, rgb(0x0c0c0c));
    }
}

} // namespace widgets

/* ==================================================================
   Ui
   ================================================================== */

ButtonResult Ui::button(uint64_t wid, Rect r, const ButtonDesc &desc)
{
    ButtonResult res;
    const bool hover = hovering(r);
    res.hovered = hover;
    if (hover && in.mouseInside)
        hot = wid;

    const bool pressedHere = hover && in.pressed;
    if (pressedHere)
        active = wid;
    if (active == wid && in.released) {
        if (hover)
            res.clicked = true;
        active = 0;
    }
    if (hover && in.rightPressed)
        res.rightClicked = true;

    const bool down = (active == wid && in.down) || desc.active;
    widgets::metalButtonLook(*cv, r, down, hover);

    const int dy = (active == wid && in.down) ? 1 : 0;
    const Color iconColor = desc.active ? rgb(0x46ff5e)
                                        : ((active == wid && in.down) ? rgb(0xcfcfcf) : rgb(0xe6e6e6));

    if (!desc.icon.empty() && desc.label.empty()) {
        Rect ir{0, 0, 12, 12};
        ir.x = r.x + (r.w - ir.w) / 2;
        ir.y = r.y + (r.h - ir.h) / 2 + dy;
        widgets::icon(*cv, ir, desc.icon, iconColor);
    }

    if (!desc.label.empty()) {
        const int ledW = desc.showLed ? 4 + 3 : 0;
        const int tw = pf::measure(desc.label, desc.labelScale, 1);
        const int total = ledW + tw;
        int x = r.x + (r.w - total) / 2;
        const int y = r.y + (r.h - pf::CH_H * desc.labelScale) / 2 + dy;
        if (desc.showLed) {
            const Rect led{x, y + (pf::CH_H * desc.labelScale - 4) / 2, 4, 4};
            cv->fillRect(led, desc.active ? rgb(0x3dff5e) : rgb(0x232323));
            cv->outline(led, rgb(0x000000));
            x += 7;
        }
        pf::draw(*cv, desc.label, x, y, desc.labelScale,
                 desc.active ? rgb(0x46ff5e) : rgb(0xe2e2e2));
    }

    if (!desc.trailingIcon.empty()) {
        Rect tr{r.right() - 4 - 8, r.y + (r.h - 6) / 2 + dy, 8, 6};
        widgets::icon(*cv, tr, desc.trailingIcon, rgb(0xd2d2d2));
    }

    if (hover && !desc.tip.empty())
        tooltip(r, desc.tip);

    return res;
}

bool Ui::hSlider(uint64_t wid, Rect r, double &value, double from, double to, double def,
                 bool &dragging)
{
    const bool hover = hovering(r);
    if (hover)
        hot = wid;

    auto setFromX = [&](float mx) {
        const double local = double(mx) - double(r.x);
        const double p = theme::clamp((local - 4.5) / std::max(1.0, double(r.w) - 9.0), 0.0, 1.0);
        value = from + (to - from) * p;
    };

    bool changed = false;
    if (hover && in.pressed) {
        active = wid;
        dragging = true;
        setFromX(in.mx);
        changed = true;
    }
    if (active == wid && in.down && dragging) {
        setFromX(in.mx);
        changed = true;
    }
    if (active == wid && in.released) {
        dragging = false;
        active = 0;
    }
    if (hover && in.doubleClick) {
        value = def;
        changed = true;
    }
    if (hover && in.wheel != 0.f) {
        const double step = (to - from) * 0.03;
        value = theme::clamp(value + (in.wheel > 0 ? step : -step), from, to);
        changed = true;
    }

    const double p = (to == from) ? 0.0 : (value - from) / (to - from);

    // track
    const Rect track{r.x, r.y + (r.h - 8) / 2, r.w, 8};
    cv->fillRect(track, rgb(0x050505));
    cv->outline(track, rgb(0x000000));
    cv->hLine(track.x + 1, track.y + 1, track.w - 2, rgb(0x000000, 204));

    // thumb
    const Rect thumb{track.x + int((track.w - 9) * theme::clamp(p, 0.0, 1.0)), r.y, 9, 14};
    cv->vGrad(thumb, {{0.f, rgb(0xdedede)}, {0.45f, rgb(0x9a9a9a)},
                      {0.56f, rgb(0x626262)}, {1.f, rgb(0x8f8f8f)}});
    cv->outline(thumb, rgb(0x080808));
    cv->vLine(thumb.x + 3, thumb.y + 2, 9, rgb(0x2c2c2c, 217));
    cv->hLine(thumb.x + 1, thumb.y + 1, thumb.w - 2, rgb(0xffffff, 140));

    return changed;
}

bool Ui::vSlider(uint64_t wid, Rect r, double &value, double from, double to, double def,
                 bool &dragging)
{
    const bool hover = hovering(r);
    if (hover)
        hot = wid;

    auto setFromY = [&](float my) {
        const double local = double(my) - double(r.y);
        double p = 1.0 - (local - 4.5) / std::max(1.0, double(r.h) - 9.0);
        p = theme::clamp(p, 0.0, 1.0);
        value = from + (to - from) * p;
    };

    bool changed = false;
    if (hover && in.pressed) {
        active = wid;
        dragging = true;
        setFromY(in.my);
        changed = true;
    }
    if (active == wid && in.down && dragging) {
        setFromY(in.my);
        changed = true;
    }
    if (active == wid && in.released) {
        dragging = false;
        active = 0;
    }
    if (hover && in.doubleClick) {
        value = def;
        changed = true;
    }
    if (hover && in.wheel != 0.f) {
        const double step = (to - from) * 0.03;
        value = theme::clamp(value + (in.wheel > 0 ? step : -step), from, to);
        changed = true;
    }

    const double p = (to == from) ? 0.0 : (value - from) / (to - from);

    const Rect track{r.x + (r.w - 8) / 2, r.y, 8, r.h};
    cv->fillRect(track, rgb(0x060606));
    cv->outline(track, rgb(0x000000));
    cv->hLine(track.x + 1, track.y + 1, track.w - 2, rgb(0x000000));
    cv->vLine(track.right() - 2, track.y + 1, track.h - 2, rgb(0x2a2a2a));

    const int ty = track.y + int((track.h - 9) * (1.0 - theme::clamp(p, 0.0, 1.0)));
    const Rect thumb{r.x + (r.w - 12) / 2, ty, 12, 9};
    cv->vGrad(thumb, {{0.f, rgb(0xe4e4e4)}, {0.45f, rgb(0x9d9d9d)},
                      {0.58f, rgb(0x666666)}, {1.f, rgb(0x929292)}});
    cv->outline(thumb, rgb(0x080808));
    cv->hLine(thumb.x + 1, thumb.y + 1, thumb.w - 2, rgb(0xffffff, 153));

    return changed;
}

SeekResult Ui::seekBar(uint64_t wid, Rect r, double pos, double dur, bool &dragging,
                       double &preview)
{
    SeekResult out;
    const bool hover = hovering(r);
    if (hover)
        hot = wid;

    auto updatePreview = [&](float mx) {
        if (dur <= 0)
            return;
        const double local = double(mx) - double(r.x);
        const double p = theme::clamp(local / std::max(1, r.w), 0.0, 1.0);
        preview = p * dur;
    };

    if (hover && in.pressed) {
        active = wid;
        dragging = true;
        updatePreview(in.mx);
    }
    if (active == wid && in.down && dragging)
        updatePreview(in.mx);
    if (active == wid && in.released) {
        dragging = false;
        active = 0;
        if (dur > 0) {
            out.seek = true;
            out.seekTo = preview;
        }
    }

    double p = 0;
    if (dragging)
        p = dur > 0 ? theme::clamp(preview / dur, 0.0, 1.0) : 0.0;
    else
        p = dur > 0 ? theme::clamp(pos / dur, 0.0, 1.0) : 0.0;

    cv->fillRect(r, rgb(0x070707));
    cv->outline(r, rgb(0x000000));

    const Rect prog{r.x + 1, r.y + 1, int((r.w - 2) * p), r.h - 2};
    if (prog.w > 0)
        cv->vGrad(prog, {{0.f, rgb(0x176b2c, 191)}, {0.5f, rgb(0x26c247, 191)},
                         {1.f, rgb(0x0d4a1c, 191)}});

    const Rect thumb{r.x + int((r.w - 7) * p), r.y - 2, 7, 12};
    cv->vGrad(thumb, {{0.f, rgb(0xdedede)}, {0.45f, rgb(0x8b8b8b)},
                      {0.58f, rgb(0x5d5d5d)}, {1.f, rgb(0x8c8c8c)}});
    cv->outline(thumb, rgb(0x080808));

    if (dragging) {
        const std::string txt = theme::fmt(preview);
        const int tw = pf::measure(txt, 1, 1) + 10;
        int bx = int((r.w - 7) * p) + 3 - tw / 2;
        bx = theme::clampInt(bx, 0, std::max(0, r.w - tw)) + r.x;
        const Rect box{bx, r.y - 21, tw, 17};
        cv->fillRect(box, theme::yellow);
        cv->outline(box, rgb(0x101010));
        pf::draw(*cv, txt, box.x + (box.w - pf::measure(txt, 1, 1)) / 2,
                 box.y + (box.h - pf::CH_H) / 2, 1, rgb(0x101010));
    }

    return out;
}

void Ui::tooltip(Rect anchor, std::string text)
{
    tips.push_back({std::move(text), anchor, tipZ});
}

void Ui::drawTooltips()
{
    if (tips.empty())
        return;
    // last registered tip belongs to the front-most region
    const Tip tip = tips.back();
    tips.clear();
    const int tw = pf::measure(tip.text, 1, 1) + 10;
    Rect box{tip.anchor.x + (tip.anchor.w - tw) / 2, tip.anchor.y - 17 - 6, tw, 17};
    box.x = std::max(0, std::min(box.x, cv->width() - box.w));
    box.y = std::max(0, box.y);
    cv->fillRect(box, theme::yellow);
    cv->outline(box, rgb(0x101010));
    pf::draw(*cv, tip.text, box.x + (box.w - pf::measure(tip.text, 1, 1)) / 2,
             box.y + (box.h - pf::CH_H) / 2, 1, rgb(0x101010));
}

/* ==================================================================
   Menu
   ================================================================== */

void Menu::show(int px, int py, int pw, int cols, std::vector<MenuItem> it)
{
    x = px;
    y = py;
    w = pw;
    columns = std::max(1, cols);
    items = std::move(it);
    open = true;
    hover = -1;
}

void Menu::close()
{
    open = false;
    hover = -1;
    items.clear();
}

Rect Menu::bounds() const
{
    if (items.empty())
        return {x, y, w, 4};
    const int rows = int((items.size() + size_t(columns) - 1) / size_t(columns));
    int h = 4;
    for (int r = 0; r < rows; ++r) {
        int rowH = 0;
        for (int c = 0; c < columns; ++c) {
            const size_t idx = size_t(r) * size_t(columns) + size_t(c);
            if (idx >= items.size())
                break;
            rowH = std::max(rowH, items[idx].sep ? 6 : 18);
        }
        h += rowH;
    }
    return {x, y, w, h};
}

bool Menu::contains(float px, float py) const
{
    return open && bounds().contains(int(px), int(py));
}

bool Menu::handle(Ui &ui)
{
    if (!open)
        return false;
    const Rect b = bounds();
    const bool inside = b.contains(int(ui.in.mx), int(ui.in.my));

    // hit test items
    int hitIdx = -1;
    if (inside) {
        const int rows = int((items.size() + size_t(columns) - 1) / size_t(columns));
        int cy = b.y + 2;
        for (int r = 0; r < rows; ++r) {
            int rowH = 0;
            for (int c = 0; c < columns; ++c) {
                const size_t idx = size_t(r) * size_t(columns) + size_t(c);
                if (idx >= items.size())
                    break;
                rowH = std::max(rowH, items[idx].sep ? 6 : 18);
            }
            if (ui.in.my >= float(cy) && ui.in.my < float(cy + rowH)) {
                const int colW = (w - 4) / columns;
                const int col = std::min(columns - 1, std::max(0, (int(ui.in.mx) - (b.x + 2)) / std::max(1, colW)));
                const size_t idx = size_t(r) * size_t(columns) + size_t(col);
                if (idx < items.size() && !items[idx].sep && !items[idx].disabled)
                    hitIdx = int(idx);
                break;
            }
            cy += rowH;
        }
    }
    hover = hitIdx;

    if (ui.in.released || (ui.in.pressed && inside)) {
        if (hitIdx >= 0) {
            auto action = items[size_t(hitIdx)].action;
            close();
            if (action)
                action();
            return true;
        }
    }
    if (ui.in.pressed && !inside) {
        close();
        return false; // let the click reach whatever is underneath
    }
    return true;
}

void Menu::draw(Ui &ui)
{
    if (!open)
        return;
    Canvas &cv = *ui.cv;
    const Rect b = bounds();
    cv.fillRect(b, rgb(0xc0c0c0));
    cv.outline(b, rgb(0x000000));
    cv.hLine(b.x + 1, b.y + 1, b.w - 2, rgb(0xffffff));
    cv.vLine(b.x + 1, b.y + 1, b.h - 2, rgb(0xffffff));
    cv.hLine(b.x + 1, b.bottom() - 2, b.w - 2, rgb(0x808080));
    cv.vLine(b.right() - 2, b.y + 1, b.h - 2, rgb(0x808080));

    const int rows = int((items.size() + size_t(columns) - 1) / size_t(columns));
    const int colW = (w - 4) / columns;
    int cy = b.y + 2;
    for (int r = 0; r < rows; ++r) {
        int rowH = 0;
        for (int c = 0; c < columns; ++c) {
            const size_t idx = size_t(r) * size_t(columns) + size_t(c);
            if (idx >= items.size())
                break;
            rowH = std::max(rowH, items[idx].sep ? 6 : 18);
        }
        for (int c = 0; c < columns; ++c) {
            const size_t idx = size_t(r) * size_t(columns) + size_t(c);
            if (idx >= items.size())
                break;
            const MenuItem &it = items[idx];
            const Rect cell{b.x + 2 + c * colW, cy, colW, rowH};
            if (it.sep) {
                cv.fillRect({cell.x, cell.y + 2, cell.w - 4, 2}, rgb(0x808080));
                cv.hLine(cell.x, cell.y + 2, cell.w - 4, rgb(0xffffff));
                continue;
            }
            const bool isHover = int(idx) == hover;
            if (isHover)
                cv.fillRect(cell, rgb(0x000080));
            const Color fg = isHover ? rgb(0xffffff)
                                     : (it.disabled ? rgb(0x808080) : rgb(0x000000));
            if (it.checked) {
                // small check mark
                cv.line(cell.x + 5, cell.y + 9, cell.x + 7, cell.y + 12, fg);
                cv.line(cell.x + 7, cell.y + 12, cell.x + 12, cell.y + 4, fg);
            }
            const int textMax = cell.w - 30;
            pf::draw(cv, pf::ellipsize(it.label, textMax), cell.x + 20, cell.y + 6, 1, fg);
            if (!it.hint.empty()) {
                const int hw = pf::measure(it.hint, 1, 1);
                pf::draw(cv, it.hint, cell.right() - 5 - hw, cell.y + 6, 1,
                         isHover ? rgb(0xffffff) : rgb(0x666666));
            }
        }
        cy += rowH;
    }
}
