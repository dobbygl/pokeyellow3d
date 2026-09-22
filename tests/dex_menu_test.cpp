#include "dex_menu_state.h"
#include "synthetic_context.h"
#include <cstdio>
#include <stdexcept>

int main() {
    try {
        auto check = [](bool ok, const char *message) {
            if (!ok)
                throw std::runtime_error(message);
        };
        synthetic::Context machine;
        auto *ctx = &machine.ctx;
        machine.io[0x40] = 0xe3;
        machine.io[0x47] = 0xe4;
        auto *tiles = machine.wram.data() + 0x3a0;
        std::fill_n(tiles, 360, 0x7f);
        for (int y = 0; y < 18; ++y)
            tiles[y * 20 + 14] = uint8_t((y == 0 || y % 2) ? 0x71 : 0x70);
        for (int x = 15; x < 20; ++x)
            tiles[6 * 20 + x] = 0x7a;
        tiles[21] = 0x82;
        tiles[36] = 0x92;
        tiles[96] = 0x8e;
        tiles[60] = 0xed;
        tiles[63] = 0x72;
        machine.wram[0xc26] = machine.wram[0xc36] = 0;
        machine.wram[0xc28] = 6;
        machine.wram[battle::IsInBattle - 0xc000] = 0;
        ctx->sp = 0xdffc;
        machine.write(0xdffc, 0x40);
        machine.write(0xdffd, 0x40); // bank 10:403D CALL 4140 -> return 4040
        machine.image[0x4003d] = 0xcd;
        machine.image[0x4003e] = 0x40;
        machine.image[0x4003f] = 0x41;
        machine.image[0x410b1] = 1;
        auto font = rom_font::decode(ctx->rom, ctx->rom_size);
        for (int tile = 128; tile < 256; ++tile)
            for (int y = 0; y < 8; ++y)
                for (int p = 0; p < 2; ++p)
                    machine.vram[0x800 + (tile - 128) * 16 + y * 2 + p] =
                        font.rows[(tile - 128) * 8 + y];
        for (int tile : {0x70, 0x71, 0x72}) {
            size_t source = tile == 0x72 ? 0x3aa28 : 0x11018 + (tile - 0x60) * 16;
            std::copy_n(ctx->rom + source, 16, machine.vram.data() + 0x1000 + tile * 16);
        }
        auto original = machine.wram;
        auto snapshot = dex_menu::sample(ctx, font);
        check(snapshot.ready && snapshot.layout.count == 4 && snapshot.caught[0] &&
                  snapshot.text[63] == 0x7f && machine.wram == original,
              "four regions and original caught marker without memory writes");
        for (int cell : {54, 135, 42}) {
            auto saved = tiles[cell];
            tiles[cell] = 0x73;
            check(!dex_menu::sample(ctx, font).ready,
                  "unknown separator, side background or list graphic rejects entire page");
            tiles[cell] = saved;
        }
        machine.vram[0x1720] ^= 1;
        check(!dex_menu::sample(ctx, font).ready, "replaced caught graphic rejected");
        machine.vram[0x1720] ^= 1;
        machine.vram[0x820] ^= 1;
        check(!dex_menu::sample(ctx, font).ready, "replaced used font rejected");
        machine.vram[0x820] ^= 1;
        machine.wram[0xc26] = 7;
        check(!dex_menu::sample(ctx, font).ready, "out-of-range original selection rejected");
        machine.wram[0xc26] = 0;
        ctx->sp = 0xdffe;
        check(!dex_menu::sample(ctx, font).ready, "popped original menu call rejected");
        check(!dex_menu::sample(nullptr, font).ready, "missing machine rejected");
        std::puts("PASS: Pokedex regions, caught graphic, source isolation and atomic rejection");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    return 0;
}
