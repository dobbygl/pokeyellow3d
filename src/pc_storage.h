#pragma once
#include "mon_pic.h"
#include "gbrt.h"
#include <array>
#include <cstddef>
#include <cstdint>

// Canonical Yellow box layout, verified against ram/{wram,sram}.asm and
// engine/menus/save.asm. Cartridge addresses are resolved in the flat ERAM
// allocation; the currently selected MBC RAM bank is deliberately irrelevant.
namespace pc_storage {
constexpr size_t BoxBytes = 0x462, BankBytes = 0x2000, BankBoxesBytes = 6 * BoxBytes;
struct Mon {
    int species = 0, level = 0;
    std::array<uint8_t, 11> name{};
};
struct Box {
    bool valid = false;
    int count = 0;
    std::array<Mon, 20> mons{};
};
struct Snapshot {
    bool valid = false, initialized = false;
    int active = -1;
    std::array<Box, 12> boxes{};
    Box party;
    uint64_t fingerprint = 1469598103934665603ull;
};
inline void hash(uint64_t &value, const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        value ^= data[i];
        value *= 1099511628211ull;
    }
}
inline uint8_t checksum(const uint8_t *data, size_t size) {
    unsigned sum = 0;
    for (size_t i = 0; i < size; i++)
        sum += data[i];
    return uint8_t(~sum);
}
inline Box decode(const GBContext *ctx, const uint8_t *species, const uint8_t *mons,
                  const uint8_t *names, int count, int capacity, int stride, int level_offset) {
    Box box;
    if (count < 0 || count > capacity || species[count] != 255)
        return box;
    for (int i = 0; i < count; i++) {
        if (!mon_pic::number(ctx->rom, ctx->rom_size, species[i]) || mons[i * stride] != species[i])
            return {};
        auto &mon = box.mons[i];
        mon.species = species[i];
        mon.level = mons[i * stride + level_offset];
        if (mon.level < 1 || mon.level > 100)
            return {};
        bool ended = false;
        for (int j = 0; j < 11; j++) {
            mon.name[j] = names[i * 11 + j];
            ended |= mon.name[j] == 0x50;
        }
        if (!ended)
            return {};
    }
    box.count = count;
    box.valid = true;
    return box;
}
inline Box decode_box(const GBContext *ctx, const uint8_t *data) {
    return decode(ctx, data + 1, data + 22, data + 902, data[0], 20, 33, 3);
}
inline bool bank_valid(const GBContext *ctx, int bank) {
    if (!ctx || !ctx->eram || bank < 2 || bank > 3 ||
        ctx->eram_size < size_t(bank) * BankBytes + BankBoxesBytes + 7)
        return false;
    const auto *data = ctx->eram + size_t(bank) * BankBytes;
    if (checksum(data, BankBoxesBytes) != data[BankBoxesBytes])
        return false;
    for (int i = 0; i < 6; i++)
        if (checksum(data + i * BoxBytes, BoxBytes) != data[BankBoxesBytes + 1 + i])
            return false;
    return true;
}
inline Snapshot read(const GBContext *ctx) {
    Snapshot result;
    if (!ctx || !ctx->wram || !ctx->rom || ctx->rom_size != 1048576)
        return result;
    const uint8_t *ram = ctx->wram;
    int number = ram[0x159f];
    result.active = number & 0x7f;
    result.initialized = number & 0x80;
    if (result.active >= 12)
        return result;
    hash(result.fingerprint, ram + 0x159f, 1);
    hash(result.fingerprint, ram + 0x1a7f, BoxBytes);
    hash(result.fingerprint, ram + 0x1162, 0x194);
    result.boxes[result.active] = decode_box(ctx, ram + 0x1a7f);
    result.party = decode(ctx, ram + 0x1163, ram + 0x116a, ram + 0x12b4, ram[0x1162], 6, 44, 33);
    if (!result.boxes[result.active].valid || !result.party.valid)
        return result;
    // Before the first Change Box, the engine considers all other boxes empty
    // and initializes them on demand. Uninitialized SRAM is never interpreted.
    if (!result.initialized) {
        for (int i = 0; i < 12; i++)
            if (i != result.active)
                result.boxes[i].valid = true;
    } else {
        for (int bank = 2; bank <= 3; bank++) {
            if (!bank_valid(ctx, bank))
                return result;
            const auto *data = ctx->eram + size_t(bank) * BankBytes;
            hash(result.fingerprint, data, BankBoxesBytes + 7);
            for (int local = 0; local < 6; local++) {
                int index = (bank - 2) * 6 + local;
                if (index == result.active)
                    continue;
                result.boxes[index] = decode_box(ctx, data + local * BoxBytes);
                if (!result.boxes[index].valid)
                    return result;
            }
        }
    }
    result.valid = true;
    return result;
}
} // namespace pc_storage
