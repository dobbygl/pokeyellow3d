#include "full_menu_layout.h"
#include <cstdio>
#include <cstdlib>

namespace {
void require(bool condition, const char *message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
void box(std::array<uint8_t, 360> &tiles, menu_layout::Rect r) {
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) {
            uint8_t edge = menu_layout::border(r, x, y);
            tiles[y * 20 + x] = edge ? edge : 0x7f;
        }
}
} // namespace

int main(int argc, char **argv) {
    using namespace full_menu;
    std::array<uint8_t, 360> tiles{};
    require(classify(nullptr, Context::PcBill).kind == Kind::Unknown, "null source");
    require(classify(tiles.data(), Context::PcBill).kind == Kind::Unknown,
            "empty transfer is not a complete PC screen");
    box(tiles, {0, 0, 16, 10});
    require(classify(tiles.data(), Context::PcCenter).kind == Kind::PcMain,
            "verified terminal geometry");
    require(classify(tiles.data(), Context::None).kind == Kind::Unknown,
            "identical geometry outside an original PC call is not a PC screen");
    require(classify(tiles.data(), Context::PcBill).kind == Kind::Unknown,
            "wrong original PC mode is rejected");
    tiles[0] = 0x7f;
    require(classify(tiles.data(), Context::PcCenter).kind == Kind::Unknown,
            "incomplete visible border keeps the original full LCD");
    tiles = {};
    box(tiles, menu_layout::Bottom);
    box(tiles, BillMain);
    box(tiles, BillBox);
    box(tiles, List);
    box(tiles, Action);
    auto actions = classify(tiles.data(), Context::PcBill);
    require(actions.kind == Kind::PcAction && actions.text.count == 5,
            "list action recognizes surviving borders of every overlapping window");
    tiles[4 * 20 + 5] = 0xed;
    require(classify(tiles.data(), Context::PcBill).kind == Kind::PcAction,
            "cursor changes content without inventing a new layout");
    tiles[19] = 0x80;
    require(classify(tiles.data(), Context::PcBill).kind == Kind::Unknown,
            "text outside all known windows cannot be silently omitted");

    // Private independently recorded original tilemaps: PATH CONTEXT KIND.
    // Context is assigned from the original journey, not guessed by this test.
    require((argc - 1) % 3 == 0, "fixture arguments: TILEMAP CONTEXT EXPECTED_KIND");
    for (int i = 1; i < argc; i += 3) {
        FILE *file = std::fopen(argv[i], "rb");
        require(file, "open private tilemap");
        require(std::fread(tiles.data(), 1, tiles.size(), file) == tiles.size() &&
                    std::fgetc(file) == EOF,
                "private tilemap has exactly 360 cells");
        std::fclose(file);
        auto actual = classify(tiles.data(), Context(std::atoi(argv[i + 1])));
        std::printf("%s: kind=%d regions=%zu\n", argv[i], int(actual.kind), actual.text.count);
        require(int(actual.kind) == std::atoi(argv[i + 2]), "original fixture classification");
    }
    std::puts("PASS: PC context, overlap geometry, unknown/partial transfer and private fixtures");
}
