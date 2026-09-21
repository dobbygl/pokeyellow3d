#include "fade_state.h"
#include "menu_state.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
void check(bool value, const char *name) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", name);
        std::exit(1);
    }
}
int main(int argc, char **argv) {
    // home/fade.asm; black steps measured in house-bgp.csv at 9,9,21 frames.
    const std::array<uint8_t, 7> bgps{0xe4, 0xf9, 0xfe, 0xff, 0x90, 0x40, 0x00};
    const float multiply[]{1, .5f, 1.f / 6, 0, .5f, 1.f / 6, 0};
    const float add[]{0, 0, 0, 0, .5f, 5.f / 6, 1};
    for (size_t i = 0; i < bgps.size(); i++) {
        auto t = fade::tone(bgps[i]);
        check(fade::palette(bgps[i]) && std::abs(t.multiply - multiply[i]) < .000001f &&
                  std::abs(t.add - add[i]) < .000001f,
              "palette luminance");
    }
    for (int value = 0; value < 256; value++) {
        auto t = fade::tone(uint8_t(value));
        check(t.multiply >= 0 && t.add >= 0 && t.multiply + t.add <= 1.000001f,
              "bounded affine luminance");
    }
    check(!fade::warp(nullptr), "null context");
    check(argc == 2, "ROM argument");
    FILE *f = std::fopen(argv[1], "rb");
    check(f, "private ROM");
    std::vector<uint8_t> rom(1048576), ram(0x8000);
    check(std::fread(rom.data(), 1, rom.size(), f) == rom.size(), "ROM size");
    std::fclose(f);
    GBContext ctx{};
    ctx.rom = rom.data();
    ctx.rom_size = rom.size();
    ctx.wram = ram.data();
    std::array<uint8_t, 128> hram{};
    ctx.hram = hram.data();
    ctx.sp = 0xdfea;
    check(!fade::warp(&ctx), "stale warp variables are insufficient");
    for (auto pair : std::array<std::array<int, 3>, 10>{{{0x576, 0x1eb6, 0xcd},
                                                         {0x581, 0x6ef, 0xcd},
                                                         {0x5a7, 0x6ef, 0xcd},
                                                         {0x5c9, 0x6ef, 0xcd},
                                                         {0x5d5, 0xfc3, 0xcd},
                                                         {0x1dc, 0xecb, 0xcd},
                                                         {0x1fd, 0x750, 0xc4},
                                                         {0x7ab, 0x7bc, 0xcd},
                                                         {0x7ae, 0x7c4, 0xcd},
                                                         {0x20d, 0x3e84, 0xcd}}}) {
        int call = pair[0], target = pair[1], ret = call + 3;
        check(rom[call] == pair[2] && (rom[call + 1] | rom[call + 2] << 8) == target,
              "canonical CALL target");
        ram[0x1fea] = ret & 255;
        ram[0x1feb] = ret >> 8;
        check(fade::warp(&ctx), "live aligned warp return");
        rom[call] = 0;
        check(!fade::home_call(&ctx, call, target, uint8_t(pair[2])), "ROM instruction validation");
        rom[call] = uint8_t(pair[2]);
        ctx.sp = 0xdff0;
        check(!fade::warp(&ctx), "popped return ignored");
        ctx.sp = 0xdfea;
        ram[0x1fea] = ram[0x1feb] = 0;
    }
    ram[0x1731] = 9;
    hram[0x38] = 1;
    check(!fade::field_loading(&ctx), "field warp flags alone cannot select loading");
    ram[0x1fea] = 0xfb;
    ram[0x1feb] = 0x5c;
    check(fade::field_loading(&ctx) && fade::warp(&ctx), "verified SpecialEnterMap delay");
    hram[0x38] = 2;
    check(!fade::field_loading(&ctx), "same return in a different bank is insufficient");
    hram[0x38] = 1;
    ram[0x1731] = 1;
    check(!fade::field_loading(&ctx), "new game SpecialEnterMap is not a field move");
    ram[0x1731] = 9;
    rom[0x5cf9] ^= 1;
    check(!fade::field_loading(&ctx), "loading target verified against original ROM");
    rom[0x5cf9] ^= 1;
    ctx.sp = 0xdff0;
    check(!fade::field_loading(&ctx), "popped field-load return ignored");
    ctx.sp = 0xdfea;
    ram[0x1fea] = ram[0x1feb] = ram[0x1731] = 0;
    check(!menu_state::running(nullptr), "null menu context");
    for (int call : {0x2de, 0x3f47}) {
        check(rom[call] == 0xcd && (rom[call + 1] | rom[call + 2] << 8) == 0x2817,
              "canonical text dispatch CALL");
        int ret = call + 3;
        ram[0x1fea] = ret & 255;
        ram[0x1feb] = ret >> 8;
        check(menu_state::running(&ctx), "live world menu lifetime");
        ram[0x1fec] = 0;
        ram[0x1fed] = 2;
        check(!fade::warp(&ctx), "StatusScreen register pair is not MapEntryAfterBattle");
        ram[0x1fec] = ram[0x1fed] = 0;
        ram[0x1056] = 1;
        check(!menu_state::running(&ctx), "battle excludes world menu lifetime");
        ram[0x1056] = 0;
        ctx.sp = 0xdff0;
        check(!menu_state::running(&ctx), "popped menu return ignored");
        ctx.sp = 0xdfea;
        rom[call + 1] ^= 1;
        check(!menu_state::running(&ctx), "menu CALL target verified");
        rom[call + 1] ^= 1;
        ram[0x1fea] = ram[0x1feb] = 0;
        check(!menu_state::running(&ctx), "menu requires live dispatch");
    }
    std::puts("PASS: BGP luminance, endpoints and ROM-validated live warp/menu lifetimes");
}
