#include "art_manifest.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
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
        if (!pallet::load_catalog(bytes.data(), bytes.size()))
            throw std::runtime_error("invalid ROM catalog");
        pallet::catalog->discover_warps(kanto::Rom(bytes.data(), bytes.size()));
        size_t interiors = 0, cells = 0;
        std::set<int> tilesets;
        for (const auto &entry : pallet::catalog->maps) {
            const auto &scene = entry.second;
            interiors += scene.interior;
            tilesets.insert(scene.tileset);
            for (int z = 0; z < scene.height; ++z)
                for (int x = 0; x < scene.width; ++x) {
                    ++cells;
                    bool supported =
                        scene.interior
                            ? art::find(interior::classify(bytes.data(), scene, x, z).kind) !=
                                  nullptr
                            : art::find(pallet::terrain(bytes.data(), scene, x, z)) != nullptr;
                    if (!supported)
                        throw std::runtime_error("missing art family in map " +
                                                 std::to_string(scene.id));
                }
        }
        if (pallet::catalog->maps.size() != 221 || interiors != 179 || tilesets.size() != 25 ||
            bytes != original)
            throw std::runtime_error("catalog coverage or read-only invariant failed");
        std::printf("PASS: manifest covers %zu ROM cells, 221 reachable maps, 179 interiors and 25 "
                    "tilesets\n",
                    cells);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
