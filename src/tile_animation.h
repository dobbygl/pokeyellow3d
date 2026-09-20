#pragma once
#include "pallet_state.h"
#include <algorithm>
#include <array>
#include <cstring>

namespace tile_animation {
using Tile = std::array<uint8_t, 16>;
// UE Yellow: UpdateMovingBgTiles (00:1c75), FlowerTile1/2/3 (00:1cd5).
// Water rotates at counter1=20; flower copies on the following VBlank when
// animations=2. Counter2 cycles through eight water steps; flower uses its low
// two bits (0/1, 2, 3). Neither counter alone describes a freshly loaded tile:
// tileset loads reset counter1 but retain counter2 and replace VRAM graphics.
constexpr size_t Flowers = 0x1cd5;
constexpr int Water = 0x14, Flower = 0x03;
constexpr int Flag = 0x57, Counter = 0x58, Step = 0xd084;
struct Phase {
    int water = -1, flower = -1;
};
inline Tile original(const GBContext *ctx, const kanto::Tileset &tiles, int tile) {
    Tile result{};
    if (ctx && ctx->rom && tiles.graphics + size_t(tile + 1) * 16 <= ctx->rom_size)
        std::copy_n(ctx->rom + tiles.graphics + tile * 16, 16, result.begin());
    return result;
}
inline Tile rotate(Tile tile, int right) {
    if (right > 0 && right < 8)
        for (auto &byte : tile)
            byte = uint8_t((byte >> right) | (byte << (8 - right)));
    return tile;
}
inline Tile flower(const GBContext *ctx, int frame) {
    Tile result{};
    if (ctx && ctx->rom && frame >= 0 && frame < 3 && Flowers + 48 <= ctx->rom_size)
        std::copy_n(ctx->rom + Flowers + frame * 16, 16, result.begin());
    return result;
}
inline Phase sample(const GBContext *ctx, const kanto::Tileset &current) {
    Phase phase;
    if (!ctx || !ctx->rom || !ctx->vram || !ctx->hram || !ctx->wram ||
        ctx->rom_size != kanto::RomSize || pallet::read(ctx, pallet::Tileset) != current.id ||
        current.animations < 1 || current.animations > 2 || ctx->hram[Flag] > 2)
        return phase;
    // Recover the exact engine phase from its resident tiles, then generate
    // private textures from ROM. This also handles pauses, skipped VBlanks,
    // unloaded neighbors, disabled animation, F2 and arbitrary state reloads
    // without integrating a second clock or retaining stale presentation time.
    const auto water = original(ctx, current, Water);
    for (int shift = 0; shift < 8; ++shift)
        if (!std::memcmp(rotate(water, shift).data(), ctx->vram + 0x1000 + Water * 16, 16)) {
            phase.water = shift;
            break;
        }
    if (current.animations == 2)
        for (int frame = 0; frame < 3; ++frame)
            if (!std::memcmp(flower(ctx, frame).data(), ctx->vram + 0x1000 + Flower * 16, 16)) {
                phase.flower = frame;
                break;
            }
    return phase;
}
inline Tile frame(const GBContext *ctx, const kanto::Tileset &tiles, int tile, Phase phase) {
    auto result = original(ctx, tiles, tile);
    if (tiles.animations < 1 || tiles.animations > 2)
        return result;
    if (tile == Water && phase.water >= 0)
        return rotate(result, phase.water);
    if (tile == Flower && tiles.animations == 2 && phase.flower >= 0)
        return flower(ctx, phase.flower);
    return result;
}
} // namespace tile_animation
