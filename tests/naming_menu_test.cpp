#include "naming_menu_state.h"
#include "synthetic_context.h"
#include <cstdio>
#include <stdexcept>

namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
void keyboard(synthetic::Context &machine, int type, int letter_case) {
    machine.reset();
    machine.io[0x40] = 0xe3;
    machine.arm_live_return(type == 0 ? 0x66ff : type == 1 ? 0x6747 : 0x62d9, 0x6307);
    machine.write(naming_menu::Type, type);
    machine.write(naming_menu::Case, letter_case);
    machine.write(0xcc25, 1);
    std::fill_n(machine.wram.data() + 0x3a0, 360, 0x7f);
    for (int y = 4; y < 15; ++y)
        for (int x = 0; x < 20; ++x) {
            int edge = menu_layout::border({0, 4, 20, 11}, x, y);
            if (edge)
                machine.tile(x, y, edge);
        }
    // Procedural symbols and patterns, never original cartridge assets.
    size_t alphabet = letter_case ? 25884 : 25940;
    for (int i = 0; i < 55; ++i)
        machine.image[alphabet + i] = uint8_t(0x80 + i % 26);
    machine.image[alphabet + 44] = 0xf0;
    for (int i = 0; i < 45; ++i)
        machine.tile(2 + i % 9 * 2, 5 + i / 9 * 2, machine.image[alphabet + i]);
    for (int i = 0; i < 10; ++i)
        machine.tile(2 + i, 15, machine.image[alphabet + 45 + i]);
    for (int tile : {0x76, 0x77, 0xf0}) {
        size_t source = tile == 0xf0 ? 0x64e5 : 0x10b60 + (tile - 0x76) * 16;
        for (int i = 0; i < (tile == 0xf0 ? 8 : 16); ++i)
            machine.image[source + i] = uint8_t(tile + i * 7);
        for (int i = 0; i < 16; ++i)
            machine.vram[(tile == 0xf0 ? 0xf00 : 0x1000 + tile * 16) + i] =
                machine.image[source + (tile == 0xf0 ? i / 2 : i)];
    }
    for (int x = 10; x < (type == 2 ? 20 : 17); ++x)
        machine.tile(x, 3, x == 10 ? 0x77 : 0x76);
    machine.tile(1, 5, 0xed);
    if (type == 2) {
        auto *entry = machine.image.data() + 0x7184d;
        const uint8_t record[]{0x00, 0x40, 4, 0x20, 0x40, 0x86};
        std::copy_n(record, 6, entry);
        for (int i = 0; i < 64; ++i)
            machine.vram[0x640 + i] = machine.image[0x80000 + i] = uint8_t(i * 13);
        for (int i = 0; i < 4; ++i) {
            machine.oam[i * 4] = uint8_t(16 + i / 2 * 8);
            machine.oam[i * 4 + 1] = uint8_t(16 + i % 2 * 8);
            machine.oam[i * 4 + 2] = uint8_t(100 + i);
        }
    }
}
} // namespace
int main() {
    try {
        synthetic::Context machine;
        auto *ctx = &machine.ctx;
        check(!naming_menu::active(nullptr), "null context");
        for (int type = 0; type < 3; ++type)
            for (int letter_case = 0; letter_case < 2; ++letter_case) {
                keyboard(machine, type, letter_case);
                const auto wram = machine.wram, vram = machine.vram, eram = machine.eram;
                const auto oam = machine.oam;
                const auto io = machine.io, hram = machine.hram;
                auto result = naming_menu::prepare(ctx);
                check(result.ready && result.type == type, "all three original name contexts");
                check(result.graphics[3 * 20 + 10] && result.graphics[13 * 20 + 18] &&
                          result.text[3 * 20 + 10] == 0x7f,
                      "verified graphics masked only in private copy");
                check(wram == machine.wram && vram == machine.vram && eram == machine.eram &&
                          oam == machine.oam && io == machine.io && hram == machine.hram,
                      "snapshot does not write any guest memory");
                machine.tile(1, 5, 0x7f);
                for (int y = 5; y <= 15; y += 2)
                    for (int x = 1; x <= (y == 15 ? 1 : 17); x += 2) {
                        machine.tile(x, y, 0xed);
                        // Zero is used temporarily by the original icon animation.
                        for (int row : {0, (y - 3) / 2}) {
                            machine.write(0xcc26, row);
                            auto cursor = naming_menu::prepare(ctx);
                            check(cursor.ready && cursor.cursor_x == x && cursor.cursor_y == y,
                                  "every painted keyboard cell and animation row zero");
                        }
                        machine.tile(x, y, 0x7f);
                    }
                machine.tile(1, 5, 0xed);
                machine.write(0xcc26, 1);
                for (auto invalid : {std::pair<int, int>{naming_menu::Type, 3},
                                     {naming_menu::Length, type == 2 ? 11 : 8},
                                     {naming_menu::Case, 2},
                                     {0xcc26, 7},
                                     {0xcc25, 0},
                                     {0xcc25, 2},
                                     {0xcc25, 19}}) {
                    auto saved = machine.at(invalid.first);
                    machine.write(invalid.first, invalid.second);
                    check(!naming_menu::prepare(ctx).ready, "invalid dedicated state rejected");
                    machine.write(invalid.first, saved);
                }
                for (auto point : {std::pair<int, int>{0, 4}, {2, 5}, {2, 15}, {10, 3}, {18, 13}}) {
                    auto &tile = machine.at(0xc3a0 + point.second * 20 + point.first);
                    auto saved = tile;
                    tile = 0x7f;
                    check(!naming_menu::prepare(ctx).ready, "border/alphabet/graphic mismatch");
                    tile = saved;
                }
                for (int address : {0xf00, 0x1760, 0x1770}) {
                    machine.vram[address] ^= 1;
                    check(!naming_menu::prepare(ctx).ready, "unverified VRAM graphic rejected");
                    machine.vram[address] ^= 1;
                }
                machine.tile(3, 5, 0xed);
                check(!naming_menu::prepare(ctx).ready, "duplicate cursor rejected");
                machine.tile(3, 5, 0x7f);
                machine.tile(1, 1, 0x60);
                check(!naming_menu::prepare(ctx).ready, "unknown low tile rejected");
                machine.tile(1, 1, 0x7f);
                machine.oam[1] = 17;
                machine.oam[0] = 16;
                check(!naming_menu::prepare(ctx).ready, "foreign sprite geometry rejected");
            }
        keyboard(machine, 2, 0);
        machine.arm_live_return(0x62a3, 0x6307);
        check(naming_menu::prepare(ctx).ready, "captured Pokemon AskName call");
        ctx->sp = 0xdfff;
        check(!naming_menu::active(ctx), "popped return is inactive");
        ctx->sp = 0xdf00;
        machine.image[0x62a4] ^= 1;
        check(!naming_menu::active(ctx), "changed original CALL target is inactive");
        std::puts(
            "PASS: naming contexts, both cases, 46 cursor cells, graphics and read-only fallback");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
