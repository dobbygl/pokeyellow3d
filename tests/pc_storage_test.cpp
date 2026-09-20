#include "pc_storage.h"
#include "pc_box_state.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
static void check(bool value, const char *why) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}
int main(int argc, char **argv) {
    if (argc != 2 && argc != 4)
        return 77;
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> rom(std::istreambuf_iterator<char>(file), {}), ram(32768),
        eram(32768, 0xa5);
    check(rom.size() == 1048576, "canonical ROM");
    GBContext ctx{};
    ctx.rom = rom.data();
    ctx.rom_size = rom.size();
    ctx.wram = ram.data();
    ctx.eram = eram.data();
    ctx.eram_size = eram.size();
    if (argc == 4) {
        std::ifstream wram_file(argv[2], std::ios::binary), eram_file(argv[3], std::ios::binary);
        if (!wram_file || !eram_file) {
            std::puts(
                "SKIP: generate original-engine storage evidence with tests/pc_storage_qa.sh");
            return 77;
        }
        ram.assign(std::istreambuf_iterator<char>(wram_file), {});
        eram.assign(std::istreambuf_iterator<char>(eram_file), {});
        check(ram.size() == 32768 && eram.size() == 32768, "original memory snapshot sizes");
        ctx.wram = ram.data();
        ctx.eram = eram.data();
        ctx.eram_size = eram.size();
        auto data = pc_storage::read(&ctx);
        check(data.valid && data.initialized && data.active == 6,
              "original Change Box produced both verified SRAM banks");
        check(data.boxes[0].count == 1 &&
                  mon_pic::number(ctx.rom, ctx.rom_size, data.boxes[0].mons[0].species) == 16,
              "original deposited Pidgey read from bank two");
        for (int i = 1; i < 12; i++)
            check(data.boxes[i].count == 0, "other boxes remain empty in original save");
        check(data.party.count == 1, "original party lost its deposited Pidgey");
        std::puts("PASS: storage decoder agrees with original-engine Pidgey deposit and box change "
                  "evidence");
        return 0;
    }
    // Deliberately distinct names, species and levels expose wrong-bank or
    // wrong-stride reads even when both banks have internally valid checksums.
    auto fill = [&](uint8_t *box, int number, int count) {
        std::fill_n(box, pc_storage::BoxBytes, 0);
        box[0] = uint8_t(count);
        box[1 + count] = 255;
        for (int i = 0; i < count; i++) {
            int species = mon_pic::species(rom.data(), rom.size(), number + i);
            box[1 + i] = box[22 + 33 * i] = uint8_t(species);
            box[22 + 33 * i + 3] = uint8_t(number + i);
            box[902 + 11 * i] = uint8_t(0x80 + number % 26);
            box[903 + 11 * i] = 0x50;
        }
    };
    auto seal = [&](int bank) {
        auto *base = eram.data() + bank * 0x2000;
        // Use an independent subtractive formulation of the original complement.
        unsigned all = 255;
        for (int i = 0; i < 6; i++) {
            unsigned sum = 255;
            for (int j = 0; j < 0x462; j++) {
                sum -= base[i * 0x462 + j];
                all -= base[i * 0x462 + j];
            }
            base[0x1a4d + i] = uint8_t(sum);
        }
        base[0x1a4c] = uint8_t(all);
    };
    int pikachu = mon_pic::species(rom.data(), rom.size(), 25);
    ram[0x1162] = 1;
    ram[0x1163] = ram[0x116a] = uint8_t(pikachu);
    ram[0x1164] = 255;
    ram[0x118b] = 7;
    ram[0x12b4] = 0x80;
    ram[0x12b5] = 0x50;
    fill(ram.data() + 0x1a7f, 16, 1);
    auto first = pc_storage::read(&ctx);
    check(first.valid && !first.initialized && first.active == 0, "uninitialized SRAM is not read");
    check(first.boxes[0].count == 1 && first.boxes[11].valid && first.boxes[11].count == 0,
          "never-used boxes are empty by original engine semantics");
    check(first.party.mons[0].level == 7 && first.party.mons[0].species == pikachu,
          "party has its own record and level offsets");
    for (int i = 0; i < 12; i++)
        fill(eram.data() + (2 + i / 6) * 0x2000 + (i % 6) * 0x462, i * 2 + 1, i % 3 + 1);
    seal(2);
    seal(3);
    ram[0x159f] = 0x80;
    for (int active = 0; active < 12; active++) {
        ram[0x159f] = uint8_t(0x80 | active);
        auto s = pc_storage::read(&ctx);
        check(s.valid && s.active == active, "all twelve bank-relative addresses");
        for (int i = 0; i < 12; i++)
            check(s.boxes[i].count == (i == active ? 1 : i % 3 + 1) &&
                      s.boxes[i].mons[0].level == (i == active ? 16 : i * 2 + 1),
                  "active WRAM overrides the stale saved copy");
    }
    auto stable = pc_storage::read(&ctx);
    ram[0x0109] ^= 4;
    check(pc_storage::read(&ctx).fingerprint == stable.fingerprint,
          "unrelated world state does not rebuild storage");
    ram[0x12b4]++;
    check(pc_storage::read(&ctx).fingerprint != stable.fingerprint,
          "party name participates in fingerprint");
    auto saved = eram;
    for (int bank = 2; bank <= 3; bank++)
        for (size_t offset : {size_t(0), size_t(0x1a4c), size_t(0x1a4d), size_t(0x1a52)}) {
            eram[bank * 0x2000 + offset] ^= 1;
            check(!pc_storage::read(&ctx).valid,
                  "data, aggregate checksum and every individual checksum reject corruption");
            eram = saved;
        }
    ctx.eram_size = 0x6000;
    check(!pc_storage::read(&ctx).valid, "truncated third bank rejected");
    ctx.eram_size = eram.size();
    ram[0x159f] = 0x8c;
    check(!pc_storage::read(&ctx).valid, "invalid active box rejected");
    ram[0x159f] = 0x80;
    ram[0x1a7f] = 21;
    check(!pc_storage::read(&ctx).valid, "overfull active box rejected");
    fill(ram.data() + 0x1a7f, 16, 1);
    ram[0x1a81] = 0;
    check(!pc_storage::read(&ctx).valid, "incomplete species terminator rejected");
    ram[0x1a81] = 255;
    ram[0x1a95] = uint8_t(pikachu);
    check(!pc_storage::read(&ctx).valid, "partially copied species record rejected");
    fill(ram.data() + 0x1a7f, 16, 1);
    auto before_ram = ram, before_eram = eram;
    check(pc_storage::read(&ctx).valid && ram == before_ram && eram == before_eram,
          "reading boxes never writes game memory");
    std::vector<uint8_t> vram(16384), io(128);
    std::array<uint32_t, 160 * 144> lcd;
    ctx.vram = vram.data();
    ctx.io = io.data();
    ctx.sp = 0xdfe0;
    io[0x40] = 0x80;
    io[0x47] = 0xe4;
    for (int tile = 128; tile < 256; tile++)
        for (int y = 0; y < 8; y++)
            vram[0x1000 + int(int8_t(tile)) * 16 + y * 2] = rom[0x10600 + (tile - 128) * 8 + y];
    auto data = pc_storage::read(&ctx);
    data.boxes[0].count = 2;
    for (int i = 0; i < 2; i++) {
        data.boxes[0].mons[i].name = {0x8f, 0x88, 0x83, 0x86, 0x84, 0x98, uint8_t(0xf7 + i), 0x50};
    }
    data.party = data.boxes[0];
    auto paint = [&](int tile, int x, int y) {
        for (int yy = 0; yy < 8; yy++)
            for (int xx = 0; xx < 8; xx++)
                lcd[(y * 8 + yy) * 160 + x * 8 + xx] =
                    rom[0x10600 + (tile - 128) * 8 + yy] & (1 << (7 - xx)) ? 0xff182028
                                                                           : 0xffffffff;
    };
    auto row = [&](int index, int line, int arrow) {
        lcd.fill(0xffffffff);
        paint(arrow, 5, 4 + line * 2);
        for (int i = 0; i < 7; i++)
            paint(data.boxes[0].mons[index].name[i], 6 + i, 4 + line * 2);
    };
    auto call = [&](int address) {
        int ret = (address % 0x4000) + 0x4003;
        ram[0x1fe0] = uint8_t(ret);
        ram[0x1fe1] = uint8_t(ret >> 8);
    };
    call(0x2170b);
    ram[0xf8a] = 0x7f;
    ram[0xf8b] = 0xda;
    ram[0xc26] = ram[0xc36] = 0;
    row(0, 0, 0xed);
    check(pc_box_state::selection(&ctx, lcd.data(), data).index == 0,
          "displayed first box entry is selected");
    ram[0xc26] = 1;
    check(pc_box_state::selection(&ctx, lcd.data(), data).index == -1,
          "future WRAM cursor cannot highlight the previous LCD name");
    row(1, 1, 0xed);
    check(pc_box_state::selection(&ctx, lcd.data(), data).index == 1,
          "new visible name and cursor update selection");
    ram[0xc26] = 0;
    ram[0xc36] = 1;
    row(1, 0, 0xed);
    check(pc_box_state::selection(&ctx, lcd.data(), data).index == 1,
          "scroll offset is part of list selection");
    ram[0xc26] = 1;
    check(pc_box_state::selection(&ctx, lcd.data(), data).index == -1,
          "Cancel does not select a portrait");
    call(0x215ad);
    ram[0xf91] = 1;
    ram[0xc26] = ram[0xc36] = 0;
    row(1, 1, 0xec);
    auto selected = pc_box_state::selection(&ctx, lcd.data(), data);
    check(selected.index == 1 && selected.party,
          "deposit action uses original chosen Pokemon, not its action-menu cursor");
    call(0x2163b);
    selected = pc_box_state::selection(&ctx, lcd.data(), data);
    check(selected.index == 1 && !selected.party, "withdraw action selects a box portrait");
    ctx.sp = 0xdfe2;
    check(pc_box_state::selection(&ctx, lcd.data(), data).index == -1,
          "popped list/action cannot leave a stale selection");
    std::puts("PASS: twelve boxes, independent banks/checksums, active WRAM precedence, party "
              "layout, invalid/transient data and fingerprints");
}
