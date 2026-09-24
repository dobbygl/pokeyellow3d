#include "art_manifest.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>

static void check(bool value, const std::string &why) {
    if (!value)
        throw std::runtime_error(why);
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream stream(argv[1], std::ios::binary);
    if (!stream) {
        std::fprintf(stderr, "SKIP: private ROM unavailable: %s\n", argv[1]);
        return 77;
    }
    try {
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream), {}};
        const auto original = bytes;
        check(pallet::load_catalog(bytes.data(), bytes.size()), "ROM catalog");
        auto &world = *pallet::catalog;
        world.discover_warps(kanto::Rom(bytes.data(), bytes.size()));
        std::array<size_t, interior::ArtRules.size()> hits{};
        std::array<size_t, size_t(interior::Kind::Count)> kinds{};
        std::set<int> tilesets;
        size_t rooms = 0, classified = 0, preserved_walls = 0, table_backs = 0;
        for (const auto &entry : world.maps) {
            const auto &m = entry.second;
            if (!m.interior)
                continue;
            ++rooms;
            tilesets.insert(m.tileset);
            const auto blocks = m.block_data;
            const auto collision = world.tilesets.at(m.tileset).walkable;
            for (int z = 0; z < m.height; ++z)
                for (int x = 0; x < m.width; ++x) {
                    const std::string where = "map=" + std::to_string(m.id) +
                                              " cell=" + std::to_string(x) + "," +
                                              std::to_string(z);
                    const auto off = interior::classify(bytes.data(), m, x, z);
                    const auto on = interior::classify_art(bytes.data(), m, x, z);
                    check(on.curated, "unclassified " + where);
                    check(on.height >= 0 && on.height <= 2 && art::find(on.kind),
                          "invalid art family/height " + where);
                    ++kinds[size_t(on.kind)];
                    const int top = pallet::map_tile(bytes.data(), m, x * 2, z * 2);
                    const int bottom = pallet::map_tile(bytes.data(), m, x * 2, z * 2 + 1);
                    const int right = pallet::map_tile(bytes.data(), m, x * 2 + 1, z * 2);
                    for (int dz = 0; dz < 2; ++dz)
                        for (int dx = 0; dx < 2; ++dx)
                            check(pallet::map_tile(bytes.data(), m, x * 2 + dx, z * 2 + dz) < 96,
                                  "unknown graphics " + where);
                    if (!off.curated) {
                        ++classified;
                        check(off.height == 0 && off.kind == interior::Kind::Floor,
                              "unexpected reference unknown " + where);
                        const auto *rule = interior::art_rule(m.tileset, top, bottom, right);
                        check(rule != nullptr, "missing documented rule " + where);
                        ++hits[size_t(rule - interior::ArtRules.data())];
                    } else if (m.tileset == 22 && top == 0x0d && bottom == 0x1d &&
                               off.kind == interior::Kind::Furniture) {
                        ++table_backs;
                        check(on.kind == off.kind && on.height == .65f && off.height == 1.45f,
                              "table halves disagree " + where);
                    } else {
                        check(on.kind == off.kind && on.height == off.height,
                              "changed established reference cell " + where);
                        if (off.kind == interior::Kind::Wall &&
                            interior::art_rule(m.tileset, top, bottom, right))
                            ++preserved_walls;
                    }
                    if (off.kind == interior::Kind::Warp || collision[size_t(bottom)])
                        check(on.height == off.height, "raised transit/warp " + where);
                    const auto live = interior::classify_art(bytes.data(), m, x, z, &blocks);
                    check(live.kind == on.kind && live.height == on.height && live.curated,
                          "identical live blocks classify differently " + where);
                }
            check(blocks == m.block_data && collision == world.tilesets.at(m.tileset).walkable,
                  "classification modified layout/collision");
        }
        check(rooms == 179 && tilesets.size() == 21 && classified == 2960,
              "incomplete interior/tileset/unclassified-cell coverage");
        check(preserved_walls == 869, "perimeter coverage changed");
        check(table_backs > 0, "facility table correction was not exercised");
        for (size_t i = 0; i < hits.size(); ++i)
            check(hits[i] > 0, "unused rule " + std::to_string(i));
        for (auto kind : {interior::Kind::Machine, interior::Kind::Window, interior::Kind::Rock})
            check(kinds[size_t(kind)] > 0, "new family not exercised");
        check(bytes == original, "classification changed ROM");
        std::printf("PASS: 179 interiors, 21 tilesets, 2960 classified cells, 143 used rules, "
                    "869 preserved walls, %zu corrected table backs, ROM/layout/collision intact\n",
                    table_backs);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
