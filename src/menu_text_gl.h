#pragma once
#include "lcd_overlay.h"
#include "menu_text.h"
#include "ui_theme.h"
#include <cstdio>

namespace menu_text {
inline GLuint texture = 0;
inline size_t texture_generation = 0;
inline void shutdown() {
    if (texture)
        glDeleteTextures(1, &texture);
    texture = 0;
    texture_generation = 0;
}
inline bool upload(const rom_font::Atlas &font) {
    if (!font.valid)
        return false;
    if (texture && texture_generation == rom_font::decodes)
        return true;
    GLint previous;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    if (!texture)
        glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rom_font::Width, rom_font::Height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, font.rgba.data());
    glBindTexture(GL_TEXTURE_2D, GLuint(previous));
    texture_generation = rom_font::decodes;
    return texture != 0;
}
struct Drawn {
    int panels = 0, glyphs = 0, fallback_cells = 0;
};
inline Drawn regions(const uint8_t *tiles, const uint8_t *vram, const uint8_t *rom, size_t rom_size,
                     const uint32_t *framebuffer, const menu_layout::Layout &layout, float width,
                     float height, Placement placement = Placement::World) {
    Drawn drawn;
    const auto &font = rom_font::get(rom, rom_size);
    auto plan = prepare(tiles, vram, font, layout, width, height, ui_theme::Padding,
                        ui_theme::StartGlyphScale, ui_theme::BottomGlyphScale, placement);
    if (!plan.count || !upload(font)) {
        lcd_overlay::regions(framebuffer, layout, width, height);
        return drawn;
    }
    auto *dl = ImGui::GetForegroundDrawList();
    static std::array<bool, 256> reported{};
    for (size_t i = 0; i < plan.count; ++i) {
        auto &p = plan.panels[i];
        if (p.fallback) {
            if (p.rejected_tile >= 0 && !reported[size_t(p.rejected_tile)]) {
                std::fprintf(stderr, "[MENU-TEXT] classic region: tile=%02x region=%zu\n",
                             p.rejected_tile, i);
                reported[size_t(p.rejected_tile)] = true;
            }
            auto r = layout.regions[i];
            lcd_overlay::upload(framebuffer, {r.x, r.y, r.w, r.h});
            continue;
        }
        if (!p.visible)
            continue;
        ++drawn.panels;
        for (int pad = 3; pad >= 1; --pad)
            dl->AddRectFilled({p.left - pad, p.top - pad + 2}, {p.right + pad, p.bottom + pad + 2},
                              ui_theme::Shadow, ui_theme::Radius);
        dl->AddRectFilled({p.left, p.top}, {p.right, p.bottom}, ui_theme::Panel, ui_theme::Radius);
    }
    // Ownership is decided in source coordinates, before changing either scale.
    // Draw each surviving cell once, so overlapping shop/save boxes cannot
    // repeat an obscured glyph in two differently positioned panels.
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 20; ++x) {
            int owner = plan.owner[y * 20 + x];
            if (owner < 0)
                continue;
            const auto &p = plan.panels[size_t(owner)];
            uint8_t tile = tiles[y * 20 + x];
            int cell = 8 * p.scale;
            ImVec2 from{p.origin_x + x * cell, p.origin_y + y * cell};
            ImVec2 to{from.x + cell, from.y + cell};
            if (p.fallback) {
                ++drawn.fallback_cells;
                dl->AddImage((ImTextureID)(intptr_t)lcd_overlay::texture, from, to,
                             {x / 20.f, y / 18.f}, {(x + 1) / 20.f, (y + 1) / 18.f},
                             ui_theme::White);
            } else if (tile >= 0x80) {
                ++drawn.glyphs;
                int glyph = tile - 0x80;
                dl->AddImage((ImTextureID)(intptr_t)texture, from, to,
                             {float(glyph % 16) / 16, float(glyph / 16) / 8},
                             {float(glyph % 16 + 1) / 16, float(glyph / 16 + 1) / 8},
                             ui_theme::Ink);
            }
        }
    return drawn;
}
} // namespace menu_text
