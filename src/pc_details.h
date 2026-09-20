#pragma once
#include "pc_storage.h"
#include "battle_state.h"
#include "dex_state.h"
#include <cstring>

// Canonical Yellow UE host-side readers. WRAM aliases and SRAM offsets are
// verified against the generated symbols and original player/Oak/League PCs.
namespace pc_details {
constexpr int ItemCount = 0xd539, ItemList = 0xd53a, ItemCapacity = 50;
constexpr int HallBuffer = 0xcc5b, HallTotal = 0xd5a1, HallIndex = 0xcd41, HallNumber = 0xcd42,
              HallSpecies = 0xcd3d, HallLevel = 0xcd3f;
constexpr size_t HallOffset = 0x598, HallRecordBytes = 16, HallTeamBytes = 96;
constexpr int HallCapacity = 50;
struct Items {
    bool valid = false;
    int count = 0, quantity = 0;
};
inline Items items(const GBContext *ctx) {
    if (!ctx || !ctx->wram)
        return {};
    int count = battle::read(ctx, ItemCount);
    if (count > ItemCapacity || battle::read(ctx, ItemList + count * 2) != 255)
        return {};
    std::array<bool, 256> seen{};
    int quantity = 0;
    for (int i = 0; i < count; ++i) {
        int id = battle::read(ctx, ItemList + i * 2);
        int amount = battle::read(ctx, ItemList + i * 2 + 1);
        if (!id || id == 255 || seen[id] || amount < 1 || amount > 99)
            return {};
        seen[id] = true;
        quantity += amount;
    }
    return {true, count, quantity};
}
struct Rating {
    bool valid = false;
    int seen = 0, caught = 0;
};
inline Rating rating(const GBContext *ctx) {
    if (!ctx || !ctx->wram)
        return {};
    // The original CountSetBits visits all 19 bytes, including their spare bit.
    return {true, dex_state::count(ctx, dex_state::Seen), dex_state::count(ctx, dex_state::Owned)};
}
struct HallTeam {
    bool valid = false;
    int index = -1, number = 0, count = 0, selected = -1;
    std::array<pc_storage::Mon, 6> mons{};
    uint64_t fingerprint = 1469598103934665603ull;
};
inline HallTeam hall_team(const GBContext *ctx, int index) {
    HallTeam team;
    if (!ctx || !ctx->wram || !ctx->eram || !ctx->rom || ctx->rom_size != 1048576)
        return team;
    int total = battle::read(ctx, HallTotal), stored = std::min(total, HallCapacity);
    if (index < 0 || index >= stored)
        return team;
    size_t start = HallOffset + size_t(index) * HallTeamBytes;
    if (ctx->eram_size < start + HallTeamBytes)
        return team;
    const uint8_t *bytes = ctx->eram + start;
    for (int i = 0; i < 6; ++i) {
        const uint8_t *record = bytes + i * HallRecordBytes;
        if (record[0] == 255)
            break;
        auto &mon = team.mons[i];
        mon.species = record[0];
        mon.level = record[1];
        if (!mon_pic::number(ctx->rom, ctx->rom_size, mon.species) || mon.level < 1 ||
            mon.level > 100)
            return {};
        std::copy_n(record + 2, mon.name.size(), mon.name.begin());
        if (std::find(mon.name.begin(), mon.name.end(), 0x50) == mon.name.end())
            return {};
        ++team.count;
    }
    if (!team.count)
        return {};
    team.index = index;
    team.number = total - stored + index + 1;
    team.valid = true;
    pc_storage::hash(team.fingerprint, bytes, HallTeamBytes);
    return team;
}
inline HallTeam hall(const GBContext *ctx) {
    if (!ctx || !ctx->wram || !ctx->rom || ctx->rom_size != 1048576 ||
        !battle::live_return(ctx, 0x75e41, 0x5e65) || !battle::live_return(ctx, 0x75e6b, 0x3852))
        return {};
    auto team = hall_team(ctx, battle::read(ctx, HallIndex));
    if (!team.valid || battle::read(ctx, HallNumber) != team.number)
        return {};
    // LeaguePCShowTeam pushes BC immediately before ShowMon/WaitForText.
    // Saved C counts six downwards. The enclosing return must belong to the
    // verified ShowTeam call: matching species alone cannot distinguish twins.
    auto stack_word = [&](int at) {
        return battle::read(ctx, at) | (battle::read(ctx, at + 1) << 8);
    };
    for (int at = ctx->sp; at + 5 < 0xe000; at += 2) {
        if (stack_word(at) != 0x5e6e || stack_word(at + 4) != 0x5e44)
            continue;
        int selected = 6 - battle::read(ctx, at + 2);
        if (selected < 0 || selected >= team.count)
            continue;
        const uint8_t *record = ctx->eram + HallOffset + size_t(team.index) * HallTeamBytes +
                                size_t(selected) * HallRecordBytes;
        if (std::memcmp(ctx->wram + HallBuffer - 0xc000, record, HallRecordBytes) ||
            battle::read(ctx, HallSpecies) != record[0] ||
            battle::read(ctx, HallLevel) != record[1])
            continue;
        team.selected = selected;
        return team;
    }
    return {};
}
} // namespace pc_details
