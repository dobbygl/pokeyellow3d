#pragma once
#include "menu_layout.h"
#include "rom_font.h"
#include <algorithm>
#include <array>
#include <cmath>

// Read-only presentation of the original tilemap. No strings or menu state are
// reconstructed. Verified against pret/pokeyellow constants/charmap.asm at
// e89ead154b9968aa50eed9328ff2b38b6c194382: cursor ED, down EE, money F0,
// e-acute BA, contractions BB/BD, PK/MN E1/E2, digits F6-FF. LV=6E is extra
// graphics, not FontGraphics, and deliberately falls back to the LCD.
namespace menu_text {
constexpr uint8_t Cursor = 0xed;
struct Panel {
    bool fallback = false, visible = false;
    int rejected_tile = -1;
    int scale = 1;
    float origin_x = 0, origin_y = 0;
    float left = 0, top = 0, right = 0, bottom = 0;
};
struct Plan {
    std::array<int, 360> owner{};
    std::array<Panel, 8> panels{};
    size_t count = 0;
    Plan() {
        owner.fill(-1);
    }
};
inline bool font_matches(const rom_font::Atlas &font, const uint8_t *vram, uint8_t tile) {
    if (!font.valid || !vram || tile < 0x80)
        return false;
    for (int y = 0; y < 8; ++y)
        for (int plane = 0; plane < 2; ++plane)
            if (vram[0x800 + (tile - 128) * 16 + y * 2 + plane] != font.rows[(tile - 128) * 8 + y])
                return false;
    return true;
}
inline bool is_bottom(menu_layout::Rect r) {
    return r.x == 0 && r.y == 12 && r.w == 20 && r.h == 6;
}
inline Plan prepare(const uint8_t *tiles, const uint8_t *vram, const rom_font::Atlas &font,
                    const menu_layout::Layout &layout, float width, float height, float padding,
                    int menu_scale, int bottom_scale) {
    Plan plan;
    if (!tiles || layout.kind != menu_layout::Kind::Partial || layout.count > 8)
        return plan;
    plan.count = layout.count;
    // A shared grid above the dialogue preserves relative placement and window
    // order. Integer scales shrink together on small windows; margins/padding
    // are accounted for before selecting a scale.
    int upper =
        std::max(1, std::min(menu_scale, int(std::floor(std::min((width - 4 * padding) / 160,
                                                                 (height - 6 * padding) / 192)))));
    int lower = std::max(1, std::min(bottom_scale, int(std::floor((width - 4 * padding) / 144))));
    while (lower > 1 && 16 * 8 * upper + 4 * 8 * lower + 6 * padding > height)
        --lower;
    for (size_t i = 0; i < layout.count; ++i) {
        auto r = layout.regions[i];
        if (r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0 || r.x + r.w > 20 || r.y + r.h > 18)
            return Plan{};
        auto &p = plan.panels[i];
        bool bottom = is_bottom(r);
        p.scale = bottom ? lower : upper;
        int cell = p.scale * 8;
        p.origin_x = bottom ? std::floor((width - 20 * cell) / 2)
                            : std::floor(width - 2 * padding - 19 * cell);
        p.origin_y = bottom ? std::floor(height - 2 * padding - 17 * cell) : 2 * padding;
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x)
                plan.owner[y * 20 + x] = int(i);
    }
    // Validate the whole visible region before emitting anything. A single
    // unsupported or replaced glyph retains that region's original LCD pixels.
    for (size_t i = 0; i < layout.count; ++i) {
        auto &p = plan.panels[i];
        int min_x = 20, min_y = 18, max_x = -1, max_y = -1;
        for (int y = 0; y < 18; ++y)
            for (int x = 0; x < 20; ++x) {
                if (plan.owner[y * 20 + x] != int(i))
                    continue;
                uint8_t tile = tiles[y * 20 + x];
                if (tile < 0x79 || (tile >= 0x80 && !font_matches(font, vram, tile))) {
                    p.fallback = true;
                    p.rejected_tile = tile;
                }
                if (tile >= 0x79 && tile <= 0x7e)
                    continue;
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        p.visible = max_x >= min_x;
        if (p.visible) {
            p.left = p.origin_x + min_x * 8 * p.scale - padding;
            p.top = p.origin_y + min_y * 8 * p.scale - padding;
            p.right = p.origin_x + (max_x + 1) * 8 * p.scale + padding;
            p.bottom = p.origin_y + (max_y + 1) * 8 * p.scale + padding;
        }
    }
    return plan;
}
} // namespace menu_text
