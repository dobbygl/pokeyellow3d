#include "options_menu_state.h"
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
        check(!options_menu::active(nullptr), "null context");
        machine.io[0x40] = 0xe3;
        machine.image[0x5df7] = 0xcd;
        machine.image[0x5df8] = 0x84;
        machine.image[0x5df9] = 0x3e;
        ctx->sp = 0xdffd;
        machine.write(0xdffd, 0xfa);
        machine.write(0xdffe, 0x5d);
        check(options_menu::active(ctx), "original wrapper CALL and live return");
        ctx->sp = 0xdfff;
        check(!options_menu::active(ctx), "popped return is inactive");
        ctx->sp = 0xdffd;
        machine.image[0x5df8] ^= 1;
        check(!options_menu::active(ctx), "changed ROM target");
        machine.image[0x5df8] ^= 1;
        auto *tiles = machine.wram.data() + 0x3a0;
        for (int y = 0; y < 18; ++y)
            for (int x = 0; x < 20; ++x) {
                int edge = menu_layout::border({0, 0, 20, 18}, x, y);
                tiles[y * 20 + x] = uint8_t(edge ? edge : 0x7f);
            }
        check(full_menu::classify(tiles, full_menu::Context::Options).kind ==
                  full_menu::Kind::Options,
              "complete original options border");
        machine.write(0xcc26, 255); // A stale Start-menu selection is irrelevant.
        for (int row : {0, 1, 2, 3, 4, 7}) {
            machine.write(options_menu::Cursor, row);
            tiles[(2 + row * 2) * 20 + 1] = 0xed;
            auto selected = options_menu::selection(ctx);
            check(selected.valid && selected.current == row && selected.painted == row,
                  "dedicated cursor and all six original rows");
            tiles[(2 + row * 2) * 20 + 1] = 0x7f;
        }
        machine.write(options_menu::Cursor, 1);
        tiles[2 * 20 + 1] = 0xed;
        auto pending = options_menu::selection(ctx);
        check(pending.valid && pending.current == 1 && pending.painted == 0,
              "original arrow retained while logical selection changes");
        for (int row : {5, 6, 8, 255}) {
            machine.write(options_menu::Cursor, row);
            check(!options_menu::selection(ctx).valid, "dummy or out-of-range option");
        }
        machine.write(options_menu::Cursor, 0);
        tiles[4 * 20 + 1] = 0xed;
        check(!options_menu::selection(ctx).valid, "duplicate cursor rejected");
        tiles[4 * 20 + 1] = 0x7f;
        tiles[3 * 20 + 3] = 0x78;
        check(!options_menu::selection(ctx).valid, "foreign graphics rejected");
        tiles[0] = 0x7f;
        check(full_menu::classify(tiles, full_menu::Context::Options).kind ==
                  full_menu::Kind::Unknown,
              "missing border rejected");
        std::puts("PASS: original options context, dedicated cursor, legal rows and fallback");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
