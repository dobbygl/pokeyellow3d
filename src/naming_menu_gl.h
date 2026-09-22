#pragma once
#include "naming_menu_state.h"
#include "pokemon_menu_gl.h"

namespace naming_menu {
inline pokemon_menu::Texture<160, 144> graphics;
inline bool draw(GBContext *ctx, const Snapshot &snapshot, const menu_layout::Layout &layout,
                 int width, int height) {
    const auto &font = rom_font::get(ctx->rom, ctx->rom_size);
    auto plan = menu_text::prepare(snapshot.text.data(), ctx->vram, font, layout, float(width),
                                   float(height), ui_theme::Padding, ui_theme::StartGlyphScale,
                                   ui_theme::BottomGlyphScale, menu_text::Placement::Full, ctx->rom,
                                   ctx->rom_size);
    if (!snapshot.ready || !plan.count ||
        std::any_of(plan.panels.begin(), plan.panels.begin() + plan.count,
                    [](const menu_text::Panel &p) { return p.fallback; })) {
        lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
        return false;
    }
    std::array<uint8_t, 160 * 144 * 4> rgba{};
    constexpr ImU32 colors[]{0, ui_theme::DimInk, ui_theme::FrameEdge | IM_COL32_A_MASK,
                             ui_theme::Ink};
    for (int cell = 0; cell < 360; ++cell) {
        if (!snapshot.graphics[size_t(cell)])
            continue;
        int tx = cell % 20, ty = cell / 20, tile = ctx->wram[0x3a0 + cell];
        size_t base = tile == 0xf0 ? 0xf00 : 0x1000 + tile * 16;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                int value = ((ctx->vram[base + y * 2] >> (7 - x)) & 1) |
                            (((ctx->vram[base + y * 2 + 1] >> (7 - x)) & 1) << 1);
                pokemon_menu::rgba(rgba.data() + ((ty * 8 + y) * 160 + tx * 8 + x) * 4,
                                   colors[value]);
            }
    }
    if (snapshot.type == 2)
        for (int i = 3; i >= 0; --i) {
            const auto *sprite = ctx->oam + i * 4;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    int sx = sprite[1] - 8 + x, sy = sprite[0] - 16 + y;
                    int px = sprite[3] & 0x20 ? 7 - x : x;
                    int py = sprite[3] & 0x40 ? 7 - y : y;
                    const auto *row = ctx->vram + sprite[2] * 16 + py * 2;
                    int index = ((row[0] >> (7 - px)) & 1) | (((row[1] >> (7 - px)) & 1) << 1);
                    if (!index)
                        continue;
                    int shade = (ctx->io[sprite[3] & 0x10 ? 0x49 : 0x48] >> (index * 2)) & 3;
                    pokemon_menu::rgba(rgba.data() + (sy * 160 + sx) * 4,
                                       shade ? colors[shade] : ui_theme::White);
                }
        }
    if (!graphics.upload(rgba) || !menu_text::upload(font) ||
        !menu_text::upload_graphics(ctx->rom, ctx->rom_size)) {
        lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
        return false;
    }
    int scale = lcd_overlay::framed_scale(float(width), float(height));
    float left = std::floor((width - 160 * scale) / 2.f);
    float top = std::floor((height - 144 * scale) / 2.f);
    auto *dl = ImGui::GetForegroundDrawList();
    // Key backgrounds follow the original cells; glyph and arrow coordinates
    // remain unchanged. Shared animated panels occupy the background layer.
    for (int row = 0; row < 6; ++row)
        for (int col = 0; col < (row == 5 ? 1 : 9); ++col) {
            int x = 1 + col * 2, y = 5 + row * 2;
            float sx = left + x * 8 * scale, sy = top + y * 8 * scale;
            float w = float((row == 5 ? 11 : 2) * 8 * scale);
            dl->AddRectFilled({sx, sy - 2 * scale}, {sx + w, sy + 9 * scale}, ui_theme::BarTrack,
                              ui_theme::CompactRadius);
            dl->AddRect({sx, sy - 2 * scale}, {sx + w, sy + 9 * scale},
                        x == snapshot.cursor_x && y == snapshot.cursor_y ? ui_theme::Accent
                                                                         : ui_theme::FrameEdge,
                        ui_theme::CompactRadius);
        }
    auto drawn = menu_text::regions(snapshot.text.data(), ctx->vram, ctx->rom, ctx->rom_size,
                                    gb_get_framebuffer(ctx), layout, float(width), float(height),
                                    menu_text::Placement::Full, &plan);
    if (!drawn.panels)
        return false;
    dl->AddImage((ImTextureID)(intptr_t)graphics.id, {left, top},
                 {left + 160 * scale, top + 144 * scale});
    return true;
}
} // namespace naming_menu
