#pragma once
#include "pokemon_menu_state.h"
#include "menu_text_gl.h"

namespace pokemon_menu {
inline PalletPokemonMenuInfo shown;
inline mon_pic::Cache portraits;
template <int Width, int Height> struct Texture {
    GLuint id = 0;
    std::array<uint8_t, Width * Height * 4> bytes{};
    bool upload(const std::array<uint8_t, Width * Height * 4> &next) {
        if (id && next == bytes)
            return true;
        GLint previous;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
        if (!id)
            glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     next.data());
        glBindTexture(GL_TEXTURE_2D, GLuint(previous));
        bytes = next;
        return id != 0;
    }
    void reset() {
        if (id)
            glDeleteTextures(1, &id);
        id = 0;
        bytes = {};
    }
};
inline Texture<56, 56> portrait_texture;
inline Texture<128, 16> graphic_texture;
inline Texture<160, 104> icon_texture;
inline void shutdown() {
    portrait_texture.reset();
    graphic_texture.reset();
    icon_texture.reset();
    portraits = {};
    shown = {};
}
inline void rgba(uint8_t *dest, ImU32 color) {
    dest[0] = uint8_t(color >> IM_COL32_R_SHIFT);
    dest[1] = uint8_t(color >> IM_COL32_G_SHIFT);
    dest[2] = uint8_t(color >> IM_COL32_B_SHIFT);
    dest[3] = uint8_t(color >> IM_COL32_A_SHIFT);
}
inline bool upload(const GBContext *ctx, const Snapshot &snapshot) {
    bool summary = snapshot.kind == Kind::Stats || snapshot.kind == Kind::Moves;
    const auto &picture = portraits.get(ctx->rom, ctx->rom_size, snapshot.species, summary);
    if (!portrait_texture.upload(mon_pic::rgba(picture, battle::palette(ctx, snapshot.species))))
        return false;
    constexpr ImU32 colors[]{0, ui_theme::DimInk, ui_theme::FrameEdge | IM_COL32_A_MASK,
                             ui_theme::Ink};
    std::array<uint8_t, 128 * 16 * 4> graphics{};
    for (int tile = 0x62; tile <= 0x78; ++tile) {
        auto source = graphic(snapshot.kind, uint8_t(tile));
        if (!source.address)
            continue;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                size_t row = source.address + y * (source.one_bit ? 1 : 2);
                int a = (ctx->rom[row] >> (7 - x)) & 1;
                int b = (ctx->rom[row + (source.one_bit ? 0 : 1)] >> (7 - x)) & 1;
                int i = tile - 0x62;
                rgba(graphics.data() + ((i / 16 * 8 + y) * 128 + i % 16 * 8 + x) * 4,
                     colors[a | (b << 1)]);
            }
    }
    if (!graphic_texture.upload(graphics))
        return false;
    if (summary)
        return true;
    std::array<uint8_t, 160 * 104 * 4> icons{};
    for (int i = snapshot.count * 4 - 1; i >= 0; --i) {
        const auto *sprite = ctx->oam + i * 4;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                int sx = sprite[1] - 8 + x, sy = sprite[0] - 16 + y;
                int px = (sprite[3] & 0x20) ? 7 - x : x;
                int py = (sprite[3] & 0x40) ? 7 - y : y;
                const auto *row = ctx->vram + sprite[2] * 16 + py * 2;
                int index = ((row[0] >> (7 - px)) & 1) | (((row[1] >> (7 - px)) & 1) << 1);
                if (!index)
                    continue;
                int shade = (ctx->io[(sprite[3] & 0x10) ? 0x49 : 0x48] >> (index * 2)) & 3;
                rgba(icons.data() + (sy * 160 + sx) * 4, shade ? colors[shade] : ui_theme::White);
            }
    }
    return icon_texture.upload(icons);
}
// Returns false when this is not a Pokemon screen. A recognized context with
// any incomplete or unsupported content is drawn atomically as the full LCD.
inline bool draw(GBContext *ctx, int width, int height) {
    auto snapshot = prepare(ctx, portraits);
    if (snapshot.kind == Kind::None)
        return false;
    shown = {
        true, true, int(snapshot.kind), snapshot.selected, snapshot.species, snapshot.bar_count};
    menu_text::prime();
    if (!snapshot.valid || !upload(ctx, snapshot)) {
        lcd_overlay::framed(gb_get_framebuffer(ctx), float(width), float(height));
        return true;
    }
    auto drawn = menu_text::regions(snapshot.text.data(), ctx->vram, ctx->rom, ctx->rom_size,
                                    gb_get_framebuffer(ctx), snapshot.layout, float(width),
                                    float(height), menu_text::Placement::Full);
    if (!drawn.panels)
        return true;
    shown.fallback = false;
    int scale = std::max(1, int(std::min(width * .75f / 160, height * .75f / 144)));
    float left = std::floor((width - 160 * scale) / 2.f);
    float top = std::floor((height - 144 * scale) / 2.f);
    auto *dl = ImGui::GetForegroundDrawList();
    const auto *tiles = ctx->wram + 0x3a0;
    for (int i = 0; i < 360; ++i)
        if (snapshot.cells[i] == Cell::Graphic) {
            int tile = tiles[i] - 0x62;
            float x = left + i % 20 * 8 * scale, y = top + i / 20 * 8 * scale;
            dl->AddImage((ImTextureID)(intptr_t)graphic_texture.id, {x, y},
                         {x + 8 * scale, y + 8 * scale},
                         {float(tile % 16) / 16, float(tile / 16) / 2},
                         {float(tile % 16 + 1) / 16, float(tile / 16 + 1) / 2});
        }
    for (size_t i = 0; i < snapshot.bar_count; ++i) {
        const auto &bar = snapshot.bars[i];
        float x = left + bar.x * 8 * scale, y = top + (bar.y * 8 + 2) * scale;
        dl->AddRectFilled({x, y}, {x + 48 * scale, y + 4 * scale}, ui_theme::BarTrack,
                          ui_theme::BarRadius);
        constexpr ImU32 colors[]{ui_theme::HpGreen, ui_theme::HpAmber, ui_theme::HpRed};
        if (bar.pixels)
            dl->AddRectFilled({x, y}, {x + bar.pixels * scale, y + 4 * scale}, colors[bar.color],
                              ui_theme::BarRadius);
    }
    bool summary = snapshot.kind == Kind::Stats || snapshot.kind == Kind::Moves;
    if (summary) {
        if (snapshot.portrait)
            dl->AddImage((ImTextureID)(intptr_t)portrait_texture.id, {left + 8 * scale, top},
                         {left + 64 * scale, top + 56 * scale});
        if (snapshot.kind == Kind::Moves) {
            // Original page two leaves row two empty above EXP POINTS.
            float x = left + 72 * scale, y = top + 18 * scale;
            dl->AddRectFilled({x, y}, {x + 80 * scale, y + 3 * scale}, ui_theme::BarTrack,
                              ui_theme::BarRadius);
            if (snapshot.experience_fraction > 0)
                dl->AddRectFilled(
                    {x, y},
                    {x + std::floor(80 * scale * snapshot.experience_fraction), y + 3 * scale},
                    ui_theme::Experience, ui_theme::BarRadius);
        }
    } else {
        dl->AddImage((ImTextureID)(intptr_t)icon_texture.id, {left, top},
                     {left + 160 * scale, top + 104 * scale});
        int portrait_scale = std::min(scale, int((left - 24) / 56));
        if (portrait_scale > 0) {
            float size = float(56 * portrait_scale), x = std::floor((left - size) / 2);
            dl->AddRectFilled({x - 8, top - 8}, {x + size + 8, top + size + 8}, ui_theme::Panel,
                              ui_theme::Radius);
            dl->AddImage((ImTextureID)(intptr_t)portrait_texture.id, {x, top},
                         {x + size, top + size});
        }
    }
    return true;
}
} // namespace pokemon_menu
