#include "gfx.h"

#include <cmath>
#include <cstring>

void Canvas::reset(int w, int h)
{
    if (w_ != w || h_ != h) {
        w_ = w;
        h_ = h;
        px_.assign(size_t(w) * size_t(h), 0u);
    }
    std::fill(px_.begin(), px_.end(), 0u);
    clip_ = {0, 0, w_, h_};
    clipStack_.clear();
}

void Canvas::clear(Color c)
{
    std::fill(px_.begin(), px_.end(), c);
}

inline void Canvas::blendPixel(int x, int y, Color c)
{
    if (!clip_.contains(x, y))
        return;
    const uint32_t a = (c >> 24) & 0xFFu;
    const size_t idx = size_t(y) * size_t(w_) + size_t(x);
    if (a == 0)
        return;
    if (a == 255) {
        px_[idx] = c | 0xFF000000u;
        return;
    }
    const uint32_t dst = px_[idx];
    const uint32_t da = (dst >> 24) & 0xFFu;
    const uint32_t outA = a + da * (255u - a) / 255u;
    auto mix = [&](uint32_t sc, uint32_t dc) {
        const uint32_t sa = a;
        const uint32_t num = sc * sa * 255u + dc * da * (255u - sa);
        const uint32_t den = outA * 255u;
        return den ? (num / den) & 0xFFu : 0u;
    };
    const uint32_t r = mix((c >> 16) & 0xFFu, (dst >> 16) & 0xFFu);
    const uint32_t g = mix((c >> 8) & 0xFFu, (dst >> 8) & 0xFFu);
    const uint32_t b = mix(c & 0xFFu, dst & 0xFFu);
    px_[idx] = (outA << 24) | (r << 16) | (g << 8) | b;
}

void Canvas::fillRect(Rect r, Color c)
{
    r = r.intersect(clip_);
    if (r.empty())
        return;
    for (int y = r.y; y < r.bottom(); ++y)
        for (int x = r.x; x < r.right(); ++x)
            blendPixel(x, y, c);
}

void Canvas::fillRectRaw(Rect r, Color c)
{
    r = r.intersect(clip_);
    if (r.empty())
        return;
    for (int y = r.y; y < r.bottom(); ++y) {
        uint32_t *row = px_.data() + size_t(y) * size_t(w_);
        for (int x = r.x; x < r.right(); ++x)
            row[x] = c;
    }
}

void Canvas::vGrad(Rect r, std::initializer_list<GradStop> stops)
{
    r = r.intersect(clip_);
    if (r.empty() || stops.size() == 0)
        return;
    std::vector<GradStop> list(stops);
    if (list.size() == 1)
        list.push_back(list[0]);
    const float h = float(std::max(1, r.h - 1));
    for (int y = 0; y < r.h; ++y) {
        const float t = float(y) / h;
        // find segment
        size_t i = 0;
        while (i + 2 < list.size() && t > list[i + 1].pos)
            ++i;
        const GradStop &a = list[i];
        const GradStop &b = list[i + 1];
        const float span = (b.pos - a.pos) > 1e-6f ? (b.pos - a.pos) : 1e-6f;
        float k = (t - a.pos) / span;
        k = std::max(0.f, std::min(1.f, k));
        auto lerp = [&](int shift) {
            const float ca = float((a.color >> shift) & 0xFFu);
            const float cb = float((b.color >> shift) & 0xFFu);
            return uint32_t(ca + (cb - ca) * k) & 0xFFu;
        };
        const uint32_t alpha = lerp(24);
        const Color c = (alpha << 24) | (lerp(16) << 16) | (lerp(8) << 8) | lerp(0);
        const int row = r.y + y;
        for (int x = r.x; x < r.right(); ++x)
            blendPixel(x, row, c);
    }
}

void Canvas::hLine(int x, int y, int w, Color c)
{
    fillRect({x, y, w, 1}, c);
}

void Canvas::vLine(int x, int y, int h, Color c)
{
    fillRect({x, y, 1, h}, c);
}

void Canvas::outline(Rect r, Color c)
{
    if (r.w <= 0 || r.h <= 0)
        return;
    hLine(r.x, r.y, r.w, c);
    hLine(r.x, r.bottom() - 1, r.w, c);
    vLine(r.x, r.y, r.h, c);
    vLine(r.right() - 1, r.y, r.h, c);
}

void Canvas::plot(int x, int y, Color c)
{
    blendPixel(x, y, c);
}

void Canvas::line(int x0, int y0, int x1, int y1, Color c)
{
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        blendPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Canvas::fillTriangle(float x1, float y1, float x2, float y2, float x3, float y3, Color c)
{
    const int minX = (int)std::floor(std::min({x1, x2, x3}));
    const int maxX = (int)std::ceil(std::max({x1, x2, x3}));
    const int minY = (int)std::floor(std::min({y1, y2, y3}));
    const int maxY = (int)std::ceil(std::max({y1, y2, y3}));
    auto edge = [](float ax, float ay, float bx, float by, float px, float py) {
        return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    };
    const float area = edge(x1, y1, x2, y2, x3, y3);
    if (std::fabs(area) < 1e-6f)
        return;
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float px = float(x) + 0.5f, py = float(y) + 0.5f;
            const float w0 = edge(x1, y1, x2, y2, px, py) / area;
            const float w1 = edge(x2, y2, x3, y3, px, py) / area;
            const float w2 = edge(x3, y3, x1, y1, px, py) / area;
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                blendPixel(x, y, c);
        }
    }
}

void Canvas::strokeArc(float cx, float cy, float radius, float a0, float a1, Color c)
{
    const int steps = std::max(8, int(std::fabs(a1 - a0) * radius * 3.f));
    int px = 0, py = 0;
    for (int i = 0; i <= steps; ++i) {
        const float t = a0 + (a1 - a0) * float(i) / float(steps);
        const int x = int(std::round(cx + std::cos(t) * radius));
        const int y = int(std::round(cy + std::sin(t) * radius));
        if (i > 0)
            line(px, py, x, y, c);
        px = x;
        py = y;
    }
}

void Canvas::pushClip(Rect r)
{
    clipStack_.push_back(clip_);
    clip_ = r.intersect(clip_);
}

void Canvas::popClip()
{
    if (!clipStack_.empty()) {
        clip_ = clipStack_.back();
        clipStack_.pop_back();
    }
}
