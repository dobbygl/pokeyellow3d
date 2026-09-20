#include "interior_scene.h"
#include "pallet_state.h"
#include "assets_manifest_pokeyellow.h"
#include <fstream>
#include <iterator>
#include <cstring>
static void check(bool value, const char *why) {
    if (!value)
        throw std::runtime_error(why);
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    try {
        std::ifstream f(argv[1], std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(f), {}};
        check(pallet::load_catalog(bytes.data(), bytes.size()), "ROM catalog");
        auto &world = *pallet::catalog;
        kanto::Rom rom(bytes.data(), bytes.size());
        check(world.maps.size() == 38, "interiors load on demand");
        const auto &house = world.ensure_map(rom, 37);
        const auto &upstairs = world.ensure_map(rom, 38);
        check(house.interior && house.component == 37 && house.origin_x == 0 && house.origin_z == 0,
              "interior component and origin");
        check(interior::classify(bytes.data(), house, 7, 1).kind == interior::Kind::Warp,
              "stairs stay flat");
        check(interior::classify(bytes.data(), house, 3, 1).height > .5f, "television has volume");
        check(interior::classify(bytes.data(), house, 3, 4).kind == interior::Kind::Furniture,
              "table has volume");
        check(interior::classify(bytes.data(), upstairs, 0, 0).height > 0, "PC has volume");
        check(interior::classify(bytes.data(), upstairs, 0, 7).height == .48f,
              "bed is low furniture, not a perimeter wall");
        check(interior::classify(bytes.data(), house, 4, 2).height == 0,
              "walkable floor stays flat");
        auto depths = world.discover_warps(rom);
        check(interior::classify(bytes.data(), *world.find(42), 4, 3).height == 1.45f,
              "mart shelves are tall");
        check(interior::classify(bytes.data(), *world.find(41), 0, 4).height == .65f,
              "healing equipment is low even at the perimeter");
        check(interior::classify(bytes.data(), *world.find(41), 0, 7).height == .65f,
              "plant pot is not a perimeter wall");
        check(interior::classify(bytes.data(), *world.find(42), 1, 4).height == 0 &&
                  !interior::classify(bytes.data(), *world.find(42), 1, 4).curated,
              "unclassified art stays flat and reported");
        check(world.maps.size() == 221 && world.tilesets.size() == 25,
              "complete reachable catalog");
        check(world.find(236) && !world.find(237), "scripted elevator placeholder is not a room");
        std::vector<uint8_t> sparse(kanto::RomSize, 255);
        for (auto e : POKEYELLOW_ASSETS_MANIFEST)
            std::memcpy(sparse.data() + e.rom_offset, bytes.data() + e.rom_offset, e.size);
        kanto::Rom resident(sparse.data(), sparse.size());
        kanto::World loaded(resident);
        loaded.discover_warps(resident);
        for (const auto &entry : world.maps) {
            const auto &m = entry.second;
            const auto &t = world.tilesets.at(m.tileset);
            check(loaded.find(m.id) && loaded.find(m.id)->block_data == m.block_data,
                  "interior layout available in manifest");
            check(!std::memcmp(bytes.data() + t.graphics, sparse.data() + t.graphics, 96 * 16),
                  "interior graphics available in manifest");
        }
        GBContext ctx{};
        std::vector<uint8_t> ram(32768), vram(16384), io(128);
        ctx.wram = ram.data();
        ctx.vram = vram.data();
        ctx.io = io.data();
        ctx.rom = bytes.data();
        ctx.rom_size = bytes.size();
        auto set = [&](int a, int v) { ram[a - 0xc000] = uint8_t(v); };
        set(pallet::Map, 37);
        set(pallet::Tileset, 1);
        set(pallet::Width, 4);
        set(pallet::Height, 4);
        set(pallet::Sprite1, 1);
        set(pallet::UpdateSprites, 1);
        set(pallet::X, 2);
        set(pallet::Y, 7);
        io[0x40] = 0x91;
        io[0x47] = 0xe4;
        for (int z = 0; z < 4; z++)
            for (int x = 0; x < 4; x++)
                set(0xc6e8 + (z + 3) * 10 + x + 3, house.block_data[z * 4 + x]);
        auto before = ram;
        check(pallet::view(&ctx) == pallet::View::Overworld, "live interior scene recognized");
        check(before == ram, "selection never writes RAM");
        set(pallet::Tileset, 0);
        check(pallet::view(&ctx) == pallet::View::Unsupported, "old tileset rejected at warp");
        std::puts("PASS: lazy interiors, 221 reachable maps, 25 tilesets, furniture, warps, sparse "
                  "manifest and live selection");
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
