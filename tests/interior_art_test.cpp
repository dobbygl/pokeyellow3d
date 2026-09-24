#include "interior_art.h"
#include "synthetic_rom.h"
#include <cstdio>

static void check(bool value, const char *why) {
    if (!value)
        throw std::runtime_error(why);
}
int main() {
    try {
        auto rom = synthetic::rom();
        check(pallet::load_catalog(rom.data(), rom.size()), "synthetic catalog");
        pallet::catalog->tilesets.clear();
        for (int id = 0; id < 25; ++id) {
            auto tiles = kanto::read_tileset(kanto::Rom(rom.data(), rom.size()), id);
            tiles.walkable.fill(false);
            tiles.walkable[0x0c] = true;
            tiles.counters = {255, 255, 255};
            pallet::catalog->tilesets.emplace(id, tiles);
        }
        pallet::Scene room;
        room.interior = true;
        room.width = room.height = 8;
        auto cell = [&](int ts, int top, int bottom, int right = -1, int x = 2, int z = 2) {
            room.tileset = ts;
            return interior::classify_art_tiles(room, x, z, top, bottom, right);
        };
        using K = interior::Kind;
        check(cell(15, 0x05, 0x15).kind == K::Furniture && cell(15, 0x05, 0x15).height == .65f,
              "cemetery tombstone has a low solid footprint");
        check(cell(22, 0x05, 0x15).height == .65f && cell(22, 0x07, 0x17).height == .65f,
              "facility foliage and pot stay low enough to read the player");
        check(cell(7, 0x07, 0x17).kind == K::Rock && cell(22, 0x07, 0x17).kind == K::Furniture,
              "same graphic indices in different tilesets are not the same object");
        check(cell(22, 0x4a, 0x4c).kind == K::Machine, "facility panels are machines");
        pallet::catalog->tilesets.at(22).walkable[0x4c] = true;
        check(cell(22, 0x4a, 0x4c).height == 0, "walkable floor wins over a decorative rule");
        pallet::catalog->tilesets.at(22).walkable[0x4c] = false;
        check(cell(22, 0x1d, 0x1d, 0x1e).kind == K::Counter &&
                  cell(22, 0x1d, 0x1d, 0x35).kind == K::Furniture &&
                  !cell(22, 0x1d, 0x1d, 0x77).curated,
              "right tile disambiguates the counter and table; no wildcard for unknowns");
        check(cell(18, 0x4b, 0x4b, 0x4c).kind == K::Counter &&
                  cell(18, 0x4b, 0x4b, 0x4f).kind == K::Wall,
              "slot-machine end and rooftop wall remain distinct");
        check(cell(7, 0x1f, 0x1f).kind == K::Floor && cell(7, 0x1f, 0x1f).height == 0 &&
                  cell(7, 0x1f, 0x1f).curated,
              "invisible maze walls stay invisible, without changing collision");
        check(cell(22, 0x33, 0x33).kind == K::Floor &&
                  cell(22, 0x33, 0x33, -1, 0, 2).kind == K::Wall,
              "void rule fills only unknown cells, never erases the perimeter");
        room.warps.push_back({2, 2, 0, 0});
        check(cell(22, 0x4a, 0x4c).kind == K::Warp && cell(22, 0x4a, 0x4c).height == 0,
              "warp wins over machine rule");
        room.warps.clear();
        check(cell(22, 0x0d, 0x1d).height == .65f && cell(15, 0x0d, 0x1d).height == 1.45f,
              "facility table correction never changes cemetery furniture");
        check(cell(19, 0x30, 0x30).height == .65f && cell(19, 0x36, 0x36).height == .65f,
              "rooftop hut matches its existing low pieces");
        check(!cell(22, 0x5e, 0x5e).curated && cell(22, 0x5e, 0x5e).height == 0,
              "unrecognized graphics stay flat and explicitly unclassified");
        check(!interior::art_rule(-1, 0, 0, 0) && !interior::art_rule(25, 0, 0, 0),
              "invalid tileset cannot index outside rule buckets");
        room.tileset = 22;
        check(!interior::classify_tiles(room, 2, 2, 0x4a, 0x4c).curated &&
                  interior::classify_tiles(room, 2, 2, 0x0d, 0x1d).height == 1.45f,
              "reference API is unchanged even after art classifications");

        // A procedural live block update must replace the art, not consult the
        // static map again (Cut, gym barriers and event doors share this route).
        room.tileset = 7;
        room.block_data.assign(16, 0);
        const auto blockset = pallet::tileset(room).blocks;
        for (int i = 0; i < 16; ++i) {
            rom[blockset + i] = i / 4 % 2 ? 0x50 : 0x40;
            rom[blockset + 16 + i] = 0x0c;
        }
        auto live = room.block_data;
        check(interior::classify_art(rom.data(), room, 2, 2).height == .65f,
              "static gym tree has volume");
        live[5] = 1;
        const auto before = rom;
        check(interior::classify_art(rom.data(), room, 2, 2, &live).height == 0 &&
                  interior::classify_art(rom.data(), room, 2, 2).height == .65f,
              "live block replacement clears only the changed cell footprint");
        check(rom == before && room.block_data[5] == 0 && live[5] == 1,
              "classification never modifies ROM, live blocks or map data");
        std::puts("PASS: A2 disambiguation, perimeter/warp/floor precedence, OFF and live blocks");
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
