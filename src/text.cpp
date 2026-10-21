#define STB_TRUETYPE_IMPLEMENTATION
#include "text.h"

#include "font.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

/* pow(cov/255, 0.78) as a lookup table */
const uint8_t COVERAGE_LUT[256] = {
#define COV(n) uint8_t(std::lround(std::pow(n / 255.0, 0.78) * 255.0))
    COV(0), COV(1), COV(2), COV(3), COV(4), COV(5), COV(6), COV(7), COV(8), COV(9), COV(10),
    COV(11), COV(12), COV(13), COV(14), COV(15), COV(16), COV(17), COV(18), COV(19), COV(20),
    COV(21), COV(22), COV(23), COV(24), COV(25), COV(26), COV(27), COV(28), COV(29), COV(30),
    COV(31), COV(32), COV(33), COV(34), COV(35), COV(36), COV(37), COV(38), COV(39), COV(40),
    COV(41), COV(42), COV(43), COV(44), COV(45), COV(46), COV(47), COV(48), COV(49), COV(50),
    COV(51), COV(52), COV(53), COV(54), COV(55), COV(56), COV(57), COV(58), COV(59), COV(60),
    COV(61), COV(62), COV(63), COV(64), COV(65), COV(66), COV(67), COV(68), COV(69), COV(70),
    COV(71), COV(72), COV(73), COV(74), COV(75), COV(76), COV(77), COV(78), COV(79), COV(80),
    COV(81), COV(82), COV(83), COV(84), COV(85), COV(86), COV(87), COV(88), COV(89), COV(90),
    COV(91), COV(92), COV(93), COV(94), COV(95), COV(96), COV(97), COV(98), COV(99), COV(100),
    COV(101), COV(102), COV(103), COV(104), COV(105), COV(106), COV(107), COV(108), COV(109),
    COV(110), COV(111), COV(112), COV(113), COV(114), COV(115), COV(116), COV(117), COV(118),
    COV(119), COV(120), COV(121), COV(122), COV(123), COV(124), COV(125), COV(126), COV(127),
    COV(128), COV(129), COV(130), COV(131), COV(132), COV(133), COV(134), COV(135), COV(136),
    COV(137), COV(138), COV(139), COV(140), COV(141), COV(142), COV(143), COV(144), COV(145),
    COV(146), COV(147), COV(148), COV(149), COV(150), COV(151), COV(152), COV(153), COV(154),
    COV(155), COV(156), COV(157), COV(158), COV(159), COV(160), COV(161), COV(162), COV(163),
    COV(164), COV(165), COV(166), COV(167), COV(168), COV(169), COV(170), COV(171), COV(172),
    COV(173), COV(174), COV(175), COV(176), COV(177), COV(178), COV(179), COV(180), COV(181),
    COV(182), COV(183), COV(184), COV(185), COV(186), COV(187), COV(188), COV(189), COV(190),
    COV(191), COV(192), COV(193), COV(194), COV(195), COV(196), COV(197), COV(198), COV(199),
    COV(200), COV(201), COV(202), COV(203), COV(204), COV(205), COV(206), COV(207), COV(208),
    COV(209), COV(210), COV(211), COV(212), COV(213), COV(214), COV(215), COV(216), COV(217),
    COV(218), COV(219), COV(220), COV(221), COV(222), COV(223), COV(224), COV(225), COV(226),
    COV(227), COV(228), COV(229), COV(230), COV(231), COV(232), COV(233), COV(234), COV(235),
    COV(236), COV(237), COV(238), COV(239), COV(240), COV(241), COV(242), COV(243), COV(244),
    COV(245), COV(246), COV(247), COV(248), COV(249), COV(250), COV(251), COV(252), COV(253),
    COV(254), COV(255),
#undef COV
};

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
            // Light-on-dark text looks thin with plain coverage; a mild gamma
            // makes the antialiased edges read as sharp instead of soft.
            for (uint8_t &cov : bmp)
                if (cov)
                    cov = COVERAGE_LUT[cov];
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
    const int pxDev = std::max(6, int(std::lround(double(px) * double(m_pxScale))));
    const Atlas *atlas = atlasFor(pxDev);
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
    return int(std::lround(double(width) / double(m_pxScale)));
}

int UiText::lineHeight(int px) const
{
    const int pxDev = std::max(6, int(std::lround(double(px) * double(m_pxScale))));
    const Atlas *atlas = atlasFor(pxDev);
    if (!atlas)
        return pf::CH_H * std::max(1, px / 7);
    return std::max(1, int(std::lround(double(atlas->lineHeight) / double(m_pxScale))));
}

void UiText::draw(Canvas &cv, std::string_view text, int x, int y, int px, Color color) const
{
    // Rasterise at the device resolution so fractional HiDPI stays sharp.
    const float s = cv.scaleX();
    const int pxDev = std::max(6, int(std::lround(double(px) * double(s))));
    const Atlas *atlas = atlasFor(pxDev);
    if (!atlas) {
        pf::draw(cv, text, x, y, std::max(1, px / 7), color);
        return;
    }
    const int baseline = cv.toDeviceY(float(y)) + atlas->ascent;
    int penX = cv.toDeviceX(float(x));
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
