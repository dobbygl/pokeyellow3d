#include "pokemon_menu_state.h"
#include "synthetic_context.h"
#include <cstdio>
#include <stdexcept>

static void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    try {
        synthetic::Context machine;
        auto *ctx = &machine.ctx;
        auto &rom = machine.image;
        auto &vram = machine.vram;
        using pokemon_menu::Kind;
        machine.io[0x40] = 0xe3;
        machine.io[0x42] = 48;
        check(pokemon_menu::ready(ctx), "full-screen window hides scrolled overworld BG");
        machine.io[0x4a] = 8;
        check(!pokemon_menu::ready(ctx), "partial window does not hide scrolled background");
        machine.io[0x4a] = 0;
        check(pokemon_menu::context(nullptr) == Kind::None, "null context safe");
        struct Call {
            size_t address;
            uint16_t target, returned;
            Kind kind;
        };
        for (const auto call : {Call{0x1230, 0x3aaf, 0x1233, Kind::Party},
                                Call{0x11c95, 0x3aab, 0x5c98, Kind::Actions},
                                Call{0x1161d, 0x3852, 0x5620, Kind::Stats},
                                // Portrait decompression, cry and Pikachu's FarCall'd clip.
                                Call{0x115e1, 0x3de0, 0x55e4, Kind::Stats},
                                Call{0x115e7, 0x1144, 0x55ea, Kind::Stats},
                                Call{0x1161a, 0x118b, 0x561d, Kind::Stats},
                                Call{0x11612, 0x3e84, 0x5615, Kind::Stats},
                                Call{0x11814, 0x3852, 0x5817, Kind::Moves}}) {
            rom[call.address] = 0xcd;
            rom[call.address + 1] = uint8_t(call.target);
            rom[call.address + 2] = uint8_t(call.target >> 8);
            ctx->sp = 0xdffd; // Real stacks are not necessarily even-aligned.
            machine.write(0xdffd, call.returned & 255);
            machine.write(0xdffe, call.returned >> 8);
            check(pokemon_menu::context(ctx) == call.kind,
                  "live verified ROM call identifies screen");
            ctx->sp = 0xdfff;
            check(pokemon_menu::context(ctx) == Kind::None,
                  "popped stack bytes cannot identify screen");
            ctx->sp = 0xdffd;
            rom[call.address + 1] ^= 1;
            check(pokemon_menu::context(ctx) == Kind::None, "changed CALL target rejected");
            rom[call.address + 1] ^= 1;
        }
        machine.write(battle::IsInBattle, 1);
        rom[0x3d236] = 0xcd;
        rom[0x3d237] = 0xab;
        rom[0x3d238] = 0x3a;
        machine.write(0xdffd, 0x39);
        machine.write(0xdffe, 0x52);
        check(pokemon_menu::context(ctx) == Kind::Actions, "original battle action menu call");
        machine.write(battle::IsInBattle, 0);
        check(pokemon_menu::context(ctx) == Kind::None,
              "battle action return requires battle context");
        // Messages printed over the still-displayed battle party list, after
        // HandlePartyMenuInput returned. Each is a bank 0F CALL to PrintText.
        auto arm = [&](size_t address, uint16_t target) {
            rom[address] = 0xcd;
            rom[address + 1] = uint8_t(target);
            rom[address + 2] = uint8_t(target >> 8);
        };
        auto push = [&](int slot, uint16_t returned) {
            machine.write(slot, returned & 255);
            machine.write(slot + 1, returned >> 8);
        };
        for (int a = 0xdff0; a < 0xe000; ++a)
            machine.write(a, 0);
        ctx->sp = 0xdff9;
        for (const auto call : {Call{0x3ca71, 0x3c36, 0x4a74, Kind::Party},
                                Call{0x3d29e, 0x3c36, 0x52a1, Kind::Party}}) {
            arm(call.address, call.target);
            push(0xdff9, call.returned);
            machine.write(battle::IsInBattle, 2);
            check(pokemon_menu::context(ctx) == Kind::Party,
                  "AlreadyOutText over the battle party list");
            machine.write(battle::IsInBattle, 0);
            check(pokemon_menu::context(ctx) == Kind::None,
                  "AlreadyOutText return requires battle context");
            machine.write(battle::IsInBattle, 2);
            machine.write(battle::Link, 1);
            check(pokemon_menu::context(ctx) == Kind::None, "link battle is never recognized");
            machine.write(battle::Link, 0);
            rom[call.address + 1] ^= 1;
            check(pokemon_menu::context(ctx) == Kind::None, "changed PrintText target rejected");
            rom[call.address + 1] ^= 1;
            push(0xdff9, 0);
            check(pokemon_menu::context(ctx) == Kind::None,
                  "returned PrintText identifies nothing");
        }
        // NoWillText is printed by HasMonFainted; only its party callers count.
        arm(0x3cb14, 0x3c36);
        push(0xdff9, 0x4b17);
        check(pokemon_menu::context(ctx) == Kind::None,
              "HasMonFainted text without a verified party caller");
        for (const auto call : {Call{0x3c84b, 0x4afc, 0x484e, Kind::Party},
                                Call{0x3ca79, 0x4afc, 0x4a7c, Kind::Party},
                                Call{0x3d2a4, 0x4afc, 0x52a7, Kind::Party},
                                Call{0x3c1c7, 0x4afc, 0x41ca, Kind::None}}) {
            arm(call.address, call.target);
            push(0xdffb, call.returned);
            check(pokemon_menu::context(ctx) == call.kind,
                  "NoWillText over the forced, trainer and voluntary party lists only");
            push(0xdff9, 0);
            check(pokemon_menu::context(ctx) == Kind::None,
                  "caller alone does not identify the message");
            push(0xdff9, 0x4b17);
            push(0xdffb, 0);
        }
        machine.write(battle::IsInBattle, 0);
        for (int a = 0xdff0; a < 0xe000; ++a)
            machine.write(a, 0);
        // 78 is a PC box pictogram but a vertical line on the summary; 6E
        // changes source too. Neither source may leak into the other profile.
        for (int row = 0; row < 8; ++row) {
            rom[0x10c18 + row] = uint8_t(0x80 >> row);
            rom[0x3aa28 + row * 2] = 0xaa;
            rom[0x3aa29 + row * 2] = 0x55;
            vram[0x1780 + row * 2] = vram[0x1781 + row * 2] = uint8_t(0x80 >> row);
        }
        check(pokemon_menu::graphic_matches(ctx, Kind::Stats, 0x78),
              "summary uses original 1bpp line");
        check(!menu_graphics::matches(0x78, vram.data(), rom.data(), rom.size()),
              "PC profile stays isolated");
        vram[0x1781] ^= 1;
        check(!pokemon_menu::graphic_matches(ctx, Kind::Stats, 0x78),
              "second bitplane mismatch rejected");
        check(!pokemon_menu::graphic_matches(ctx, Kind::Party, 0x61), "unknown graphic rejected");
        auto *tiles = machine.wram.data() + 0x3a0;
        std::fill_n(tiles, 360, 0x7f);
        tiles[20 + 4] = 0x71;
        tiles[20 + 5] = 0x62;
        tiles[20 + 12] = 0x6c;
        for (int pixels : {0, 1, 9, 47, 48}) {
            for (int i = 0; i < 6; ++i)
                tiles[20 + 6 + i] = uint8_t(0x63 + std::clamp(pixels - i * 8, 0, 8));
            pokemon_menu::Snapshot snapshot;
            check(pokemon_menu::health(ctx, snapshot, 6, 1, 0x6c, 2) &&
                      snapshot.bars[0].pixels == pixels && snapshot.bars[0].color == 2,
                  "HP reads the original painted pixel count, including fainted and one-pixel HP");
        }
        tiles[26] = 0x63;
        pokemon_menu::Snapshot malformed;
        check(!pokemon_menu::health(ctx, malformed, 6, 1, 0x6c, 0),
              "discontinuous HP bar rejected");
        rom[0x410b1] = 1;
        rom[0x383de + 19] = 0;
        rom[0x58e73] = 0x11; // Procedural medium-fast curve: level cubed.
        rom[0x58e74] = rom[0x58e75] = rom[0x58e76] = 0;
        machine.write(0xcf91, 0);
        machine.write(0xcfb8, 7);
        machine.write(0xd162, 1);
        machine.write(0xd89b, 1);
        machine.write(0xda7f, 1);
        for (int location = 0; location < 4; ++location) {
            int records[]{0xd16a, 0xd8a3, 0xda95, 0xda5e};
            int record = records[location];
            machine.write(0xcc49, location);
            machine.write(record, 1);
            machine.write(record + 14, 0);
            machine.write(record + 15, 1);
            machine.write(record + 16, 175); // 431 total; 81 remaining to level eight.
            machine.write(0xcfa5, 0);
            machine.write(0xcfa6, 0);
            machine.write(0xcfa7, 81);
            pokemon_menu::Snapshot snapshot;
            snapshot.species = 1;
            auto before = machine.wram;
            check(pokemon_menu::experience(ctx, snapshot) && snapshot.experience == 431 &&
                      std::abs(snapshot.experience_fraction - 88.f / 169) < .00001f,
                  "all four source records retain total XP while loaded record holds remaining XP");
            check(before == machine.wram, "experience decoder does not write source memory");
            machine.write(record, 2);
            check(!pokemon_menu::experience(ctx, snapshot), "source species mismatch rejected");
        }
        machine.write(0xcc49, 0);
        machine.write(0xcf91, 6);
        pokemon_menu::Snapshot snapshot;
        snapshot.species = 1;
        check(!pokemon_menu::experience(ctx, snapshot), "party index cannot escape six records");
        machine.write(0xcf91, 0);
        machine.write(0xd16a, 1);
        machine.write(0xcfb8, 100);
        machine.write(0xd178, 0x0f);
        machine.write(0xd179, 0x42);
        machine.write(0xd17a, 0x40);
        check(pokemon_menu::experience(ctx, snapshot) && snapshot.experience_fraction == 1,
              "level 100 has a full bar without reading a level 101 curve");
        std::puts(
            "PASS: Pokemon menu context, graphic isolation, HP and persistent XP without ROM");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
