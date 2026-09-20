#include "dex_state.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
static void check(bool value, const char *message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 77;
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<uint8_t> rom(std::istreambuf_iterator<char>(input), {});
    check(rom.size() == 1048576, "local canonical ROM");
    std::vector<uint8_t> ram(32768), vram(16384), io(128);
    GBContext ctx{};
    ctx.rom = rom.data();
    ctx.rom_size = rom.size();
    ctx.wram = ram.data();
    ctx.vram = vram.data();
    ctx.io = io.data();
    ctx.sp = 0xdff0;
    auto set = [&](int address, int value) { ram[address - 0xc000] = uint8_t(value); };
    set(0xdff0, 7);
    set(0xdff1, 0x41);
    set(dex_state::Current, 84);
    set(0xc3a0, 0x63);
    set(0xc3a0 + 359, 0x6e);
    io[0x40] = 0x81;
    io[0x47] = 0xe4;
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 7; x++) {
            int id = (6 - x) * 7 + y;
            set(0xc3a0 + (y + 1) * 20 + x + 1, id);
            vram[0x1800 + (y + 1) * 32 + x + 1] = uint8_t(id);
        }
    auto before_ram = ram, before_vram = vram, before_io = io;
    check(dex_state::data(&ctx), "positive ROM call, mirrored rectangle and live BG agree");
    check(before_ram == ram && before_vram == vram && before_io == io, "detector is read-only");
    ctx.sp = 0xdff2;
    check(!dex_state::data(&ctx), "stale popped return does not detect a screen");
    ctx.sp = 0xdff0;
    rom[0x40104] = 0;
    check(!dex_state::data(&ctx), "call opcode is verified in ROM");
    rom[0x40104] = 0xcd;
    io[0x47] = 255;
    check(!dex_state::data(&ctx), "palette transitions use existing compositor");
    io[0x47] = 0xe4;
    set(battle::IsInBattle, 1);
    check(!dex_state::data(&ctx), "battle presentation untouched");
    set(battle::IsInBattle, 0);
    set(dex_state::Current, 0);
    check(!dex_state::data(&ctx), "invalid species rejected");
    set(dex_state::Current, 84);
    vram[0x1821] = 0;
    check(!dex_state::data(&ctx), "partial VRAM transfer rejected");
    vram[0x1821] = 42;
    std::copy_n(vram.data() + 0x1800, 1024, vram.data() + 0x1c00);
    io[0x40] = 0xe1;
    io[0x4b] = 7;
    vram[0x1821] = 0;
    check(dex_state::data(&ctx), "visible window tilemap takes precedence over hidden BG");
    io[0x4a] = 8;
    check(!dex_state::data(&ctx), "non-fullscreen window does not hide bad BG");
    check(!dex_state::data(nullptr), "null context safe");
    std::fill(ram.begin(), ram.end(), 0);
    io[0x40] = 0xe3;
    io[0x47] = 0xe4;
    set(0xdff0, 0x40);
    set(0xdff1, 0x40);
    set(0xc3a0 + 14, 0x71);
    set(0xc3a0 + 21, 0x82);
    set(0xc3a0 + 36, 0x92);
    set(0xc3a0 + 96, 0x8e);
    set(0xcc26, 2);
    set(0xcc36, 20);
    auto selected = dex_state::list(&ctx);
    check(selected.number == 23 && selected.registration == dex_state::Registration::Absent,
          "list selection is cursor plus scroll, independent of temporary PokedexNum");
    set(dex_state::Seen + 2, 64);
    selected = dex_state::list(&ctx);
    check(selected.registration == dex_state::Registration::Seen && selected.seen == 1 &&
              !selected.caught,
          "seen-only bit selects silhouette");
    set(dex_state::Owned + 2, 64);
    selected = dex_state::list(&ctx);
    check(selected.registration == dex_state::Registration::Caught && selected.caught == 1,
          "owned bit selects full portrait");
    set(0xdff0, 0x64);
    set(dex_state::Current, 84);
    set(0xcc26, 4);
    selected = dex_state::list(&ctx);
    check(selected.number == 25 && selected.side,
          "side menu keeps species while its own cursor moves");
    set(0xc3a0 + 14, 0);
    check(!dex_state::list(&ctx).number, "live list return without matching layout falls back");
    std::puts("PASS: ROM-validated dex data lifetime, species, mirrored tiles and visible VRAM");
}
