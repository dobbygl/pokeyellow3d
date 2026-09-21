#include "pc_details.h"
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
    std::vector<uint8_t> rom(1048576), ram(32768), eram(32768, 0xa5);
    GBContext ctx{};
    ctx.rom = rom.data();
    ctx.rom_size = rom.size();
    ctx.wram = ram.data();
    ctx.eram = eram.data();
    ctx.eram_size = eram.size();
    // Procedural species mapping and CALL opcodes: no cartridge bytes/assets.
    rom[0x410b1] = 1;
    rom[0x410b2] = 2;
    for (auto anchor : {std::pair<size_t, int>{0x75e41, 0x5e65}, {0x75e6b, 0x3852}}) {
        rom[anchor.first] = 0xcd;
        rom[anchor.first + 1] = anchor.second & 255;
        rom[anchor.first + 2] = uint8_t(anchor.second >> 8);
    }
    auto put = [&](int address, int value) { ram[address - 0xc000] = uint8_t(value); };
    auto word = [&](int address, int value) {
        put(address, value);
        put(address + 1, value >> 8);
    };
    put(pc_details::ItemList, 255);
    check(pc_details::items(&ctx).valid && !pc_details::items(&ctx).count, "empty item PC");
    put(pc_details::ItemCount, 2);
    put(pc_details::ItemList, 4);
    put(pc_details::ItemList + 1, 99);
    put(pc_details::ItemList + 2, 20);
    put(pc_details::ItemList + 3, 1);
    put(pc_details::ItemList + 4, 255);
    auto items = pc_details::items(&ctx);
    check(items.valid && items.count == 2 && items.quantity == 100,
          "slots and quantities are distinct");
    put(pc_details::ItemList + 2, 4);
    check(!pc_details::items(&ctx).valid, "duplicate item rejected");
    put(pc_details::ItemList + 2, 20);
    put(pc_details::ItemList + 3, 0);
    check(!pc_details::items(&ctx).valid, "partial zero quantity rejected");
    put(pc_details::ItemList + 3, 100);
    check(!pc_details::items(&ctx).valid, "quantity beyond 99 rejected");
    put(pc_details::ItemList + 3, 1);
    put(pc_details::ItemCount, 51);
    check(!pc_details::items(&ctx).valid, "over-capacity item count rejected");
    put(pc_details::ItemCount, 2);
    put(pc_details::ItemList + 4, 0);
    check(!pc_details::items(&ctx).valid, "missing item terminator rejected");
    put(pc_details::ItemList + 4, 255);
    put(dex_state::Seen, 0x85);
    put(dex_state::Owned, 0x04);
    put(dex_state::Seen + 18, 0x80);
    auto rating = pc_details::rating(&ctx);
    check(rating.valid && rating.seen == 4 && rating.caught == 1,
          "original nineteen-byte bit counts");
    auto fill = [&](int index, int count) {
        auto *team = eram.data() + 0x598 + index * 96;
        std::fill_n(team, 96, 0);
        for (int i = 0; i < count; i++) {
            team[i * 16] = 1;
            team[i * 16 + 1] = uint8_t(10 + index);
            team[i * 16 + 2] = 0x80;
            team[i * 16 + 3] = 0x50;
        }
        if (count < 6)
            team[count * 16] = 255;
    };
    for (int i = 0; i < 50; i++)
        fill(i, i % 6 + 1);
    put(pc_details::HallTotal, 51);
    for (int i = 0; i < 50; i++) {
        auto team = pc_details::hall_team(&ctx, i);
        check(team.valid && team.index == i && team.number == i + 2 && team.count == i % 6 + 1 &&
                  team.mons[0].level == 10 + i,
              "all fifty team offsets and retained championship numbers");
    }
    check(!pc_details::hall_team(&ctx, -1).valid && !pc_details::hall_team(&ctx, 50).valid,
          "team bounds");
    put(pc_details::HallTotal, 255);
    check(pc_details::hall_team(&ctx, 49).number == 255, "saturated championship count");
    put(pc_details::HallTotal, 1);
    fill(0, 6);
    check(!pc_details::hall_team(&ctx, 1).valid, "unwritten team never read");
    ctx.eram_size = 0x598 + 95;
    check(!pc_details::hall_team(&ctx, 0).valid, "truncated record rejected");
    ctx.eram_size = eram.size();
    eram[0x598] = 3;
    check(!pc_details::hall_team(&ctx, 0).valid, "unknown species rejected");
    eram[0x598] = 1;
    eram[0x599] = 0;
    check(!pc_details::hall_team(&ctx, 0).valid, "invalid level rejected");
    eram[0x599] = 10;
    std::fill_n(eram.data() + 0x59a, 11, 0x80);
    check(!pc_details::hall_team(&ctx, 0).valid, "unterminated nickname rejected");
    fill(0, 6);
    ctx.sp = 0xdfe0;
    word(0xdfe0, 0x385f);
    word(0xdfe2, 0x5e6e);
    word(0xdfe6, 0x5e44);
    put(pc_details::HallIndex, 0);
    put(pc_details::HallNumber, 1);
    put(pc_details::HallSpecies, 1);
    put(pc_details::HallLevel, 10);
    std::copy_n(eram.data() + 0x598, 16, ram.data() + 0xc5b);
    // All six records are intentionally identical. Only saved C identifies
    // the live slot; a species/name lookup would wrongly select slot zero.
    for (int i = 0; i < 6; i++) {
        word(0xdfe4, 6 - i);
        auto hall = pc_details::hall(&ctx);
        check(hall.valid && hall.selected == i,
              "identical teammates keep distinct original indices");
    }
    auto oldram = ram, oldrom = rom, olderam = eram;
    check(pc_details::hall(&ctx).valid, "ready hall before read-only check");
    pc_details::items(&ctx);
    pc_details::rating(&ctx);
    check(ram == oldram && rom == oldrom && eram == olderam,
          "readers never mutate machine buffers");
    put(pc_details::HallBuffer + 2, 0x81);
    check(!pc_details::hall(&ctx).valid, "live WRAM/SRAM mismatch rejected");
    ram = oldram;
    put(pc_details::HallNumber, 2);
    check(!pc_details::hall(&ctx).valid, "stale team number rejected");
    ram = oldram;
    word(0xdfe6, 0x7777);
    check(!pc_details::hall(&ctx).valid, "wrong caller rejected");
    ram = oldram;
    rom[0x75e6b] = 0;
    check(!pc_details::hall(&ctx).valid, "wrong ROM call rejected");
    rom = oldrom;
    ctx.sp = 0xdfe8;
    check(!pc_details::hall(&ctx).valid, "popped stack return rejected");
    check(!pc_details::hall(nullptr).valid && !pc_details::items(nullptr).valid &&
              !pc_details::rating(nullptr).valid,
          "null context rejected");
    std::puts("PASS: item slots, Oak bitmaps, fifty SRAM teams, original selection, corruption and "
              "read-only guards");
}
