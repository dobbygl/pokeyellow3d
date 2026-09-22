#include "battle_bag_state.h"
#include "synthetic_context.h"
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <cstdio>

namespace {
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
std::vector<uint8_t> read(const char *path) {
    std::ifstream f(path, std::ios::binary);
    check(bool(f), "private input exists");
    return {std::istreambuf_iterator<char>(f), {}};
}
void box(uint8_t *tiles, menu_layout::Rect r) {
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) {
            int edge = menu_layout::border(r, x, y);
            tiles[y * 20 + x] = uint8_t(edge ? edge : 0x7f);
        }
}
} // namespace
int main(int argc, char **argv) {
    try {
        synthetic::Context machine;
        auto *ctx = &machine.ctx;
        check(!battle_bag::back_picture(nullptr, 0, 84).valid, "missing back source rejected");
        check(!battle_bag::back_picture(ctx->rom, ctx->rom_size, 0).valid,
              "invalid species rejected");
        if (argc > 1) {
            machine.image = read(argv[1]);
            ctx->rom = machine.image.data();
            ctx->rom_size = machine.image.size();
            check(ctx->rom_size == 1048576, "private canonical ROM");
            if (argc > 2 && std::string(argv[2]) == "export-backs") {
                check(argc == 4, "back export arguments ROM export-backs OUTPUT");
                FILE *out = std::fopen(argv[3], "wb");
                check(out, "private back export");
                for (int n = 1; n <= 151; ++n) {
                    int species = mon_pic::species(ctx->rom, ctx->rom_size, n);
                    auto picture = battle_bag::back_picture(ctx->rom, ctx->rom_size, species);
                    check(picture.valid, "all 151 original back pictures decode");
                    std::fwrite(picture.tiles.data(), 1, picture.tiles.size(), out);
                }
                std::fclose(out);
                std::puts("PASS: 151 original cropped and doubled back pictures");
                return 0;
            }
            check(argc == 3, "private test arguments ROM STATE");
            auto blob = read(argv[2]);
            check(blob.size() > 80, "private state header");
            auto word = [&](int at) {
                return unsigned(blob[at]) | unsigned(blob[at + 1]) << 8 |
                       unsigned(blob[at + 2]) << 16 | unsigned(blob[at + 3]) << 24;
            };
            size_t total = 0;
            for (int at = 24; at <= 48; at += 4)
                total += word(at);
            check(total <= blob.size() && word(24) >= 8192 && word(28) >= 8192,
                  "private state sections");
            size_t start = blob.size() - total;
            std::copy_n(blob.data() + start, 8192, machine.wram.begin());
            std::copy_n(blob.data() + start + word(24), 8192, machine.vram.begin());
            size_t io = start;
            for (int at = 24; at < 40; at += 4)
                io += word(at);
            std::copy_n(blob.data() + io, 128, machine.io.begin());
            ctx->sp = uint16_t(blob[64] | blob[65] << 8);
            battle_bag::Cache cache;
            auto snapshot = battle_bag::prepare(ctx, cache);
            std::printf(
                "private front=%d back=%d front-match=%d back-match=%d ready=%d\n",
                cache.front.valid, cache.back.valid,
                std::memcmp(cache.front.tiles.data(), ctx->vram + 0x1000, mon_pic::TileBytes) == 0,
                std::memcmp(cache.back.tiles.data(), ctx->vram + 0x1310, mon_pic::TileBytes) == 0,
                snapshot.ready);
            check(snapshot.ready, "original bag HUD and both complete ROM portraits verified");
            machine.vram[0x1310] ^= 1;
            check(!battle_bag::prepare(ctx, cache).ready,
                  "replaced back picture rejects full menu");
            std::puts("PASS: original battle bag private state and ROM graphics");
            return 0;
        }
        machine.io[0x40] = 0xe3;
        machine.io[0x47] = 0xe4;
        machine.write(battle::IsInBattle, 1);
        machine.write(battle::BattleType, 0);
        machine.write(battle::Link, 0);
        ctx->sp = 0xdffd;
        machine.write(0xdffd, 0x51);
        machine.write(0xdffe, 0x51);
        machine.image[0x3d14e] = 0xcd;
        machine.image[0x3d14f] = 0xe0;
        machine.image[0x3d150] = 0x2a;
        check(item_menu::context(ctx) == full_menu::Context::BattleBag,
              "original live battle bag call");
        auto *tiles = machine.wram.data() + 0x3a0;
        std::fill_n(tiles, 360, 0x7f);
        box(tiles, menu_layout::Bottom);
        box(tiles, full_menu::List);
        check(full_menu::classify(tiles, full_menu::Context::BattleBag).kind ==
                  full_menu::Kind::BattleBag,
              "original battle windows allow separately verified outer graphics");
        tiles[2 * 20 + 4] = 0x7f;
        check(full_menu::classify(tiles, full_menu::Context::BattleBag).kind ==
                  full_menu::Kind::Unknown,
              "missing original list border rejected");
        ctx->sp = 0xdfff;
        check(item_menu::context(ctx) == full_menu::Context::None, "popped battle call rejected");
        std::puts("PASS: battle bag call, geometry and invalid sources");
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
    return 0;
}
