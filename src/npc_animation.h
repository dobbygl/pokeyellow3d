#pragma once
#include "gbrt.h"
#include <array>
#include <cstdint>

// Canonical UE Yellow. SpriteSheetPointerTable = 05:42a9 and the facing
// table = 01:4000, verified against the imported symbols and original ROM.
// No host clock: CheckSpriteAvailability can freeze an offscreen NPC midway
// through a step. Its live counters remain the authority, including that pause.
namespace npc_animation {
constexpr size_t Sheets = 0x142a9, SheetsEnd = 0x143f1, Facings = 0x4000;
struct Motion {
    int frame = 0;
    float dx = 0, dz = 0;
    bool walking = false;
};
inline Motion sample(const GBContext *ctx, int slot) {
    Motion result;
    if (!ctx || !ctx->wram || slot < 1 || slot > 14)
        return result;
    const auto *a = ctx->wram + 0x100 + slot * 16;
    const auto *b = ctx->wram + 0x200 + slot * 16;
    result.frame = a[9] & 12;
    int speed = a[1] == 4 ? 2 : 1;
    int dx = int8_t(a[5]), dz = int8_t(a[3]);
    result.walking = (a[1] == 3 || a[1] == 4) && b[0] > 0 && b[0] <= 16 / speed &&
                     ((dx == 0 && (dz == -1 || dz == 1)) || (dz == 0 && (dx == -1 || dx == 1)));
    if (result.walking) {
        result.frame |= a[8] & 3;
        // Map coordinates contain the destination as soon as TryWalking starts.
        result.dx = float(-dx * b[0] * speed) / 16.f;
        result.dz = float(-dz * b[0] * speed) / 16.f;
    }
    return result;
}
using Image = std::array<uint8_t, 32 * 32>;
inline bool decode(const GBContext *ctx, int slot, int frame, Image &out) {
    if (!ctx || !ctx->wram || !ctx->rom || ctx->rom_size != 1048576 || slot < 1 || slot > 14 ||
        frame < 0 || frame > 15)
        return false;
    int picture = ctx->wram[0x100 + slot * 16];
    // The matching commercial ROM retains 82 entries (including unused ones).
    // Current disassembly source may omit those: never import its renumbered IDs.
    if (picture < 1 || size_t(picture) > (SheetsEnd - Sheets) / 4)
        return false;
    const auto *entry = ctx->rom + Sheets + (picture - 1) * 4;
    size_t address = size_t(entry[0]) | (size_t(entry[1]) << 8);
    int bytes = entry[2], bank = entry[3];
    if ((bytes != 64 && bytes != 192) || bank < 1 || bank >= 64 || address < 0x4000 ||
        address >= 0x8000)
        return false;
    size_t sheet = size_t(bank) * 0x4000 + address - 0x4000;
    if (bytes == 64)
        frame = 0; // Items and other four-tile pictures never walk or turn.
    size_t table = Facings + size_t(frame) * 2;
    size_t at = size_t(ctx->rom[table]) | (size_t(ctx->rom[table + 1]) << 8);
    if (at < 0x4000 || at >= 0x8000)
        return false;
    int count = ctx->rom[at++];
    if (count < 1 || count > 9 || at + size_t(count) * 4 > 0x8000)
        return false;
    Image image{};
    for (int i = 0; i < count; ++i, at += 4) {
        int dy = int8_t(ctx->rom[at]), dx = int8_t(ctx->rom[at + 1]);
        int tile = ctx->rom[at + 2], flags = ctx->rom[at + 3];
        if ((tile & 127) >= bytes / 16 || (bytes == 64 && (tile & 128)))
            return false;
        // LoadWalkingTilePattern copies the next twelve tiles into VRAM +$800.
        size_t source = sheet + ((tile & 128) ? 192 : 0) + size_t(tile & 127) * 16;
        if (source + 16 > ctx->rom_size)
            return false;
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                int tx = (flags & 0x20) ? 7 - x : x, ty = (flags & 0x40) ? 7 - y : y;
                int value = ((ctx->rom[source + ty * 2] >> (7 - tx)) & 1) |
                            (((ctx->rom[source + ty * 2 + 1] >> (7 - tx)) & 1) << 1);
                int px = 8 + dx + x, py = 8 + dy + y;
                if (px >= 0 && px < 32 && py >= 0 && py < 32)
                    image[size_t(py) * 32 + px] = uint8_t(value);
            }
    }
    out = image;
    return true;
}
} // namespace npc_animation
