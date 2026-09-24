#pragma once
#include "kanto_rom.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>

namespace pallet {
using Scene = kanto::Map;
struct House {
    float x, z, w, d;
    bool lab;
    float eave = 1.5f, ridge = 1.f;
    int facade_row = 0;
};
inline std::map<int, std::vector<House>> house_cache;
inline std::unique_ptr<kanto::World> catalog;
inline const uint8_t *catalog_rom = nullptr;
inline bool load_catalog(const uint8_t *rom, size_t size) {
    if (catalog && catalog_rom == rom)
        return true;
    try {
        auto next = std::make_unique<kanto::World>(kanto::Rom(rom, size));
        catalog = std::move(next);
        catalog_rom = rom;
        house_cache.clear();
        return true;
    } catch (const std::exception &e) {
        static bool warned = false;
        if (!warned)
            std::fprintf(stderr, "[3D] Map catalog unavailable: %s\n", e.what());
        warned = true;
        return false;
    }
}
inline const Scene *scene(int id) {
    return catalog ? catalog->find(id) : nullptr;
}
inline const Scene *ensure_scene(int id) {
    if (const auto *known = scene(id))
        return known;
    if (!catalog || id < 0 || id >= kanto::MapCount)
        return nullptr;
    try {
        return &catalog->ensure_map(kanto::Rom(catalog_rom, kanto::RomSize), id);
    } catch (const std::exception &e) {
        static std::array<bool, kanto::MapCount> warned{};
        if (!warned[id])
            std::fprintf(stderr, "[3D] Map %d unavailable: %s\n", id, e.what());
        warned[id] = true;
        return nullptr;
    }
}
inline const kanto::Tileset &tileset(const Scene &s) {
    return catalog->tilesets.at(s.tileset);
}
inline uint8_t map_tile(const uint8_t *rom, const Scene &s, int tx, int tz,
                        const std::vector<uint8_t> *live = nullptr) {
    return catalog->tile(kanto::Rom(rom, kanto::RomSize), s, tx, tz, live);
}
inline uint8_t tile_pixel(const uint8_t *rom, const Scene &s, int tile, int x, int y) {
    size_t at = tileset(s).graphics + tile * 16 + (y % 8) * 2;
    return ((rom[at] >> (7 - x % 8)) & 1) | (((rom[at + 1] >> (7 - x % 8)) & 1) << 1);
}
inline uint8_t map_pixel(const uint8_t *rom, const Scene &s, int x, int y) {
    return tile_pixel(rom, s, map_tile(rom, s, x / 8, y / 8), x, y);
}
enum class Terrain {
    Ground,
    Tree,
    TallTree,
    Canopy,
    CutTree,
    Rock,
    Grass,
    Ledge,
    Water,
    Fence,
    Sign,
    Pillar,
    Wall,
    Portal,
    Count
};
// Classification of one movement cell from its two left-column tiles.
inline Terrain terrain_of(const Scene &s, int top, int bottom) {
    if (bottom == tileset(s).grass)
        return Terrain::Grass;
    if (s.tileset == 0) {
        if (top == 0x40 && bottom == 0x50)
            return Terrain::Tree;
        if (top == 0x2d && bottom == 0x3d)
            return Terrain::CutTree;
        if (top == 0x2a && bottom == 0x3a)
            return Terrain::Rock;
        for (auto l : catalog->ledges)
            if (l.to == bottom)
                return Terrain::Ledge;
        if (bottom == 0x14)
            return Terrain::Water;
        if (top == 0x0e && bottom == 0x55)
            return Terrain::Fence;
        if (top == 0x46 && bottom == 0x56)
            return Terrain::Sign;
    }
    if (s.tileset == 3) {
        if (top == 2 && bottom == 0x12)
            return Terrain::Tree;
        if (top == 4 && bottom == 0x23)
            return Terrain::TallTree;
        if ((top == 6 && bottom == 0x16) || (top == 0x24 && bottom == 0) ||
            (top == 0x26 && bottom == 0x36))
            return Terrain::Canopy;
        if (top == 0x21 && bottom == 0x31)
            return Terrain::Sign;
    }
    if (s.tileset == 23) {
        if (top == 0x0b && bottom == 0x1b)
            return Terrain::Portal;
        if ((top == 0x10 || top == 0x25) && bottom == 0x28)
            return Terrain::Pillar;
        if ((top == 0x05 || top == 0x15) && (bottom == 0x30 || bottom == 0x05 || bottom == 0x15))
            return Terrain::Pillar;
        if ((top == 0x0f && bottom == 0x0e) || (top == 0x0d && bottom == 0x0f) ||
            (top == 0x0f && bottom == 0x0f) || (top == 0x03 && bottom == 0x0d))
            return Terrain::Wall;
    }
    if (bottom == 0x14)
        return Terrain::Water;
    return Terrain::Ground;
}
inline Terrain terrain(const uint8_t *rom, const Scene &s, int x, int z,
                       const std::vector<uint8_t> *live = nullptr) {
    int top = map_tile(rom, s, x * 2, z * 2, live),
        bottom = map_tile(rom, s, x * 2, z * 2 + 1, live);
    return terrain_of(s, top, bottom);
}
// Roof borders divide attached city buildings more reliably than a flood fill
// of collision solids. Width/depth and facade rows are decoded from ROM tiles.
inline const std::vector<House> &houses(const Scene &s) {
    auto found = house_cache.find(s.id);
    if (found != house_cache.end())
        return found->second;
    std::vector<House> result;
    if (s.interior)
        return house_cache.emplace(s.id, std::move(result)).first->second;
    if (s.tileset == 0) {
        int width = s.width * 2, height = s.height * 2;
        auto tile = [&](int x, int z) { return map_tile(catalog_rom, s, x, z); };
        for (int z = 0; z < height; z++)
            for (int x = 0; x < width; x++) {
                int first = tile(x, z);
                if (first != 5 && first != 0x4c)
                    continue;
                int right = x + 1, end = first == 5 ? 9 : 0x4d;
                while (right < width) {
                    int t = tile(right, z);
                    if (t != 0x53 && !(first == 5 && (t == 6 || t == 7 || t == 8)))
                        break;
                    ++right;
                }
                if (right >= width || tile(right, z) != end || right - x < 3)
                    continue;
                for (int bottom = z + 2; bottom < height; bottom++) {
                    if (tile(x, bottom) != 0x4e || tile(right, bottom) != 0x4f)
                        continue;
                    float depth = (bottom + 1 - z) * .5f;
                    int facade = z + 2;
                    while (facade < bottom && tile(x, facade) != 0x0f && tile(x, facade) != 0x25)
                        ++facade;
                    House h{x * .5f,
                            z * .5f,
                            (right + 1 - x) * .5f,
                            depth,
                            first == 0x4c,
                            std::clamp(1.5f + (depth - 3) * .38f, 1.3f, 5.5f),
                            first == 0x4c ? .25f : 1.f,
                            facade};
                    // Architectural accents: geometry bounds still come from the ROM.
                    if (s.id == 0 && x == 20)
                        h.lab = true;
                    if (s.id == 2 && x == 24 && z == 28) {
                        h.eave = 2.5f;
                        h.lab = true;
                    }
                    if (s.id == 6 && x == 12 && z == 12) {
                        h.eave = 4.5f;
                        h.lab = true;
                    }
                    if (s.id == 10 && x == 32 && z == 20) {
                        h.eave = 6.f;
                        h.lab = true;
                    }
                    result.push_back(h);
                    break;
                }
            }
        // Lavender's tower extends beyond the northern edge; its roof is absent
        // from the ROM map, so a bounded artistic correction is necessary.
        if (s.id == 4)
            result.push_back({12, 0, 6, 6, true, 5.f, .4f, 0});
    }
    if (s.id == 9)
        result.push_back({5, 0, 10, 6, true, 3.f, .4f, 8});
    return house_cache.emplace(s.id, std::move(result)).first->second;
}
inline bool in_house(const Scene &s, int x, int z) {
    for (const auto &h : houses(s))
        if (x >= h.x && x < h.x + h.w && z >= h.z && z < h.z + h.d)
            return true;
    return false;
}
inline bool clears(Terrain t) {
    return t == Terrain::Tree || t == Terrain::TallTree || t == Terrain::Canopy ||
           t == Terrain::Pillar || t == Terrain::Wall || t == Terrain::Portal ||
           t == Terrain::CutTree || t == Terrain::Rock || t == Terrain::Fence || t == Terrain::Sign;
}
inline bool cleared(const uint8_t *rom, const Scene &s, int x, int z,
                    const std::vector<uint8_t> *live = nullptr) {
    return in_house(s, x, z) || clears(terrain(rom, s, x, z, live));
}
inline std::array<float, 2> camera_target(float x, float z, float) {
    // A common world focus follows the player continuously across map seams.
    return {x, z};
}
inline std::vector<int> resident_maps(const Scene &s) {
    std::vector<int> ids{s.id};
    for (auto c : s.connections)
        if (scene(c.map))
            ids.push_back(c.map);
    return ids; // At most current + four neighbors. No unbounded historical cache.
}
} // namespace pallet
