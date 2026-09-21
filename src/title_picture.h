#pragma once
#include "gbrt.h"
#include "ppu.h"
#include <array>

namespace title_picture {
constexpr int Width = 96, Height = 72, Left = 32, Top = 64;
using Image = std::array<uint8_t, Width * Height * 4>;
inline Image decode(const GBContext *ctx) {
    Image out{};
    if (!ctx || !ctx->vram || !ctx->oam || !ctx->io || !ctx->ppu)
        return out;
    const auto *ppu = static_cast<const GBPPU *>(ctx->ppu);
    const bool cgb = ctx->config.model == GB_MODEL_CGB && !ctx->config.cgb_compatibility_mode;
    const int lcdc = ctx->io[0x40];
    if (!(lcdc & 0x80))
        return out;
    std::array<uint8_t, Width * Height> ink{};
    auto color = [&](bool obj, int palette, int raw) {
        if (!cgb)
            raw = (ctx->io[obj ? 0x48 + palette : 0x47] >> (raw * 2)) & 3;
        if (ctx->config.model != GB_MODEL_CGB) {
            constexpr uint32_t dmg[]{0xe0f8d0, 0x88c070, 0x346856, 0x081820};
            return dmg[raw];
        }
        const auto *ram = obj ? ppu->obj_palette_ram : ppu->bg_palette_ram;
        const int at = (cgb || obj ? palette : 0) * 8 + raw * 2;
        int rgb = ram[at] | (ram[at + 1] << 8);
        return uint32_t(((rgb & 31) * 255 / 31) << 16 | (((rgb >> 5) & 31) * 255 / 31) << 8 |
                        ((rgb >> 10) & 31) * 255 / 31);
    };
    for (int y = 0; y < Height; ++y)
        for (int x = 0; x < Width; ++x) {
            int sx = x + Left, sy = y + Top;
            bool window = (lcdc & 32) && sy >= ctx->io[0x4a] && sx >= int(ctx->io[0x4b]) - 7;
            int px = window ? sx - int(ctx->io[0x4b]) + 7 : (sx + ctx->io[0x43]) & 255;
            int py = window ? sy - ctx->io[0x4a] : (sy + ctx->io[0x42]) & 255;
            int map = (lcdc & (window ? 64 : 8)) ? 0x1c00 : 0x1800;
            int cell = map + (py / 8) * 32 + px / 8;
            int tile = ctx->vram[cell], attr = cgb ? ctx->vram[cell + 0x2000] : 0;
            int tx = (attr & 32) ? 7 - px % 8 : px % 8;
            int ty = (attr & 64) ? 7 - py % 8 : py % 8;
            int address = ((lcdc & 16) ? tile * 16 : 0x1000 + int(int8_t(tile)) * 16) +
                          ((attr & 8) ? 0x2000 : 0) + ty * 2;
            int raw = ((ctx->vram[address] >> (7 - tx)) & 1) |
                      (((ctx->vram[address + 1] >> (7 - tx)) & 1) << 1);
            uint32_t rgb = color(false, attr & 7, raw);
            bool opaque = raw != 0;
            // The title uses OAM for Pikachu's cheeks and animated mouth.
            // Honor the original priority, flips and 8x16 mode read-only.
            int count = 0, best_x = 256;
            for (int i = 0; (lcdc & 2) && i < 40; ++i) {
                const auto *obj = ctx->oam + i * 4;
                int ox = int(obj[1]) - 8, oy = int(obj[0]) - 16, height = (lcdc & 4) ? 16 : 8;
                if (sy < oy || sy >= oy + height)
                    continue;
                if (++count > 10)
                    break;
                if (sx < ox || sx >= ox + 8 || ((!cgb || ppu->opri) && ox >= best_x))
                    continue;
                int flags = obj[3], row = (flags & 64) ? height - 1 - (sy - oy) : sy - oy;
                int bit = (flags & 32) ? sx - ox : 7 - (sx - ox);
                int at = ((height == 16 ? obj[2] & 254 : obj[2]) * 16) + row * 2 +
                         ((cgb && (flags & 8)) ? 0x2000 : 0);
                int value = ((ctx->vram[at] >> bit) & 1) | (((ctx->vram[at + 1] >> bit) & 1) << 1);
                if (!value)
                    continue;
                best_x = ox;
                bool behind = raw && (flags & 128);
                if (cgb)
                    behind = raw && (lcdc & 1) && ((attr | flags) & 128);
                if (!behind) {
                    rgb = color(true, cgb ? flags & 7 : (flags >> 4) & 1, value);
                    opaque = true;
                }
                if (cgb && !ppu->opri)
                    break;
            }
            int p = y * Width + x;
            ink[p] = opaque;
            out[p * 4] = uint8_t(rgb >> 16);
            out[p * 4 + 1] = uint8_t(rgb >> 8);
            out[p * 4 + 2] = uint8_t(rgb);
            out[p * 4 + 3] = 255;
        }
    // As with monster portraits, remove only exterior background; keep white
    // highlights enclosed by the outline. No framebuffer pixels are modified.
    std::array<int, Width * Height> queue{};
    int begin = 0, end = 0;
    auto push = [&](int p) {
        if (!ink[p] && out[p * 4 + 3]) {
            out[p * 4 + 3] = 0;
            queue[end++] = p;
        }
    };
    for (int x = 0; x < Width; ++x) {
        push(x);
        push((Height - 1) * Width + x);
    }
    for (int y = 0; y < Height; ++y) {
        push(y * Width);
        push(y * Width + Width - 1);
    }
    while (begin < end) {
        int p = queue[begin++];
        if (p % Width)
            push(p - 1);
        if (p % Width < Width - 1)
            push(p + 1);
        if (p >= Width)
            push(p - Width);
        if (p < Width * (Height - 1))
            push(p + Width);
    }
    return out;
}
} // namespace title_picture
