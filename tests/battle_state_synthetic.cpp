#include "synthetic_context.h"
#include <array>
#include <cstdio>
#include <string>

// Covers src/battle_state.h against the hand-built GBContext: every condition
// battle::ready enumerates, the portrait rectangles, the HUD names at their
// three centring offsets, the procedural portrait decode and the live-return
// probe that recognises a running animation. No cartridge is involved.
static void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}

int main() {
    try {
        const auto &plan = synthetic::layout();
        synthetic::Context machine;
        GBContext *ctx = &machine.ctx;
        const int enemy_species = 25, player_species = 1;

        machine.reset();
        machine.place_player(plan.home, 4, 4, 0);
        machine.start_battle(enemy_species, player_species);

        // A procedural compressed 1x1 portrait, independent of cartridge data.
        // Each source row is AA in the first stream, 55 in the second. Their
        // differential decode is CC/66; modes 1 and 2 XOR the second plane.
        for (int mode = 0; mode < 3; mode++)
            for (int first = 0; first < 2; first++) {
                std::vector<uint8_t> packed{0x11};
                size_t bit = 8;
                auto put = [&](unsigned value, int n) {
                    for (int b = n - 1; b >= 0; b--) {
                        if (bit / 8 == packed.size())
                            packed.push_back(0);
                        packed[bit / 8] |= ((value >> b) & 1) << (7 - bit % 8);
                        ++bit;
                    }
                };
                put(first, 1);
                put(1, 1);
                for (int i = 0; i < 32; i++)
                    put(2, 2);
                if (mode == 0)
                    put(0, 1);
                else {
                    put(1, 1);
                    put(mode - 1, 1);
                }
                put(1, 1);
                for (int i = 0; i < 32; i++)
                    put(1, 2);
                auto decoded = mon_pic::decompress(packed.data(), packed.size());
                auto flipped = mon_pic::decompress(packed.data(), packed.size(), true);
                check(decoded.valid && flipped.valid && decoded.mode == mode,
                      "procedural compressed portrait modes");
                for (int x = 0; x < 7; x++)
                    for (int y = 0; y < 56; y++)
                        for (int plane = 0; plane < 2; plane++) {
                            int expected = 0;
                            if (x == 3 && y >= 48)
                                expected = plane == first ? 0xcc
                                           : mode == 0    ? 0x66
                                           : mode == 1    ? 0x99
                                                          : 0xaa;
                            size_t i = (x * 56 + y) * 2 + plane;
                            check(decoded.tiles[i] == expected,
                                  "independent expected pixels, alignment and plane order");
                            int mirrored = expected == 0xcc   ? 0x33
                                           : expected == 0xaa ? 0x55
                                                              : expected;
                            check(flipped.tiles[i] == mirrored, "compressed portrait mirror");
                        }
                check(!mon_pic::decompress(packed.data(), packed.size() - 2).valid,
                      "truncated compressed stream rejected");
            }

        // --- normal() and ready() --------------------------------------------
        check(battle::normal(ctx), "a wild battle with both parties is normal");
        check(battle::ready(ctx), "the synthetic arena satisfies every ready() condition");
        check(!battle::trainer_intro(ctx), "a wild battle is not a trainer introduction");

        machine.write(battle::Link, 1);
        check(!battle::normal(ctx) && !battle::ready(ctx), "a link battle is never composed");
        machine.write(battle::Link, 0);
        machine.write(battle::BattleType, 1);
        check(!battle::normal(ctx) && !battle::ready(ctx),
              "the old man tutorial is never composed");
        machine.write(battle::BattleType, 0);
        machine.write(battle::IsInBattle, 3);
        check(!battle::normal(ctx), "only wild (1) and trainer (2) battles are normal");
        machine.write(battle::IsInBattle, 1);
        check(battle::normal(ctx) && battle::ready(ctx), "the normal wild battle is back");

        machine.lcd_on(false);
        check(!battle::ready(ctx), "an off LCD is never composed");
        machine.lcd_on(true);
        machine.write(0xd11c, 1);
        check(!battle::ready(ctx), "a full-screen list over the arena is refused");
        machine.write(0xd11c, 0);
        machine.write_hp(0xcff3, 0);
        check(!battle::ready(ctx), "an enemy without a maximum HP word is refused");
        machine.write_hp(0xcff3, 24);
        machine.write(0xd013, 0);
        check(!battle::ready(ctx), "a player without a species is refused");
        machine.write(0xd013, player_species);
        machine.io[0x42] = 1;
        check(!battle::ready(ctx), "a scrolled background is refused");
        machine.io[0x42] = 0;
        machine.io[0x43] = 1;
        check(!battle::ready(ctx), "an unexpected horizontal scroll is refused");
        machine.io[0x43] = 2;
        check(battle::ready(ctx), "SCX = 2 is the other accepted alignment");
        machine.io[0x43] = 0;

        // --- rectangle(), with and without the displayed check ----------------
        check(battle::rectangle(ctx, battle::Enemy), "the enemy rectangle is in the tile map");
        check(battle::rectangle(ctx, battle::Enemy, true), "the enemy rectangle is also on screen");
        check(battle::rectangle(ctx, battle::Player), "the player rectangle is in the tile map");
        check(battle::rectangle(ctx, battle::Player, true),
              "the player rectangle is also on screen");
        uint8_t saved_map = machine.vram[0x1800 + 3 * 32 + 14];
        machine.vram[0x1800 + 3 * 32 + 14] = 0xff;
        check(battle::rectangle(ctx, battle::Enemy), "the tile map alone is still intact");
        check(!battle::rectangle(ctx, battle::Enemy, true),
              "a BG map that does not show the picture");
        machine.vram[0x1800 + 3 * 32 + 14] = saved_map;
        machine.tile(14, 3, 0);
        check(!battle::rectangle(ctx, battle::Enemy),
              "a hole in the tile map breaks the rectangle");
        check(!battle::ready(ctx), "and with it the whole battle view");
        machine.tile(14, 3, battle::Enemy.base + 2 * 7 + 3);
        check(battle::ready(ctx), "restoring the tile restores the battle");

        // --- name_matches() at the three CenterMonName offsets ----------------
        for (const char *name : {"PI", "PIKA", "PIKACH"}) {
            machine.clear_names();
            machine.write_name(0xcfd9, name, 1, 0);
            machine.write_name(0xd008, name, 10, 7);
            check(battle::name_matches(ctx, 0xcfd9, 1, 0),
                  "the enemy name sits where CenterMonName put it");
            check(battle::name_matches(ctx, 0xd008, 10, 7),
                  "the player name sits where CenterMonName put it");
            check(battle::ready(ctx), "names of any length keep the battle ready");
            check(battle::mon(ctx, true).name == name, "the name decodes back out of WRAM");
        }
        machine.clear_names();
        machine.write_name(0xcfd9, "PIKACH", 1, 0);
        machine.write_name(0xd008, "BULBAS", 10, 7);
        machine.tile(1, 0, 0);
        check(!battle::name_matches(ctx, 0xcfd9, 1, 0), "a HUD without the name is refused");
        machine.tile(1, 0, 0x80 + 'P' - 'A');
        machine.write(0xcfd9, 0x50);
        check(!battle::name_matches(ctx, 0xcfd9, 1, 0), "an empty name matches nothing");
        machine.write_name(0xcfd9, "PIKACH", 1, 0);
        check(battle::ready(ctx), "both names are back in place");

        // --- palette(): the image carries no pokedex table ---------------------
        check(ctx->rom[0x410b1 + enemy_species - 1] == 0,
              "the generator leaves the pokedex number table at 0x410b1 zeroed");
        check(battle::dex(ctx, enemy_species) == 0, "a zero entry means no dex number");
        const std::array<std::array<uint8_t, 3>, 4> monochrome{
            {{255, 255, 255}, {170, 170, 170}, {85, 85, 85}, {0, 0, 0}}};
        check(battle::palette(ctx, enemy_species) == monochrome,
              "species without a dex number keep the default ramp");
        check(battle::palette(ctx, player_species) == monochrome, "so does the player's species");

        // --- portrait(): the procedural VRAM pattern ---------------------------
        auto front = battle::portrait(ctx, battle::Enemy, enemy_species);
        auto back = battle::portrait(ctx, battle::Player, player_species);
        auto alpha = [](const battle::Image &image, int x, int y) {
            return image[size_t(y * 56 + x) * 4 + 3];
        };
        size_t front_clear = 0, back_clear = 0;
        for (int y = 0; y < 56; y++)
            for (int x = 0; x < 56; x++) {
                size_t i = size_t(y * 56 + x) * 4;
                uint8_t expected = uint8_t(255 - 85 * synthetic::portrait_index(x, y));
                check(front[i] == expected && front[i + 1] == expected && front[i + 2] == expected,
                      "the front picture decodes the pattern through the default ramp");
                check(back[i] == expected && back[i + 1] == expected && back[i + 2] == expected,
                      "the back picture decodes the same pattern");
                front_clear += !front[i + 3];
                back_clear += !back[i + 3];
            }
        check(!alpha(front, 0, 0) && !alpha(front, 55, 0) && !alpha(front, 2, 30) &&
                  !alpha(front, 53, 30),
              "the background around the body is transparent");
        check(!alpha(back, 0, 0) && !alpha(back, 55, 55),
              "the back picture clears its background too");
        check(alpha(front, 8, 20) == 255 && alpha(back, 8, 20) == 255,
              "the outline is never cleared");
        check(alpha(front, 20, 20) == 255 && alpha(back, 20, 20) == 255,
              "white enclosed by the outline stays opaque");
        check(alpha(front, 35, 32) == 255 && alpha(front, 20, 45) == 255,
              "shaded patches stay opaque");
        check(!alpha(front, 28, 52), "the front body leaks through the open bottom edge");
        check(alpha(back, 28, 52) == 255, "the back crop closes that edge before the flood fill");
        check(back_clear < front_clear, "the crop leaves strictly fewer transparent pixels");
        // The body box of portrait_index spans x 8..47 and y 8..55, so once the
        // crop seals its open bottom exactly the surrounding background clears.
        check(back_clear == 56 * 56 - 40 * 48,
              "the back picture clears the background and nothing else");

        // --- mon() and status() -------------------------------------------------
        auto enemy = battle::mon(ctx, true);
        auto player = battle::mon(ctx, false);
        check(enemy.species == enemy_species && enemy.hp == 18 && enemy.max_hp == 24,
              "enemy species and HP words");
        check(enemy.level == 7 && enemy.hp_color == 1 && enemy.name == "PIKACH",
              "enemy level, bar colour and name");
        check(player.species == player_species && player.hp == 26 && player.max_hp == 31,
              "player species and HP words");
        check(player.level == 9 && player.hp_color == 1 && player.name == "BULBAS",
              "player level, bar colour and name");
        check(std::string(battle::status(enemy.status)).empty(), "a healthy mon has no status");
        for (auto probe : {std::pair<int, const char *>{1, "SLP"},
                           {7, "SLP"},
                           {8, "PSN"},
                           {16, "BRN"},
                           {32, "FRZ"},
                           {64, "PAR"}}) {
            machine.write(0xcfe8, probe.first);
            check(std::string(battle::status(battle::mon(ctx, true).status)) == probe.second,
                  "status name");
        }
        machine.write(0xcfe8, 0);

        // --- animation_running() and live_return() --------------------------------
        check(!battle::animation_running(ctx), "no animation while the id byte is clear");
        machine.write(battle::Animation, 0x0c);
        check(!battle::animation_running(ctx),
              "an animation id alone proves nothing: the ids are unions");
        machine.arm_live_return(0x3f09b, 0x3eb4);
        check(battle::animation_running(ctx),
              "the verified CALL plus its return address on the stack");
        machine.image[0x3f09c] = 0;
        check(!battle::animation_running(ctx), "a CALL to another address is a different site");
        machine.image[0x3f09c] = 0xb4;
        check(battle::animation_running(ctx), "and the site is recognised again");
        check(battle::ready(ctx), "a running animation keeps the arena composed");
        machine.bgp(0x1b);
        machine.io[0x42] = 40;
        check(battle::ready(ctx), "its palette flash and LCD scroll are tolerated");
        machine.bgp(0xe4);
        machine.io[0x42] = 0;
        uint16_t stack = ctx->sp;
        ctx->sp = 0xc800;
        check(!battle::animation_running(ctx), "a stack pointer outside WRAM bank 1 is refused");
        ctx->sp = stack;
        machine.disarm_live_return(0x3f09b);
        check(!battle::animation_running(ctx), "clearing the return address ends the animation");
        machine.write(battle::Animation, 0);
        check(battle::ready(ctx), "the arena is composed again without an animation");

        // --- displayed(): the message box on the LCD layer ---------------------------
        // A procedural bottom box in wTileMap and in both BG maps. Effect
        // animations may present it integrated only while the displayed layer
        // shows these exact cells.
        for (int y = 12; y < 18; y++)
            for (int x = 0; x < 20; x++) {
                int value = y == 12 || y == 17 || !x || x == 19 ? 0x79 + (x + y) % 6
                                                                : 0x80 + (x * 5 + y) % 26;
                machine.tile(x, y, value);
                machine.vram[size_t(0x1800 + y * 32 + x)] = uint8_t(value);
                machine.vram[size_t(0x1c00 + y * 32 + x)] = uint8_t(value);
            }
        auto before = machine.wram;
        check(battle::displayed(ctx, 0, 12, 20, 6), "the BG map shows the tile-map message box");
        check(before == machine.wram, "displayed() reads memory only");
        check(!battle::displayed(ctx, 0, 12, 20, 7) && !battle::displayed(ctx, -1, 12, 20, 6) &&
                  !battle::displayed(ctx, 0, 12, 0, 6),
              "areas outside the 20x18 screen are refused");
        machine.vram[0x1800 + 15 * 32 + 7] ^= 0x40;
        check(!battle::displayed(ctx, 0, 12, 20, 6),
              "a BG cell awaiting the auto transfer is not displayed");
        check(battle::displayed(ctx, 8, 12, 12, 6), "cells outside the checked area are ignored");
        machine.vram[0x1800 + 15 * 32 + 7] ^= 0x40;
        machine.io[0x42] = 7;
        machine.io[0x43] = 6;
        check(battle::displayed(ctx, 0, 12, 20, 6), "a sub-tile shake shows the same tiles");
        machine.io[0x42] = 8;
        check(!battle::displayed(ctx, 0, 12, 20, 6), "a whole-tile BG scroll shows other cells");
        machine.io[0x42] = 0;
        machine.io[0x43] = 0;
        machine.io[0x40] |= 0x08;
        machine.vram[0x1c00 + 13 * 32 + 3] ^= 1;
        check(!battle::displayed(ctx, 0, 12, 20, 6), "the selected 9C00 BG map is compared");
        machine.vram[0x1c00 + 13 * 32 + 3] ^= 1;
        check(battle::displayed(ctx, 0, 12, 20, 6), "and accepted once it holds the box");
        machine.io[0x40] &= uint8_t(~0x08);
        // The battle normally presents the window at WX=7, WY=0 from 9C00.
        machine.io[0x40] |= 0x60;
        machine.vram[0x1800 + 14 * 32 + 2] ^= 1;
        check(battle::displayed(ctx, 0, 12, 20, 6),
              "the full-screen window hides a stale BG map");
        machine.io[0x4b] = 11;
        machine.io[0x4a] = 3;
        check(battle::displayed(ctx, 0, 12, 20, 6), "a shaken window still shows the same box");
        machine.vram[0x1c00 + 16 * 32 + 18] ^= 1;
        check(!battle::displayed(ctx, 0, 12, 20, 6), "a window map mismatch is refused");
        machine.vram[0x1c00 + 16 * 32 + 18] ^= 1;
        machine.io[0x4a] = 0x90;
        check(!battle::displayed(ctx, 0, 12, 20, 6),
              "a hidden window falls back to the stale BG map");
        machine.vram[0x1800 + 14 * 32 + 2] ^= 1;
        check(battle::displayed(ctx, 0, 12, 20, 6), "which is accepted once it is current");
        machine.io[0x4a] = 0;
        machine.io[0x4b] = 7;
        machine.io[0x40] &= uint8_t(~0x60);
        machine.io[0x40] &= uint8_t(~0x01);
        check(!battle::displayed(ctx, 0, 12, 20, 6), "a disabled BG shows no message");
        machine.io[0x40] |= 0x01;
        machine.lcd_on(false);
        check(!battle::displayed(ctx, 0, 12, 20, 6), "an off LCD shows no message");
        machine.lcd_on(true);
        check(battle::displayed(ctx, 0, 12, 20, 6) && battle::ready(ctx),
              "the message box and the arena are back");

        // --- the trainer introduction path ------------------------------------------
        machine.write(battle::IsInBattle, 2);
        machine.write(0xd030, 1);
        machine.write(0xcfe7, 255);
        check(battle::trainer_intro(ctx) && battle::ready(ctx),
              "the trainer introduction is composed");
        machine.write(0xcfe7, 0);
        check(!battle::trainer_intro(ctx), "sending the first mon ends the introduction");
        check(battle::ready(ctx), "and the ordinary battle conditions take over");

        std::fprintf(
            stderr,
            "PASS: ready() conditions, both portrait rectangles, three name lengths, the %zux%zu "
            "procedural portrait (%zu / %zu cleared pixels) and the live-return animation probe\n",
            size_t(battle::PortraitSize), size_t(battle::PortraitSize), front_clear, back_clear);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
