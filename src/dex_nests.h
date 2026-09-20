#pragma once
#include "kanto_rom.h"
#include "mon_pic.h"
#include <map>
#include <queue>
#include <set>
#include <vector>

// The AREA command searches the land/water encounter tables, not fishing,
// trades, gifts or static encounters. Cerulean Cave is hidden by the original
// DisplayWildLocations routine even when its encounter tables contain a match.
namespace dex_nests {
struct Location {
    int map = 0;
    bool land = false, water = false;
    uint8_t coordinate = 0;
};
struct Result {
    bool valid = false;
    std::vector<Location> locations;
    std::set<uint8_t> coordinates;
};
inline uint8_t coordinate(const kanto::Rom &rom, int map) {
    if (map < 0 || map >= kanto::MapCount)
        throw std::runtime_error("Invalid nest map");
    if (map < 37)
        return rom.byte(0x7139c + map * 3);
    for (int i = 0; i < 128; i++) {
        size_t at = 0x7140b + i * 4;
        if (map < rom.byte(at))
            return rom.byte(at + 1);
    }
    throw std::runtime_error("Unterminated internal town-map entries");
}
inline Result read(const uint8_t *bytes, size_t size, int species) {
    Result result;
    if (!mon_pic::number(bytes, size, species))
        return result;
    try {
        kanto::Rom rom(bytes, size);
        bool ended = false;
        for (int map = 0; map <= kanto::MapCount; map++) {
            int pointer = rom.word(0xcb95 + map * 2);
            if ((pointer >> 8) == 255) {
                ended = true;
                break;
            }
            if (map == kanto::MapCount || pointer < 0x4000 || pointer >= 0x8000)
                return {};
            size_t at = rom.address(3, pointer);
            bool found[2]{};
            for (int type = 0; type < 2; type++) {
                if (!rom.byte(at++))
                    continue;
                if (at + 20 > 0x10000)
                    return {};
                for (int slot = 0; slot < 10; slot++)
                    found[type] |= rom.byte(at + slot * 2 + 1) == species;
                at += 20;
            }
            if (!found[0] && !found[1])
                continue;
            auto coord = coordinate(rom, map);
            if (!map || coord == 0x19)
                continue;
            result.locations.push_back({map, found[0], found[1], coord});
            result.coordinates.insert(coord);
        }
        result.valid = ended;
    } catch (const std::exception &) {
        return {};
    }
    return result;
}
struct Entrance {
    int map = -1, steps = 0;
    float x = 0, z = 0;
};
inline std::map<int, Entrance> entrances(const kanto::World &world) {
    std::map<int, Entrance> result;
    std::queue<int> queue;
    for (const auto &entry : world.maps)
        if (entry.second.component == 0) {
            const auto &map = entry.second;
            result[map.id] = {map.id, 0, map.origin_x + map.width * .5f,
                              map.origin_z + map.height * .5f};
            for (const auto &warp : map.warps) {
                const auto *destination = world.find(warp.destination);
                if (!destination || destination->component == 0 || result.count(destination->id))
                    continue;
                result[destination->id] = {map.id, 1, map.origin_x + warp.x + .5f,
                                           map.origin_z + warp.z + .5f};
                queue.push(destination->id);
            }
        }
    while (!queue.empty()) {
        int id = queue.front();
        queue.pop();
        auto origin = result.at(id);
        const auto &map = world.maps.at(id);
        for (const auto &warp : map.warps) {
            const auto *destination = world.find(warp.destination);
            if (kanto::dynamic_warp(map, warp) || !destination || destination->component == 0 ||
                result.count(destination->id))
                continue;
            auto next = origin;
            ++next.steps;
            result[destination->id] = next;
            queue.push(destination->id);
        }
    }
    return result;
}
} // namespace dex_nests
