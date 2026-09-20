#include "interior_scene.h"
#include <fstream>
#include <iterator>
#include <set>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: interior_audit ROM [MAP_ID]\n");
        return 2;
    }
    try {
        std::ifstream input(argv[1], std::ios::binary);
        std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(input), {});
        if (!pallet::load_catalog(bytes.data(), bytes.size()))
            return 1;
        auto &world = *pallet::catalog;
        kanto::Rom rom(bytes.data(), bytes.size());
        auto depth = world.discover_warps(rom, 32);
        int rooms = 0;
        if (argc > 2) {
            const auto &m = world.maps.at(std::stoi(argv[2]));
            std::printf("%d %s tileset=%d size=%dx%d\n", m.id, m.title.c_str(), m.tileset, m.width,
                        m.height);
            for (size_t i = 0; i < m.warps.size(); i++) {
                auto w = m.warps[i];
                std::printf("warp %zu xy=%d,%d -> %d entry=%d\n", i, w.x, w.z, w.destination,
                            w.entrance);
            }
            for (auto o : m.actors)
                std::printf("actor sprite=%d xy=%d,%d text=%d\n", o.sprite, o.x, o.z, o.text);
            for (int z = 0; z < m.height; z++) {
                std::printf("%02d ", z);
                for (int x = 0; x < m.width; x++) {
                    int bottom = pallet::map_tile(bytes.data(), m, x * 2, z * 2 + 1);
                    char c = world.tilesets.at(m.tileset).walkable[bottom] ? '.' : '#';
                    for (auto w : m.warps)
                        if (w.x == x && w.z == z)
                            c = 'W';
                    for (auto a : m.actors)
                        if (a.x == x && a.z == z)
                            c = 'N';
                    std::putchar(c);
                }
                std::puts("");
            }
            return 0;
        }
        std::puts("map,title,interior,depth,tileset,width,height,warps,floor,warp_cells,wall,"
                  "furniture,counter,water,unclassified_flat_cells,unknown_graphics");
        for (const auto &entry : world.maps) {
            const auto &m = entry.second;
            std::array<int, 6> count{};
            int inferred = 0, unknown = 0;
            if (m.interior) {
                ++rooms;
                for (int z = 0; z < m.height; z++)
                    for (int x = 0; x < m.width; x++) {
                        auto cell = interior::classify(bytes.data(), m, x, z);
                        ++count[int(cell.kind)];
                        inferred += !cell.curated;
                        if (cell.height < 0 || cell.height > 3)
                            throw std::runtime_error("Invalid height");
                        for (int dy = 0; dy < 2; dy++)
                            for (int dx = 0; dx < 2; dx++)
                                unknown +=
                                    pallet::map_tile(bytes.data(), m, x * 2 + dx, z * 2 + dy) >= 96;
                    }
                if (unknown)
                    throw std::runtime_error("Unknown graphics in map " + std::to_string(m.id));
                for (auto w : m.warps)
                    if (interior::classify(bytes.data(), m, w.x, w.z).height != 0)
                        throw std::runtime_error("Raised warp in map " + std::to_string(m.id));
            }
            std::printf("%d,%s,%d,%d,%d,%d,%d,%zu,%d,%d,%d,%d,%d,%d,%d,%d\n", m.id, m.title.c_str(),
                        m.interior, depth.at(m.id), m.tileset, m.width, m.height, m.warps.size(),
                        count[0], count[1], count[2], count[3], count[4], count[5], inferred,
                        unknown);
        }
        std::fprintf(stderr,
                     "PASS: %zu reachable maps, %d interiors, %zu tilesets, bounded traversal, all "
                     "graphics decoded and all warps flat\n",
                     world.maps.size(), rooms, world.tilesets.size());
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
