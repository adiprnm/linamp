#define STB_TRUETYPE_IMPLEMENTATION
#include "text.h"

#include "font.h"
#include "util.h"

#include <algorithm>
#include <cstdlib>

namespace {

const char *FONT_CANDIDATES[] = {
    // Linux (Arch/Fedora/Debian/Ubuntu layouts)
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
    "/usr/share/fonts/gnu-free/FreeSans.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/TTF/arial.ttf",
    "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
    "/usr/share/fonts/TTF/tahoma.ttf",
    "/usr/share/fonts/TTF/verdana.ttf",
    // Windows
    "C:/Windows/Fonts/tahoma.ttf",
    "C:/Windows/Fonts/segoeui.ttf",
    "C:/Windows/Fonts/arial.ttf",
    "C:/Windows/Fonts/verdana.ttf",
    // macOS
    "/System/Library/Fonts/Supplemental/Arial.ttf",
    "/System/Library/Fonts/Supplemental/Verdana.ttf",
    "/System/Library/Fonts/Supplemental/Tahoma.ttf",
    "/Library/Fonts/Arial.ttf",
};

uint32_t nextCodepoint(std::string_view s, size_t &i)
{
    const unsigned char c = (unsigned char)s[i];
    if (c < 0x80) {
        ++i;
        return c;
    }
    auto cont = [&](size_t k) { return i + k < s.size() && ((unsigned char)s[i + k] & 0xC0) == 0x80; };
    if ((c & 0xE0) == 0xC0 && cont(1)) {
        const uint32_t cp = ((c & 0x1Fu) << 6) | ((unsigned char)s[i + 1] & 0x3Fu);
        i += 2;
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && cont(1) && cont(2)) {
        const uint32_t cp = ((c & 0x0Fu) << 12) | (((unsigned char)s[i + 1] & 0x3Fu) << 6) |
                            ((unsigned char)s[i + 2] & 0x3Fu);
        i += 3;
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && cont(1) && cont(2) && cont(3)) {
        const uint32_t cp = ((c & 0x07u) << 18) | (((unsigned char)s[i + 1] & 0x3Fu) << 12) |
                            (((unsigned char)s[i + 2] & 0x3Fu) << 6) | ((unsigned char)s[i + 3] & 0x3Fu);
        i += 4;
        return cp;
    }
    ++i;
    return '?';
}

} // namespace

bool UiText::load()
{
    auto tryPath = [this](const std::string &path) {
        const std::string data = util::readFile(path);
        if (data.size() < 1024)
            return false;
        m_data.assign(data.begin(), data.end());
        const int offset = stbtt_GetFontOffsetForIndex(m_data.data(), 0);
        if (offset < 0)
            return false;
        if (!stbtt_InitFont(&m_font, m_data.data(), offset))
            return false;
        m_path = path;
        m_ok = true;
        m_atlases.clear();
        return true;
    };

    if (const char *env = std::getenv("LINAMP_UI_FONT"); env && *env) {
        if (tryPath(env))
            return true;
    }
    for (const char *path : FONT_CANDIDATES) {
        if (tryPath(path))
            return true;
    }
    m_ok = false;
    return false;
}

const UiText::Atlas *UiText::atlasFor(int px) const
{
    if (!m_ok || px <= 0)
        return nullptr;
    auto it = m_atlases.find(px);
    if (it != m_atlases.end())
        return &it->second;
    Atlas atlas;
    buildAtlas(atlas, px);
    auto res = m_atlases.emplace(px, std::move(atlas));
    return &res.first->second;
}

void UiText::buildAtlas(Atlas &atlas, int px) const
{
    const float scale = stbtt_ScaleForPixelHeight(&m_font, float(px));
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&m_font, &ascent, &descent, &lineGap);
    atlas.px = px;
    atlas.ascent = int(std::ceil(ascent * scale));
    atlas.lineHeight = int(std::ceil((ascent - descent + lineGap) * scale));

    const int maxWidth = 512;
    int penX = 0, penY = 0, rowHeight = 0;
    std::vector<uint8_t> pending; // coverage rows appended below

    struct Placed {
        uint32_t cp;
        int x, y, w, h, xoff, yoff, xadv;
        std::vector<uint8_t> bitmap;
    };
    std::vector<Placed> placed;

    for (uint32_t cp = 32; cp < 0x250; ++cp) {
        int adv = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&m_font, int(cp), &adv, &lsb);
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(&m_font, int(cp), scale, scale, &x0, &y0, &x1, &y1);
        const int w = x1 - x0, h = y1 - y0;
        if (w > 0 && h > 0) {
            if (penX + w + 1 > maxWidth) {
                penX = 0;
                penY += rowHeight + 1;
                rowHeight = 0;
            }
            std::vector<uint8_t> bmp(size_t(w) * size_t(h), 0);
            stbtt_MakeCodepointBitmap(&m_font, bmp.data(), w, h, w, scale, scale, int(cp));
            Placed p{cp, penX, penY, w, h, x0, y0, int(std::lround(adv * scale)), std::move(bmp)};
            placed.push_back(std::move(p));
            penX += w + 1;
            rowHeight = std::max(rowHeight, h);
        } else {
            Glyph g;
            g.w = 0;
            g.h = 0;
            g.xadv = int(std::lround(adv * scale));
            atlas.glyphs[cp] = g;
        }
    }

    atlas.width = maxWidth;
    atlas.height = penY + rowHeight + 1;
    atlas.mask.assign(size_t(atlas.width) * size_t(atlas.height), 0);

    for (const Placed &p : placed) {
        for (int row = 0; row < p.h; ++row) {
            const uint8_t *src = p.bitmap.data() + size_t(row) * size_t(p.w);
            uint8_t *dst = atlas.mask.data() + size_t(p.y + row) * size_t(atlas.width) + size_t(p.x);
            std::copy(src, src + p.w, dst);
        }
        Glyph g;
        g.x = p.x;
        g.y = p.y;
        g.w = p.w;
        g.h = p.h;
        g.xoff = p.xoff;
        g.yoff = p.yoff;
        g.xadv = p.xadv;
        atlas.glyphs[p.cp] = g;
    }
}

int UiText::measure(std::string_view text, int px) const
{
    if (!m_ok) {
        return pf::measure(text, std::max(1, px / 7), 1);
    }
    const Atlas *atlas = atlasFor(px);
    if (!atlas)
        return pf::measure(text, 1, 1);
    int width = 0;
    size_t i = 0;
    while (i < text.size()) {
        const uint32_t cp = nextCodepoint(text, i);
        auto it = atlas->glyphs.find(cp);
        if (it == atlas->glyphs.end())
            it = atlas->glyphs.find('?');
        if (it != atlas->glyphs.end())
            width += it->second.xadv;
    }
    return width;
}

int UiText::lineHeight(int px) const
{
    const Atlas *atlas = atlasFor(px);
    return atlas ? atlas->lineHeight : pf::CH_H * std::max(1, px / 7);
}

void UiText::draw(Canvas &cv, std::string_view text, int x, int y, int px, Color color) const
{
    const Atlas *atlas = atlasFor(px);
    if (!atlas) {
        pf::draw(cv, text, x, y, std::max(1, px / 7), color);
        return;
    }
    const int baseline = y + atlas->ascent;
    int penX = x;
    size_t i = 0;
    while (i < text.size()) {
        const uint32_t cp = nextCodepoint(text, i);
        auto it = atlas->glyphs.find(cp);
        if (it == atlas->glyphs.end())
            it = atlas->glyphs.find('?');
        if (it == atlas->glyphs.end())
            continue;
        const Glyph &g = it->second;
        if (g.w > 0 && g.h > 0) {
            const uint8_t *mask = atlas->mask.data() + size_t(g.y) * size_t(atlas->width) + size_t(g.x);
            cv.blendMask(penX + g.xoff, baseline + g.yoff, g.w, g.h, mask, atlas->width, color);
        }
        penX += g.xadv;
    }
}

void UiText::drawIn(Canvas &cv, std::string_view text, Rect box, int align, int px, Color color) const
{
    const int w = measure(text, px);
    int x = box.x;
    if (align == 1)
        x = box.x + (box.w - w) / 2;
    else if (align == 2)
        x = box.right() - w;
    const int y = box.y + (box.h - lineHeight(px)) / 2;
    draw(cv, text, x, y, px, color);
}
