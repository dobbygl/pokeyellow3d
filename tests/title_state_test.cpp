#include "title_state.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static void check(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
int main(int argc, char **argv) {
    std::vector<uint8_t> rom(1048576), wram(32768), vram(16384), hram(128), io(128);
    if (argc == 2) {
        FILE *file = std::fopen(argv[1], "rb");
        check(file && std::fread(rom.data(), 1, rom.size(), file) == rom.size(), "ROM fixture");
        std::fclose(file);
    } else {
        for (size_t i = 0xf46fb; i < 0xf525b; ++i)
            rom[i] = uint8_t(i * 17 + i / 13);
        constexpr uint8_t audio_loop[]{0xfa, 0xc6, 0xcf, 0xa7, 0x20, 0xfa, 0xc3, 0x10, 0x1d};
        std::memcpy(rom.data() + 0x4321, audio_loop, sizeof(audio_loop));
        for (auto call : {title_state::Call{0x42aa, 0x1e64},
                          {0x5c36, 0x3aab},
                          {0x5c64, 0x5d1f},
                          {0x5cd7, 0x5e85}}) {
            rom[call.address] = 0xcd;
            rom[call.address + 1] = uint8_t(call.target);
            rom[call.address + 2] = uint8_t(call.target >> 8);
        }
    }
    GBContext ctx{};
    ctx.rom = rom.data();
    ctx.rom_size = rom.size();
    ctx.wram = wram.data();
    ctx.vram = vram.data();
    ctx.hram = hram.data();
    ctx.io = io.data();
    ctx.sp = 0xdff9;
    auto root = [&](uint16_t address) {
        wram[0x1ffd] = uint8_t(address);
        wram[0x1ffe] = uint8_t(address >> 8);
    };
    using Phase = title_state::Phase;
    check(title_state::sample(&ctx) == Phase::None, "empty machine stays native");
    root(0x42ad);
    check(title_state::sample(&ctx) == Phase::None, "return without logo is insufficient");
    std::memcpy(vram.data() + 0x1000, rom.data() + 0xf46fb, 0x730);
    std::memcpy(vram.data() + 0x800, rom.data() + 0xf4e5b, 0x400);
    check(title_state::sample(&ctx) == Phase::Title, "verified title wait");
    vram[0x800] ^= 1;
    check(title_state::sample(&ctx) == Phase::None, "vTitleLogo backup rejects stale lettering");
    vram[0x800] ^= 1;
    rom[0x42ab] ^= 1;
    check(title_state::sample(&ctx) == Phase::None, "CALL target verified against ROM");
    rom[0x42ab] ^= 1;
    ctx.sp = 0xdfff;
    check(title_state::sample(&ctx) == Phase::None, "popped return is not live");
    ctx.pc = 0x4324;
    hram[0x38] = 1;
    wram[0xfc6] = 12;
    check(title_state::sample(&ctx) == Phase::Title, "ROM-verified busy idle-reset audio loop");
    hram[0x38] = 2;
    check(title_state::sample(&ctx) == Phase::None, "audio loop requires active title bank");
    hram[0x38] = 1;
    wram[0xfc6] = 0;
    check(title_state::sample(&ctx) == Phase::None, "finished audio fade cannot retain title");
    ctx.pc = 0;
    ctx.sp = 0xdffa;
    check(title_state::sample(&ctx) == Phase::None, "unaligned stack rejected");
    ctx.sp = 0xdff9;
    root(0x1db3);
    wram[0x1ff9] = 0xad;
    wram[0x1ffa] = 0x42;
    check(title_state::sample(&ctx) == Phase::None, "nested saved register is not title root");
    root(0x5c39);
    check(title_state::sample(&ctx) == Phase::None, "menu requires save-file status");
    wram[0x1087] = 1;
    check(title_state::sample(&ctx) == Phase::Menu, "menu without a saved game");
    wram[0x1087] = 2;
    check(title_state::sample(&ctx) == Phase::Menu, "menu with a saved game");
    root(0x5c67);
    check(title_state::sample(&ctx) == Phase::Continue, "saved-game summary");
    wram[0x1087] = 1;
    check(title_state::sample(&ctx) == Phase::None, "continue requires a valid saved game");
    root(0x5cda);
    check(title_state::sample(&ctx) == Phase::NewGame, "Oak speech and naming lifetime");
    root(0x5cfb);
    check(title_state::sample(&ctx) == Phase::None, "map loader is no longer a title menu");
    root(0x1db3);
    check(title_state::sample(&ctx) == Phase::None,
          "intro stays native despite stale logo and save");
    check(title_state::sample(nullptr) == Phase::None, "null context");
    std::puts("PASS: title, menu, continue, naming and stale/invalid lifetime rejection");
}
