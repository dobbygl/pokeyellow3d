#include "art_manifest.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>

// Independent collision oracle for the bounding rectangle of raised natural
// objects. All vertices are bounded by these rectangles in the pure tests.
bool solid_footprint(const uint8_t *rom, const pallet::Scene &map, int x, int z, int width) {
    if (x < 0 || z < 0 || x + width > map.width || z + width > map.height)
        return false;
    for (int dz = 0; dz < width; ++dz)
        for (int dx = 0; dx < width; ++dx) {
            const int bottom = pallet::map_tile(rom, map, (x + dx) * 2, (z + dz) * 2 + 1);
            if (pallet::tileset(map).walkable[bottom])
                return false;
        }
    return true;
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    std::ifstream stream(argv[1], std::ios::binary);
    if (!stream) {
        std::fprintf(stderr, "SKIP: private ROM unavailable\n");
        return 77;
    }
    try {
        std::vector<uint8_t> rom{std::istreambuf_iterator<char>(stream), {}};
        const auto before = rom;
        if (!pallet::load_catalog(rom.data(), rom.size()))
            throw std::runtime_error("invalid ROM catalog");
        size_t objects = 0, walkable_controls = 0, tall = 0, cuts = 0;
        std::array<bool, 3> roofs{};
        for (const auto &entry : pallet::catalog->maps) {
            const auto &map = entry.second;
            if (map.interior)
                continue;
            for (const auto &house : pallet::houses(map))
                for (size_t i = 0; i < art::Roofs.size(); ++i) {
                    auto r = art::Roofs[i];
                    if (map.id == r.map && house.x == r.x && house.z == r.z) {
                        roofs[i] = art::roof(map, house) == art::geometry::Roof::Flat;
                    }
                }
            for (int z = 0; z < map.height; ++z)
                for (int x = 0; x < map.width; ++x) {
                    const auto kind = pallet::terrain(rom.data(), map, x, z);
                    const auto mesh = art::find(kind)->mesh;
                    if (mesh == art::Mesh::Crown || mesh == art::Mesh::TallCrown ||
                        mesh == art::Mesh::FacetedRock) {
                        const int width = mesh == art::Mesh::TallCrown ? 2 : 1;
                        if (!solid_footprint(rom.data(), map, x, z, width))
                            throw std::runtime_error("raised geometry crosses a walkable cell");
                        ++objects;
                        tall += width == 2;
                        cuts += kind == pallet::Terrain::CutTree;
                    }
                    const int bottom = pallet::map_tile(rom.data(), map, x * 2, z * 2 + 1);
                    if (pallet::tileset(map).walkable[bottom]) {
                        // A misplaced tree on a real path must fail this oracle.
                        if (solid_footprint(rom.data(), map, x, z, 1))
                            throw std::runtime_error("negative path control accepted a tree");
                        ++walkable_controls;
                    }
                }
        }
        if (!objects || !tall || !cuts || !walkable_controls || rom != before ||
            std::find(roofs.begin(), roofs.end(), false) != roofs.end())
            throw std::runtime_error("coverage, landmarks or read-only invariant failed");
        std::printf("PASS: %zu solid object footprints (%zu tall, %zu cut), %zu negative path "
                    "controls, three flat landmarks; ROM unchanged\n",
                    objects, tall, cuts, walkable_controls);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
