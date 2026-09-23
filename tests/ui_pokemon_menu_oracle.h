#pragma once
#include "pallet3d.h"
#include "ui_theme.h"
#include "imgui.h"
#include <SDL_opengles2.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

// Independent of pokemon_menu::prepare: original cells, ROM bitplanes and
// submitted GL quads are checked against the final surface on every frame.
namespace ui_pokemon_qa {
inline size_t frames = 0, glyphs = 0, bits = 0, cursors = 0, fallbacks = 0;
inline size_t graphic_bits = 0, hp_bars = 0;
inline std::array<size_t, 5> kinds{};
inline void fail(const char *message, int x = -1, int y = -1) {
    std::fprintf(stderr, "[UI-POKEMON] FAIL frame=%zu %s at=%d,%d\n", frames, message, x, y);
    std::exit(67);
}
inline void report() {
    std::fprintf(stderr,
                 "[UI-POKEMON] frames=%zu glyphs=%zu bits=%zu cursors=%zu fallback=%zu "
                 "party=%zu actions=%zu stats=%zu moves=%zu\n",
                 frames, glyphs, bits, cursors, fallbacks, kinds[1], kinds[2], kinds[3], kinds[4]);
    std::fprintf(stderr, "[UI-POKEMON-GRAPHICS] bits=%zu hp_bars=%zu\n", graphic_bits, hp_bars);
}
inline void observe(GBContext *ctx, int width, int height) {
    auto shown = pallet3d_pokemon_menu();
    if (!shown.active || pallet3d_blend().active)
        return;
    if (shown.kind < 1 || shown.kind > 4)
        fail("unknown disposition");
    ++frames;
    ++kinds[size_t(shown.kind)];
    int scale = std::max(1, int(std::min(width * .75f / 160, height * .75f / 144)));
    int left = (width - 160 * scale) / 2, top = (height - 144 * scale) / 2;
    std::vector<uint8_t> pixels(size_t(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    auto pixel = [&](int x, int y) {
        int sx = left + x * scale + scale / 2, sy = top + y * scale + scale / 2;
        if (sx < 0 || sx >= width || sy < 0 || sy >= height)
            fail("content outside viewport", x, y);
        return pixels.data() + (size_t(height - 1 - sy) * width + sx) * 4;
    };
    if (shown.fallback) {
        ++fallbacks;
        const auto *lcd = gb_get_framebuffer(ctx);
        for (int y = 0; y < 144; ++y)
            for (int x = 0; x < 160; ++x)
                for (int c = 0; c < 3; ++c)
                    if (pixel(x, y)[c] != uint8_t(lcd[y * 160 + x] >> (16 - 8 * c)))
                        fail("fallback differs from complete original LCD", x, y);
        return;
    }
    if (shown.kind <= 2) {
        int selected = ctx->wram[shown.kind == 1 ? 0xc26 : 0xc2b];
        bool active = false;
        int chosen = -1, unfilled = 0;
        for (int row = 0; row < ctx->wram[0x1162]; ++row) {
            uint8_t arrow = ctx->wram[0x3a0 + (row * 2 + 1) * 20];
            if (arrow == (shown.kind == 1 ? 0xed : 0xec)) {
                selected = row;
                active = true;
                break;
            }
            if (arrow == 0xec) {
                chosen = row;
                ++unfilled;
            }
        }
        // A battle message over a party choice (already out, no will to fight)
        // keeps only the unfilled arrow that marks the chosen row.
        if (shown.kind == 1 && !active && unfilled == 1)
            selected = chosen;
        if (shown.selected != selected || selected >= 6 ||
            shown.species != ctx->wram[0x116a + selected * 44])
            fail("portrait does not follow the visibly selected original party row");
    }
    std::array<unsigned, 360> coverage{};
    const auto *data = ImGui::GetDrawData();
    if (!data)
        fail("missing draw data");
    for (int list = 0; list < data->CmdListsCount; ++list) {
        const auto *dl = data->CmdLists[list];
        for (const auto &cmd : dl->CmdBuffer) {
            if (cmd.UserCallback ||
                cmd.GetTexID() != (ImTextureID)(intptr_t)pallet3d_font_texture())
                continue;
            for (unsigned i = 0; i + 5 < cmd.ElemCount; i += 6) {
                const auto &a = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i]];
                const auto &b = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 2]];
                int x = int(std::lround((a.pos.x - left) / (8 * scale)));
                int y = int(std::lround((a.pos.y - top) / (8 * scale)));
                if (x < 0 || x >= 20 || y < 0 || y >= 18 || a.pos.x != left + x * 8 * scale ||
                    a.pos.y != top + y * 8 * scale || b.pos.x - a.pos.x != 8 * scale ||
                    b.pos.y - a.pos.y != 8 * scale)
                    continue;
                int tile = 128 + int(std::lround(a.uv.y * 8)) * 16 + int(std::lround(a.uv.x * 16));
                if (tile != ctx->wram[0x3a0 + y * 20 + x])
                    fail("submitted glyph differs from original cell", x, y);
                ++coverage[size_t(y * 20 + x)];
            }
        }
    }
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 20; ++x) {
            int tile = ctx->wram[0x3a0 + y * 20 + x];
            if (tile < 128) {
                bool summary = shown.kind == 3 || shown.kind == 4;
                bool graphic = summary
                                   ? (tile == 0x70 || tile == 0x71 || tile == 0x62 ||
                                      tile == 0x6e || tile == 0x72 || tile == 0x73 || tile == 0x74)
                                   : (tile == 0x71 || tile == 0x62 || tile == 0x6e);
                if (graphic) {
                    size_t source = 0x10a20 + size_t(tile - 0x62) * 16;
                    bool one = false;
                    if (summary && tile == 0x6e) {
                        source = 0x10c08;
                        one = true;
                    }
                    if (summary && tile == 0x72) {
                        source = 0x11682;
                        one = true;
                    }
                    for (int py = 0; py < 8; ++py)
                        for (int px = 0; px < 8; ++px) {
                            size_t row = source + py * (one ? 1 : 2);
                            int index = ((ctx->rom[row] >> (7 - px)) & 1) |
                                        (((ctx->rom[row + (one ? 0 : 1)] >> (7 - px)) & 1) << 1);
                            constexpr ImU32 colors[]{0, ui_theme::DimInk,
                                                     ui_theme::FrameEdge | IM_COL32_A_MASK,
                                                     ui_theme::Ink};
                            const auto *p = pixel(x * 8 + px, y * 8 + py);
                            auto color = colors[index];
                            if (index && (p[0] != uint8_t(color >> IM_COL32_R_SHIFT) ||
                                          p[1] != uint8_t(color >> IM_COL32_G_SHIFT) ||
                                          p[2] != uint8_t(color >> IM_COL32_B_SHIFT)))
                                fail("graphic differs from original ROM bitplanes", x, y);
                            ++graphic_bits;
                        }
                }
                if (tile == 0x62 && (shown.kind <= 2 || shown.kind == 3)) {
                    int cap = ctx->wram[0x3a0 + y * 20 + x + 7];
                    // A taller action window can cover a partial party bar;
                    // its remaining original graphic cells stay visible.
                    if (cap == (shown.kind == 3 ? 0x6d : 0x6c)) {
                        int fill = 0;
                        for (int i = 1; i <= 6; ++i)
                            fill += ctx->wram[0x3a0 + y * 20 + x + i] - 0x63;
                        int color = ctx->wram[shown.kind == 3 ? 0xf24 : 0xf1e + y / 2];
                        constexpr ImU32 colors[]{ui_theme::HpGreen, ui_theme::HpAmber,
                                                 ui_theme::HpRed};
                        if (fill < 0 || fill > 48 || color > 2)
                            fail("invalid painted HP bar was integrated", x, y);
                        for (int px = 1; px < 47; ++px) {
                            const auto *p = pixel((x + 1) * 8 + px, y * 8 + 4);
                            auto expected = px < fill ? colors[color] : ui_theme::BarTrack;
                            if (p[0] != uint8_t(expected >> IM_COL32_R_SHIFT) ||
                                p[1] != uint8_t(expected >> IM_COL32_G_SHIFT) ||
                                p[2] != uint8_t(expected >> IM_COL32_B_SHIFT))
                                fail("HUD HP fill or color differs from original painted bar", x,
                                     y);
                        }
                        ++hp_bars;
                    }
                }
                continue;
            }
            if (coverage[size_t(y * 20 + x)] != 1)
                fail("original glyph omitted or duplicated", x, y);
            ++glyphs;
            for (int py = 0; py < 8; ++py) {
                uint8_t row = ctx->rom[0x10600 + (tile - 128) * 8 + py];
                if (ctx->vram[0x800 + (tile - 128) * 16 + py * 2] != row ||
                    ctx->vram[0x801 + (tile - 128) * 16 + py * 2] != row)
                    fail("replaced font failed to retain LCD", x, y);
                for (int px = 0; px < 8; ++px) {
                    const auto *p = pixel(x * 8 + px, y * 8 + py);
                    bool ink = p[0] == 248 && p[1] == 244 && p[2] == 219;
                    if (ink != bool(row & (0x80 >> px)))
                        fail("presented font bit differs from ROM", x * 8 + px, y * 8 + py);
                    ++bits;
                }
            }
            int pointer = ctx->wram[0xc30] | (ctx->wram[0xc31] << 8);
            if (tile == 0xed && pointer == 0xc3a0 + y * 20 + x) {
                int current_y = ctx->wram[0xc24] + 2 * ctx->wram[0xc26];
                int previous_y = ctx->wram[0xc24] + 2 * ctx->wram[0xc2a];
                if (x != ctx->wram[0xc25] || (y != current_y && y != previous_y))
                    fail("arrow differs from current and painted original selection", x, y);
                ++cursors;
            }
        }
}
} // namespace ui_pokemon_qa
