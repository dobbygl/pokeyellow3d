#include "item_menu_state.h"
#include "synthetic_context.h"
#include <cstdio>
#include <stdexcept>

namespace {
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
void box(uint8_t *tiles, menu_layout::Rect r) {
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) {
            auto edge = menu_layout::border(r, x, y);
            tiles[y * 20 + x] = edge ? edge : 0x7f;
        }
}
} // namespace
int main(int argc, char **argv) {
    try {
        synthetic::Context machine;
        auto *ctx = &machine.ctx;
        machine.io[0x40] = 0xe3;
        using full_menu::Context;
        using full_menu::Kind;
        check(item_menu::context(nullptr) == Context::None, "null item context");
        struct Call {
            size_t offset;
            int target;
            Context domain;
        };
        for (auto c : {Call{0x292e, 0x69a5, Context::Mart}, Call{0x11ed2, 0x2ae0, Context::Bag},
                       Call{0x11f21, 0x3aab, Context::Bag}, Call{0x11fc5, 0x2c51, Context::Bag},
                       Call{0x11f7a, 0x2fe4, Context::Bag}, Call{0x11fce, 0x2fec, Context::Bag}}) {
            auto &rom = machine.image;
            rom[c.offset] = 0xcd;
            rom[c.offset + 1] = uint8_t(c.target);
            rom[c.offset + 2] = uint8_t(c.target >> 8);
            int returned = int(c.offset < 0x4000 ? c.offset : 0x4000 + c.offset % 0x4000) + 3;
            ctx->sp = 0xdffd;
            machine.write(0xdffd, returned & 255);
            machine.write(0xdffe, returned >> 8);
            check(item_menu::context(ctx) == c.domain, "live original banked or fixed CALL");
            ctx->sp = 0xdfff;
            check(item_menu::context(ctx) == Context::None, "stale popped return rejected");
            ctx->sp = 0xdffd;
            rom[c.offset + 1] ^= 1;
            check(item_menu::context(ctx) == Context::None, "changed ROM target rejected");
            rom[c.offset + 1] ^= 1;
            machine.io[0x47] = 0;
            check(item_menu::context(ctx) == Context::None, "palette transition rejected");
            machine.io[0x47] = 0xe4;
        }
        auto *tiles = machine.wram.data() + 0x3a0;
        std::fill_n(tiles, 360, 0);
        box(tiles, {10, 0, 10, 16});
        box(tiles, full_menu::List);
        check(full_menu::classify(tiles, Context::Bag).kind == Kind::BagList,
              "bag preserves surviving Start window behind original list");
        check(full_menu::classify(tiles, Context::PcItems).kind == Kind::Unknown,
              "bag geometry cannot be mistaken for player PC");
        box(tiles, {13, 10, 7, 5});
        box(tiles, full_menu::Quantity);
        box(tiles, menu_layout::Bottom);
        box(tiles, menu_layout::YesNo);
        auto layout = full_menu::classify(tiles, Context::Bag);
        check(layout.kind == Kind::BagAction && layout.text.count == 6,
              "Toss quantity and confirmation preserve all six overlapping windows");
        check(item_menu::text_only(tiles, layout.text), "item profile accepts original text");
        tiles[4 * 20 + 6] = 0x6e;
        check(!item_menu::text_only(tiles, layout.text), "PC level graphic rejected in bag");
        tiles[4 * 20 + 6] = 0x7f;
        tiles[2 * 20 + 4] = 0x7f;
        check(full_menu::classify(tiles, Context::Bag).kind == Kind::Unknown,
              "incomplete visible list border rejected");
        check((argc - 1) % 3 == 0, "private fixture arguments: TILEMAP CONTEXT KIND");
        for (int i = 1; i < argc; i += 3) {
            FILE *f = std::fopen(argv[i], "rb");
            check(f, "private tilemap opened");
            check(std::fread(tiles, 1, 360, f) == 360 && std::fgetc(f) == EOF,
                  "private tilemap exact size");
            std::fclose(f);
            auto actual = full_menu::classify(tiles, Context(std::atoi(argv[i + 1])));
            std::printf("%s kind=%d regions=%zu\n", argv[i], int(actual.kind), actual.text.count);
            check(int(actual.kind) == std::atoi(argv[i + 2]), "private original layout");
        }
        std::puts(
            "PASS: original item calls, stale/altered returns, overlap and graphic isolation");
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    return 0;
}
