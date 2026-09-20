#include "tile_animation.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
static void check(bool ok, const char *why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}
int main() {
    std::vector<uint8_t> rom(kanto::RomSize), vram(16384), wram(32768), hram(128);
    GBContext ctx{};
    ctx.rom = rom.data();
    ctx.rom_size = rom.size();
    ctx.vram = vram.data();
    ctx.wram = wram.data();
    ctx.hram = hram.data();
    kanto::Tileset tiles;
    tiles.graphics = 0x4000;
    tiles.animations = 2;
    for (int i = 0; i < 16; ++i) {
        rom[tiles.graphics + tile_animation::Water * 16 + i] = uint8_t(i * 13 + 1);
        for (int j = 0; j < 3; ++j)
            rom[tile_animation::Flowers + j * 16 + i] = uint8_t(i * 7 + j * 17);
    }
    const auto original = tile_animation::original(&ctx, tiles, tile_animation::Water);
    for (int shift = 0; shift < 8; ++shift) {
        // Build the oracle by repeatedly rotating single bits, independently
        // of the closed-form renderer operation.
        auto expected = original;
        for (int n = 0; n < shift; ++n)
            for (auto &byte : expected)
                byte = uint8_t((byte & 1) * 128 + byte / 2);
        std::copy(expected.begin(), expected.end(), vram.begin() + 0x1140);
        for (int f = 0; f < 3; ++f) {
            std::copy_n(rom.data() + tile_animation::Flowers + f * 16, 16, vram.data() + 0x1030);
            auto before = vram;
            auto phase = tile_animation::sample(&ctx, tiles);
            check(phase.water == shift && phase.flower == f, "all live ROM phases detected");
            check(tile_animation::frame(&ctx, tiles, tile_animation::Water, phase) == expected,
                  "water generated from ROM matches independent bit oracle");
            check(vram == before, "read-only phase detection");
            for (int disabled : {0, 3, 255}) {
                auto other = tiles;
                other.animations = disabled;
                check(tile_animation::frame(&ctx, other, tile_animation::Water, phase) == original,
                      "non-animated/unknown tilesets stay static");
            }
            auto water_only = tiles;
            water_only.animations = 1;
            check(tile_animation::sample(&ctx, water_only).flower == -1,
                  "water-only tilesets never treat tile 03 as a flower");
            check(tile_animation::frame(&ctx, water_only, tile_animation::Flower, phase) ==
                      tile_animation::original(&ctx, water_only, tile_animation::Flower),
                  "water-only tile 03 remains intact");
            check(tile_animation::sample(&ctx, tiles).water == shift,
                  "engine flag zero freezes the actual resident phase");
        }
    }
    std::fill(vram.begin() + 0x1140, vram.begin() + 0x1150, 0xff);
    std::fill(vram.begin() + 0x1030, vram.begin() + 0x1040, 0xff);
    auto invalid = tile_animation::sample(&ctx, tiles);
    check(invalid.water == -1 && invalid.flower == -1, "unrecognized graphics rejected");
    check(tile_animation::frame(&ctx, tiles, tile_animation::Water, invalid) == original,
          "unrecognized state retains static presentation");
    wram[pallet::Tileset - 0xc000] = 1;
    check(tile_animation::sample(&ctx, tiles).water == -1, "mismatched tileset rejected");
    check(tile_animation::sample(nullptr, tiles).water == -1, "missing context rejected");
    std::puts(
        "PASS: independent rotations, three flowers, flags, frozen/reloaded phases, fallback");
}
