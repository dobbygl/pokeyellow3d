#pragma once
#include "full_menu_layout.h"
#include "pallet3d.h"
#include "rom_font.h"
#include "ui_theme.h"
#include <SDL_opengles2.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace ui_full_menu_qa {
inline size_t frames = 0, glyphs = 0, bits = 0, graphics = 0, fallbacks = 0, cursors = 0;
inline size_t pending_cursors = 0;
inline std::array<size_t, 6> kinds{};
inline void fail(const char *message, int tile = -1, int x = -1, int y = -1) {
    std::fprintf(stderr, "[UI-FULL] FAIL frame=%zu %s tile=%02x at=%d,%d\n", frames, message, tile,
                 x, y);
    std::exit(66);
}
inline void report() {
    std::fprintf(stderr,
                 "[UI-FULL] frames=%zu glyphs=%zu bits=%zu graphics=%zu fallback=%zu "
                 "cursor=%zu pending_cursor=%zu kinds=%zu,%zu,%zu,%zu,%zu,%zu\n",
                 frames, glyphs, bits, graphics, fallbacks, cursors, pending_cursors, kinds[0],
                 kinds[1], kinds[2], kinds[3], kinds[4], kinds[5]);
}
inline void observe(GBContext *ctx, int width, int height) {
    auto shown = pallet3d_full_menu();
    if (!shown.active || pallet3d_blend().active)
        return;
    ++frames;
    if (shown.kind < 0 || shown.kind >= int(kinds.size()))
        fail("invalid disposition identifier");
    ++kinds[size_t(shown.kind)];
    if (shown.current != ctx->wram[0xc26] || shown.scroll != ctx->wram[0xc36] ||
        shown.selected != int(ctx->wram[0xc26]) + ctx->wram[0xc36])
        fail("list selection or scroll differs from the original engine");
    auto layout = full_menu::classify(ctx->wram + 0x3a0, full_menu::Context(shown.context));
    if (int(layout.kind) != shown.kind)
        fail("presentation differs from original window geometry");
    // Compute source ownership and screen coordinates independently of prepare().
    // Every source cell gets one position; no renderer-produced glyph list is used.
    std::array<bool, 360> owned{};
    for (size_t i = 0; i < layout.text.count; ++i) {
        const auto r = layout.text.regions[i];
        for (int y = r.y; y < r.y + r.h; ++y)
            for (int x = r.x; x < r.x + r.w; ++x)
                owned[size_t(y * 20 + x)] = true;
    }
    int scale = std::max(1, int(std::min(width * .75f / 160, height * .75f / 144)));
    int left = (width - 160 * scale) / 2, top = (height - 144 * scale) / 2;
    std::vector<uint8_t> pixels(size_t(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    auto pixel = [&](int x, int y) {
        int sx = left + x * scale + scale / 2, sy = top + y * scale + scale / 2;
        if (sx < 0 || sx >= width || sy < 0 || sy >= height)
            fail("source cell outside viewport", -1, x, y);
        return pixels.data() + (size_t(height - 1 - sy) * width + sx) * 4;
    };
    if (shown.fallback) {
        ++fallbacks;
        const auto *lcd = gb_get_framebuffer(ctx);
        for (int y = 0; y < 144; ++y)
            for (int x = 0; x < 160; ++x) {
                const auto *actual = pixel(x, y);
                uint32_t expected = lcd[y * 160 + x];
                for (int c = 0; c < 3; ++c)
                    if (actual[c] != uint8_t(expected >> (16 - 8 * c)))
                        fail("fallback does not preserve the complete native LCD", -1, x, y);
            }
        return;
    }
    if (layout.kind == full_menu::Kind::Unknown)
        fail("unknown layout was integrated");
    // Count submitted source-font cells independently, in addition to checking
    // the final pixels: identical overlapping glyphs must not mask duplicates.
    std::array<unsigned, 360> coverage{};
    const auto *data = ImGui::GetDrawData();
    if (!data)
        fail("missing full-menu draw data");
    for (int list = 0; list < data->CmdListsCount; ++list) {
        const auto *dl = data->CmdLists[list];
        for (const auto &cmd : dl->CmdBuffer) {
            if (cmd.UserCallback ||
                cmd.GetTexID() != (ImTextureID)(intptr_t)pallet3d_font_texture())
                continue;
            for (unsigned i = 0; i + 5 < cmd.ElemCount; i += 6) {
                const auto &a = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i]];
                const auto &b = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + i + 2]];
                int tx = int(std::lround((a.pos.x - left) / (8 * scale)));
                int ty = int(std::lround((a.pos.y - top) / (8 * scale)));
                if (tx < 0 || tx >= 20 || ty < 0 || ty >= 18 || a.pos.x != left + tx * 8 * scale ||
                    a.pos.y != top + ty * 8 * scale || b.pos.x - a.pos.x != 8 * scale ||
                    b.pos.y - a.pos.y != 8 * scale)
                    continue; // Independent counter labels belong to the PC background.
                int cell = ty * 20 + tx;
                int tile = 128 + int(std::lround(a.uv.y * 8)) * 16 + int(std::lround(a.uv.x * 16));
                if (!owned[size_t(cell)] || tile != ctx->wram[0x3a0 + cell])
                    fail("submitted full-menu cell differs from original tilemap", tile, tx, ty);
                ++coverage[size_t(cell)];
            }
        }
    }
    for (int ty = 0; ty < 18; ++ty)
        for (int tx = 0; tx < 20; ++tx) {
            int tile = ctx->wram[0x3a0 + ty * 20 + tx];
            if (!owned[size_t(ty * 20 + tx)] || (tile >= 0x79 && tile < 0x80))
                continue;
            if (tile >= 0x80 && coverage[size_t(ty * 20 + tx)] != 1)
                fail("original font cell omitted or submitted more than once", tile, tx, ty);
            // Two exceptions independently tied to the original cartridge
            // graphics; compare both bitplanes and all four presented indices.
            size_t source = tile == 0x6e ? 0x10ae0 : tile == 0x78 ? 0x3aa28 : 0;
            if (tile < 0x80 && !source)
                fail("unsupported graphic did not retain LCD", tile, tx, ty);
            for (int y = 0; y < 8; ++y)
                for (int plane = 0; plane < 2; ++plane) {
                    uint8_t expected = source ? ctx->rom[source + y * 2 + plane]
                                              : ctx->rom[0x10600 + (tile - 128) * 8 + y];
                    size_t address = tile < 128 ? 0x1000 + tile * 16 : 0x800 + (tile - 128) * 16;
                    if (ctx->vram[address + y * 2 + plane] != expected)
                        fail("VRAM replacement did not trigger LCD fallback", tile, tx, ty);
                }
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    int index =
                        source ? ((ctx->rom[source + y * 2] >> (7 - x)) & 1) |
                                     (((ctx->rom[source + y * 2 + 1] >> (7 - x)) & 1) << 1)
                               : ((ctx->rom[0x10600 + (tile - 128) * 8 + y] >> (7 - x)) & 1) * 3;
                    constexpr ImU32 colors[]{0, ui_theme::DimInk,
                                             ui_theme::FrameEdge | IM_COL32_A_MASK, ui_theme::Ink};
                    const auto *actual = pixel(tx * 8 + x, ty * 8 + y);
                    if (index) {
                        ImU32 color = colors[index];
                        if (actual[0] != uint8_t(color >> IM_COL32_R_SHIFT) ||
                            actual[1] != uint8_t(color >> IM_COL32_G_SHIFT) ||
                            actual[2] != uint8_t(color >> IM_COL32_B_SHIFT))
                            fail("presented glyph/graphic ink differs from original bits", tile,
                                 tx * 8 + x, ty * 8 + y);
                    } else if (actual[0] == 248 && actual[1] == 244 && actual[2] == 219)
                        fail("unexpected ink in an empty glyph pixel", tile, tx * 8 + x,
                             ty * 8 + y);
                    ++bits;
                }
            source ? ++graphics : ++glyphs;
            int pointer = ctx->wram[0xc30] | (ctx->wram[0xc31] << 8);
            if (tile == 0xed && pointer == 0xc3a0 + ty * 20 + tx) {
                int step = ctx->hram[0x7a] & 2 ? 1 : 2;
                // ChangeBox clears BIT_DOUBLE_SPACED_MENU immediately after
                // HandleMenuInput returns, before erasing the twelve-row list.
                // Its surviving original arrow is still single-spaced.
                bool box_return = layout.kind == full_menu::Kind::PcBoxes && step == 2;
                if (box_return)
                    step = 1;
                int current_y = ctx->wram[0xc24] + step * ctx->wram[0xc26];
                int painted_y = ctx->wram[0xc24] + step * ctx->wram[0xc2a];
                if (tx != ctx->wram[0xc25] || (ty != current_y && ty != painted_y)) {
                    gb_context_save_state_file(ctx, "logs/full-cursor-disagreement.state");
                    fail("cursor differs from original selection and painted item", tile, tx, ty);
                }
                ++cursors;
                pending_cursors += ty != current_y || box_return;
            }
        }
    if (glGetError() != GL_NO_ERROR)
        fail("OpenGL error in full-screen observer");
}
} // namespace ui_full_menu_qa
