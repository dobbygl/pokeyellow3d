#pragma once
#include "pallet3d.h"
#include "imgui.h"
#include "ui_theme.h"
#include <SDL_opengles2.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace ui_dex_qa {
inline size_t frames = 0, glyphs = 0, bits = 0, graphics = 0, cursors = 0, fallbacks = 0;
inline void fail(const char *message, int x = -1, int y = -1) {
    std::fprintf(stderr, "[UI-DEX] FAIL frame=%zu %s at=%d,%d\n", frames, message, x, y);
    std::exit(68);
}
inline void report() {
    std::fprintf(stderr,
                 "[UI-DEX] frames=%zu glyphs=%zu bits=%zu graphics=%zu cursor=%zu fallback=%zu\n",
                 frames, glyphs, bits, graphics, cursors, fallbacks);
}
inline void observe(GBContext *ctx, int w, int h) {
    auto info = pallet3d_dex_menu();
    if (!info.active || pallet3d_blend().active)
        return;
    ++frames;
    std::vector<uint8_t> pixels(size_t(w) * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    auto pixel = [&](int x, int y) {
        if (x < 0 || x >= w || y < 0 || y >= h)
            fail("cell outside viewport", x, y);
        return pixels.data() + (size_t(h - 1 - y) * w + x) * 4;
    };
    if (info.fallback) {
        ++fallbacks;
        int s = std::max(1, int(std::min(w * .75f / 160, h * .75f / 144)));
        int left = (w - 160 * s) / 2, top = (h - 144 * s) / 2;
        const auto *lcd = gb_get_framebuffer(ctx);
        for (int y = 0; y < 144; ++y)
            for (int x = 0; x < 160; ++x) {
                auto actual = pixel(left + x * s + s / 2, top + y * s + s / 2);
                for (int c = 0; c < 3; ++c)
                    if (actual[c] != uint8_t(lcd[y * 160 + x] >> (16 - 8 * c)))
                        fail("fallback differs from complete original LCD", x, y);
            }
        return;
    }
    // Independent source rectangles: no renderer plan or normalized text is read.
    constexpr int regions[][4]{{0, 0, 14, 18}, {15, 8, 5, 9}, {16, 1, 4, 2}, {16, 4, 4, 2}};
    std::array<int, 360> coverage{};
    auto *data = ImGui::GetDrawData();
    if (!data)
        fail("no submitted draw data");
    for (int list = 0; list < data->CmdListsCount; ++list) {
        const auto *dl = data->CmdLists[list];
        for (const auto &cmd : dl->CmdBuffer) {
            if (cmd.UserCallback ||
                cmd.GetTexID() != (ImTextureID)(intptr_t)pallet3d_font_texture())
                continue;
            for (unsigned index = 0; index + 5 < cmd.ElemCount; index += 6) {
                const auto &a = dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + index]];
                const auto &b =
                    dl->VtxBuffer[cmd.VtxOffset + dl->IdxBuffer[cmd.IdxOffset + index + 2]];
                bool found = false;
                for (size_t r = 0; r < 4; ++r) {
                    int cell = info.scale[r] * 8;
                    if (!cell)
                        fail("zero font scale");
                    int x = int(std::lround((a.pos.x - info.left[r]) / cell));
                    int y = int(std::lround((a.pos.y - info.top[r]) / cell));
                    if (x < 0 || y < 0 || x >= regions[r][2] || y >= regions[r][3] ||
                        a.pos.x != info.left[r] + x * cell || a.pos.y != info.top[r] + y * cell ||
                        b.pos.x - a.pos.x != cell || b.pos.y - a.pos.y != cell)
                        continue;
                    x += regions[r][0];
                    y += regions[r][1];
                    int tile =
                        128 + int(std::lround(a.uv.y * 8)) * 16 + int(std::lround(a.uv.x * 16));
                    if (tile != ctx->wram[0x3a0 + y * 20 + x] || found)
                        fail("submitted glyph differs from source or belongs to two regions", x, y);
                    ++coverage[size_t(y * 20 + x)];
                    found = true;
                }
                if (!found)
                    fail("unexpected source-font quad outside device regions");
            }
        }
    }
    for (size_t r = 0; r < 4; ++r)
        for (int y = 0; y < regions[r][3]; ++y)
            for (int x = 0; x < regions[r][2]; ++x) {
                int tx = regions[r][0] + x, ty = regions[r][1] + y;
                int tile = ctx->wram[0x3a0 + ty * 20 + tx];
                if (tile == 0x7f)
                    continue;
                bool caught = tile == 0x72 && tx == 3 && ty >= 3 && ty <= 15 && ty % 2;
                if ((!caught && tile < 128) || (!caught && coverage[size_t(ty * 20 + tx)] != 1))
                    fail("source cell omitted, duplicated or unsupported", tx, ty);
                for (int py = 0; py < 8; ++py)
                    for (int px = 0; px < 8; ++px) {
                        size_t rom = caught ? 0x3aa28 + py * 2 : 0x10600 + (tile - 128) * 8 + py;
                        int value = caught ? ((ctx->rom[rom] >> (7 - px)) & 1) |
                                                 (((ctx->rom[rom + 1] >> (7 - px)) & 1) << 1)
                                           : ((ctx->rom[rom] >> (7 - px)) & 1) * 3;
                        size_t vram = caught ? 0x1720 + py * 2 : 0x800 + (tile - 128) * 16 + py * 2;
                        if (ctx->vram[vram] != ctx->rom[rom] ||
                            ctx->vram[vram + 1] != ctx->rom[rom + (caught ? 1 : 0)])
                            fail("unverified VRAM graphic or font", tx, ty);
                        int s = info.scale[r];
                        const auto *actual = pixel(info.left[r] + (x * 8 + px) * s + s / 2,
                                                   info.top[r] + (y * 8 + py) * s + s / 2);
                        constexpr ImU32 colors[]{0, ui_theme::DimInk,
                                                 ui_theme::FrameEdge | IM_COL32_A_MASK,
                                                 ui_theme::Ink};
                        ImU32 color = colors[value];
                        if (value) {
                            if (actual[0] != uint8_t(color >> IM_COL32_R_SHIFT) ||
                                actual[1] != uint8_t(color >> IM_COL32_G_SHIFT) ||
                                actual[2] != uint8_t(color >> IM_COL32_B_SHIFT))
                                fail("presented ink differs from original pixel", tx, ty);
                        } else if (actual[0] == 248 && actual[1] == 244 && actual[2] == 219)
                            fail("ink in an empty source pixel", tx, ty);
                        ++bits;
                    }
                caught ? ++graphics : ++glyphs;
                if (tile == 0xed || tile == 0xec) {
                    if (tx == 0) {
                        auto selection = pallet3d_dex();
                        if (ty != 3 + selection.row * 2)
                            fail("list cursor differs from visible portrait row", tx, ty);
                    } else if (tx == 15) {
                        int now = 8 + 2 * ctx->wram[0xc26], painted = 8 + 2 * ctx->wram[0xc2a];
                        if (ty != now && ty != painted)
                            fail("side cursor differs from original current/painted selection", tx,
                                 ty);
                    } else
                        fail("cursor outside original columns", tx, ty);
                    ++cursors;
                }
            }
    if (glGetError() != GL_NO_ERROR)
        fail("OpenGL error");
}
} // namespace ui_dex_qa
