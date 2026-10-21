/* ------------------------------------------------------------------
   Linamp — system UI font (TrueType via stb_truetype).

   The player itself keeps the 5x7 bitmap font (that is the Winamp skin
   look), but menus, help and tooltips use the system UI font like the
   original app did — mixed case, antialiased, readable at 10-12 px.

   No font file is bundled: a font is picked from the usual system paths
   and if none is found the bitmaps in font.h are used as a fallback.
   ------------------------------------------------------------------ */
#pragma once

#include "gfx.h"

#include "third_party/stb_truetype.h"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class UiText {
public:
    /* Tries $LINAMP_UI_FONT and then the usual per-OS font paths. */
    bool load();

    bool ok() const { return m_ok; }
    const std::string &fontPath() const { return m_path; }

    /* Antialiased text; falls back to the pixel font when no TTF is loaded.
       `y` is the top of the line box (baseline is y + ascent). */
    void draw(Canvas &cv, std::string_view text, int x, int y, int px, Color color) const;
    /* Draws inside a box: vertically centred, horizontally aligned (0/1/2). */
    void drawIn(Canvas &cv, std::string_view text, Rect box, int align, int px, Color color) const;
    /* Width in pixels, and a vertical offset that centres a line of `px`. */
    int measure(std::string_view text, int px) const;
    int lineHeight(int px) const;

private:
    struct Glyph {
        int x = 0, y = 0, w = 0, h = 0;
        int xoff = 0, yoff = 0, xadv = 0;
    };
    struct Atlas {
        int px = 0;
        int height = 0;     // mask height (packing extent)
        int lineHeight = 0; // typographic line height
        int ascent = 0;
        int width = 0;
        std::vector<uint8_t> mask;   // width*height coverage
        std::map<uint32_t, Glyph> glyphs;
    };

    const Atlas *atlasFor(int px) const;
    void buildAtlas(Atlas &atlas, int px) const;

    bool m_ok = false;
    std::string m_path;
    std::vector<uint8_t> m_data;
    mutable std::map<int, Atlas> m_atlases;
    stbtt_fontinfo m_font{};
};
