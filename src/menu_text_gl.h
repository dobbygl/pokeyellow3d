#pragma once
#include "lcd_overlay.h"
#include "menu_text.h"
#include "menu_motion.h"
#include "ui_theme.h"
#include <cstdio>

namespace menu_text {
inline GLuint texture = 0;
inline GLuint graphics_texture = 0;
inline std::array<uint8_t, 32> graphics_source{};
inline size_t texture_generation = 0;
inline bool motion_frame = false, motion_eligible = false, motion_suppressed = false;
inline void begin_frame(uint32_t cycles, bool paused, bool integrated) {
    motion_frame = integrated;
    motion_eligible = false;
    motion_suppressed = false;
    if (integrated)
        menu_motion::state.begin(cycles, paused, menu_motion::enabled);
    else
        menu_motion::state.reset();
}
inline void prime() {
    motion_eligible = true;
}
inline void finish_frame(bool visible) {
    if (!motion_frame)
        return;
    auto &motion = menu_motion::state;
    motion.finish(motion_eligible);
    if (!visible || motion_suppressed)
        return;
    // ImGui draws this list before every native LCD and glyph foreground quad.
    // A closing panel retains decoration only, never the previous menu's text.
    auto *dl = ImGui::GetBackgroundDrawList();
    for (const auto &panel : motion.panels) {
        float alpha = panel.opacity(), dy = std::round(panel.offset());
        if (!panel.visible || alpha <= 0)
            continue;
        auto b = panel.bounds;
        for (int pad = 3; pad >= 1; --pad)
            dl->AddRectFilled({b.left - pad, b.top - pad + 2 + dy},
                              {b.right + pad, b.bottom + pad + 2 + dy},
                              ui_theme::opacity(ui_theme::Shadow, alpha), ui_theme::Radius);
        dl->AddRectFilled({b.left, b.top + dy}, {b.right, b.bottom + dy},
                          ui_theme::opacity(ui_theme::Panel, alpha), ui_theme::Radius);
    }
}
struct FrameEnd {
    const bool &scene_active, &scene_failed;
    ~FrameEnd() {
        finish_frame(scene_active && !scene_failed);
    }
};
inline void shutdown() {
    if (texture)
        glDeleteTextures(1, &texture);
    if (graphics_texture)
        glDeleteTextures(1, &graphics_texture);
    graphics_texture = 0;
    graphics_source = {};
    texture = 0;
    texture_generation = 0;
    menu_motion::state.reset();
    motion_frame = motion_eligible = false;
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
inline bool upload_graphics(const uint8_t *rom, size_t size) {
    if (!rom || size != 1048576)
        return false;
    std::array<uint8_t, 32> source{};
    std::copy_n(rom + menu_graphics::LevelOffset, 16, source.begin());
    std::copy_n(rom + menu_graphics::OccupiedBoxOffset, 16, source.begin() + 16);
    if (graphics_texture && source == graphics_source)
        return true;
    std::array<uint8_t, 16 * 8 * 4> rgba{};
    // Preserve all four original 2bpp indices. The level glyph uses only 0/3;
    // the occupied-box pictogram also uses the two intermediate theme tones.
    constexpr ImU32 colors[]{0, ui_theme::DimInk, ui_theme::FrameEdge | IM_COL32_A_MASK,
                             ui_theme::Ink};
    for (int image = 0; image < 2; ++image)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                uint8_t tile = image ? menu_graphics::OccupiedBox : menu_graphics::Level;
                ImU32 color = colors[menu_graphics::pixel(rom, tile, x, y)];
                size_t dest = (size_t(y) * 16 + image * 8 + x) * 4;
                rgba[dest] = uint8_t(color >> IM_COL32_R_SHIFT);
                rgba[dest + 1] = uint8_t(color >> IM_COL32_G_SHIFT);
                rgba[dest + 2] = uint8_t(color >> IM_COL32_B_SHIFT);
                rgba[dest + 3] = uint8_t(color >> IM_COL32_A_SHIFT);
            }
    GLint previous;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
    if (!graphics_texture)
        glGenTextures(1, &graphics_texture);
    glBindTexture(GL_TEXTURE_2D, graphics_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glBindTexture(GL_TEXTURE_2D, GLuint(previous));
    graphics_source = source;
    return graphics_texture != 0;
}
inline Drawn regions(const uint8_t *tiles, const uint8_t *vram, const uint8_t *rom, size_t rom_size,
                     const uint32_t *framebuffer, const menu_layout::Layout &layout, float width,
                     float height, Placement placement = Placement::World) {
    Drawn drawn;
    const auto &font = rom_font::get(rom, rom_size);
    auto plan =
        prepare(tiles, vram, font, layout, width, height, ui_theme::Padding,
                ui_theme::StartGlyphScale, ui_theme::BottomGlyphScale, placement, rom, rom_size);
    if (placement == Placement::Full &&
        (!plan.count || !upload(font) || !upload_graphics(rom, rom_size) ||
         std::any_of(plan.panels.begin(), plan.panels.begin() + plan.count,
                     [](const Panel &panel) { return panel.fallback; }))) {
        lcd_overlay::framed(framebuffer, width, height);
        return drawn;
    }
    if (!plan.count || !upload(font)) {
        // An empty/unrecognized frame still retires the previous decoration
        // through finish_frame(). The native LCD remains on top of it.
        lcd_overlay::regions(framebuffer, layout, width, height);
        return drawn;
    }
    auto *dl = ImGui::GetForegroundDrawList();
    static std::array<bool, 256> reported{};
    prime();
    for (size_t i = 0; i < plan.count; ++i) {
        auto &p = plan.panels[i];
        menu_motion::Key key{layout.regions[i], int(placement)};
        if (p.fallback || !p.visible)
            menu_motion::state.hide(key);
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
        if (motion_frame) {
            menu_motion::state.submit(key, {p.left, p.top, p.right, p.bottom});
            continue;
        }
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
            } else if (placement == Placement::Full && menu_graphics::offset(tile)) {
                float u = tile == menu_graphics::Level ? 0.f : .5f;
                dl->AddImage((ImTextureID)(intptr_t)graphics_texture, from, to, {u, 0},
                             {u + .5f, 1}, ui_theme::White);
            }
        }
    return drawn;
}
} // namespace menu_text
