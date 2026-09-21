#pragma once
#include "menu_text.h"

// Only promote a complete known LCD region when every displayed pixel still
// matches its original ROM glyph. wTileMap can lead the LCD by several frames.
namespace rom_text_region {
inline bool ready(const uint8_t *tiles, const uint8_t *vram, const rom_font::Atlas &font,
                  const uint32_t *lcd, menu_layout::Rect r) {
    if (!tiles || !vram || !lcd || !font.valid || r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0 ||
        r.x + r.w > 20 || r.y + r.h > 18)
        return false;
    bool seen[2]{};
    uint32_t colors[2]{};
    for (int ty = r.y; ty < r.y + r.h; ++ty)
        for (int tx = r.x; tx < r.x + r.w; ++tx) {
            uint8_t tile = tiles[ty * 20 + tx];
            if (tile < 0x7f || (tile >= 0x80 && !menu_text::font_matches(font, vram, tile)))
                return false;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    int bit = font.ink(tile, x, y);
                    uint32_t color = lcd[(ty * 8 + y) * 160 + tx * 8 + x] & 0xffffff;
                    if (seen[bit] && colors[bit] != color)
                        return false;
                    seen[bit] = true;
                    colors[bit] = color;
                }
        }
    return seen[0] && seen[1] && colors[0] != colors[1];
}
} // namespace rom_text_region
