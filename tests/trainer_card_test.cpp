#include "trainer_card_state.h"
#include "synthetic_context.h"
#include <cstdio>
#include <stdexcept>

namespace {
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    try {
        synthetic::Context machine;
        auto *ctx = &machine.ctx;
        machine.io[0x40] = 0xe3;
        machine.image[0x12029] = 0xcd;
        machine.image[0x1202a] = 0x52;
        machine.image[0x1202b] = 0x38;
        ctx->sp = 0xdffd;
        machine.write(0xdffd, 0x2c);
        machine.write(0xdffe, 0x60);
        check(trainer_card::active(ctx), "original card wait call");
        ctx->sp = 0xdfff;
        check(!trainer_card::active(ctx), "returned card is inactive");
        ctx->sp = 0xdffd;
        machine.image[0x1202a] ^= 1;
        check(!trainer_card::active(ctx), "changed call target is inactive");
        machine.image[0x1202a] ^= 1;
        trainer_card::Cache cache;
        cache.rom = ctx->rom;
        cache.red.valid = true; // Procedural blank portrait; decompression tested separately.
        machine.vram.fill(0);
        for (auto source : {std::pair<size_t, size_t>{59675, 1024}, {1006628, 640}, {0x10ee8, 16}})
            std::fill_n(machine.image.data() + source.first, source.second, 0);
        auto *tiles = machine.wram.data() + 0x3a0;
        auto page = [&](int badges) {
            std::fill_n(tiles, 360, uint8_t(0x7f));
            machine.write(0xd355, badges);
            for (int band = 0; band < 2; ++band) {
                int left = band, right = 19 - band, top = band * 10, bottom = top + 7;
                for (int x = left; x <= right; ++x) {
                    tiles[top * 20 + x] = x == left ? 0x79 : x == right ? 0x7b : 0x7a;
                    tiles[bottom * 20 + x] = x == left ? 0x7d : x == right ? 0x7e : 0x77;
                }
                for (int y = top + 1; y < bottom; ++y) {
                    tiles[y * 20 + left] = 0x7c;
                    tiles[y * 20 + right] = 0x78;
                }
            }
            for (int y = 10; y < 18; ++y)
                tiles[y * 20] = tiles[y * 20 + 19] = 0xd7;
            tiles[9 * 20 + 6] = tiles[9 * 20 + 13] = 0x76;
            for (int x = 15; x <= 18; ++x)
                for (int y = 1; y <= 6; ++y)
                    tiles[y * 20 + x] = uint8_t((x - 15) * 7 + y - 1);
            for (int badge = 0; badge < 8; ++badge) {
                int x = 2 + badge % 4 * 4, y = 11 + badge / 4 * 3;
                tiles[y * 20 + x] = uint8_t(0xd8 + badge);
                if (!(badges & (1 << badge))) {
                    tiles[y * 20 + x + 1] = uint8_t(0x60 + badge * 2);
                    tiles[y * 20 + x + 2] = uint8_t(0x61 + badge * 2);
                }
                for (int part = 0; part < 4; ++part)
                    tiles[(y + 1 + part / 2) * 20 + x + 1 + part % 2] =
                        uint8_t(0x20 + badge * 8 + ((badges & (1 << badge)) ? 4 : 0) + part);
            }
            tiles[6 * 20 + 10] = 0xd6;
            tiles[2 * 20 + 2] = 0x80;
        };
        for (int badges = 0; badges < 256; ++badges) {
            page(badges);
            auto before = machine.wram;
            check(trainer_card::prepare(ctx, cache).ready, "all 256 original badge combinations");
            check(machine.wram == before, "read-only trainer snapshot");
            tiles[12 * 20 + 3] ^= 4;
            check(!trainer_card::prepare(ctx, cache).ready, "stale face/badge is rejected");
        }
        page(0);
        for (int address : {0x1000, 0x1200, 0x1600, 0x1760, 0x1770, 0xd60, 0xd70, 0xd80}) {
            machine.vram[address] ^= 1;
            check(!trainer_card::prepare(ctx, cache).ready, "replaced original graphic rejected");
            machine.vram[address] ^= 1;
        }
        for (int cell : {0, 20 + 19, 7 * 20 + 3, 10 * 20 + 1}) {
            int old = tiles[cell];
            tiles[cell] = 0x7f;
            check(!trainer_card::prepare(ctx, cache).ready, "broken card edge rejected");
            tiles[cell] = uint8_t(old);
        }
        tiles[2 * 20 + 2] = 0xd8;
        check(!trainer_card::prepare(ctx, cache).ready, "badge number outside original cell");
        std::puts("PASS: trainer call, all 256 badge masks, private graphics and atomic fallback");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
