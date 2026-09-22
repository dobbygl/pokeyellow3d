#pragma once
#include "world_scene.h"

namespace interior {
enum class Kind { Floor, Warp, Wall, Furniture, Counter, Water, Count };
struct Cell {
    Kind kind = Kind::Floor;
    float height = 0;
    bool curated = false;
};
inline bool cave(const pallet::Scene &scene) {
    return scene.tileset == 17 || scene.tileset == 11;
}
inline Cell classify(const uint8_t *rom, const pallet::Scene &scene, int x, int z,
                     const std::vector<uint8_t> *live = nullptr) {
    for (auto w : scene.warps)
        if (w.x == x && w.z == z)
            return {Kind::Warp, 0, true};
    int top = pallet::map_tile(rom, scene, x * 2, z * 2, live);
    int bottom = pallet::map_tile(rom, scene, x * 2, z * 2 + 1, live);
    const auto &tiles = pallet::tileset(scene);
    if (scene.tileset == 1 || scene.tileset == 4) {
        if (scene.tileset == 4 && (bottom == 0x26 || bottom == 0x27))
            return {Kind::Furniture, .65f, true};
        if (top == 0x40 || top == 0x42)
            return {Kind::Furniture, 1.15f, true}; // PC and desk.
        if (top == 0x06)
            return {Kind::Furniture, .85f, true}; // Television.
        if (top == 0x0e)
            return {Kind::Furniture, .24f, true}; // Console.
        if (top == 0x2d || bottom == 0x3f)
            return {Kind::Furniture, .48f, true}; // Bed.
        if (top == 0x44 || top == 0x46)
            return {Kind::Furniture, 1.05f, true}; // Plant.
        if (bottom == 0x30 || bottom == 0x32)
            return {Kind::Counter, 1.45f, true};
        if (bottom == 0x36 || bottom == 0x38 || bottom == 0x3c || bottom == 0x3a)
            return {Kind::Furniture, .65f, true};
    }
    if (cave(scene) && bottom == 0x14)
        return {Kind::Water, 0, true};
    for (int counter : tiles.counters)
        if (counter != 255 && bottom == counter)
            return {Kind::Counter, 1.05f, true};
    if (tiles.walkable[bottom])
        return {Kind::Floor, 0, true};
    // Artistic rules are local to a tileset. The same tile number can be a
    // bookshelf in one family and a floor or void in another. Evaluate these
    // before the perimeter heuristic: a plant against a wall is still a plant.
    switch (scene.tileset) {
    case 2:
    case 6: // Shared shop / Pokemon Center graphics.
        if (scene.tileset == 6 && top == 0x42 && bottom == 0x52)
            return {Kind::Furniture, 1.05f, true}; // PC.
        if (top == 0x20 || top == 0x21)
            return {Kind::Furniture, 1.05f, true};
        if (top == 0x22 || top == 0x23)
            return {Kind::Furniture, .65f, true}; // Plant pots.
        if (top == 0x40 || top == 0x41 || top == 0x44 || top == 0x45)
            return {Kind::Furniture, 1.45f, true}; // Stock shelves.
        if (top == 0x04 && bottom == 0x14)
            return {Kind::Furniture, .55f, true}; // Healing table.
        if (scene.tileset == 6 && (top == 0x24 || top == 0x26))
            return {Kind::Furniture, .65f, true};
        if (top == 0x4c || top == 0x4d || top == 0x2e)
            return {Kind::Counter, 1.05f, true};
        break;
    case 5:
    case 7: // Lab / dojo / gym.
        if (top == 0x0d || bottom == 0x0d || bottom == 0x4e)
            return {Kind::Furniture, 1.45f, true}; // Bookcases and cabinets.
        if (top == 0x02 || top == 0x22)
            return {Kind::Furniture, 1.2f, true}; // Statues.
        if (top == 0x24 || top == 0x25 || top == 0x26 || top == 0x0f)
            return {Kind::Wall, 2.f, true};
        if (top == 0x3b)
            return {Kind::Furniture, .65f, true}; // Lab tables.
        if (top == 0x5d)
            return {Kind::Furniture, .25f, true}; // Gym floor door.
        break;
    case 8: // Houses.
        if (top == 0x08 || top == 0x09 || top == 0x2a || top == 0x2b)
            return {Kind::Furniture, 1.05f, true};
        if (top == 0x0e || top == 0x0f)
            return {Kind::Furniture, 1.45f, true};
        if (top == 0x26 || top == 0x27 || top == 0x2f || top == 0x36)
            return {Kind::Furniture, .65f, true};
        break;
    case 9:
    case 10:
    case 12: // Gates and museum share graphics.
        if (top == 0x04 || top == 0x05 || top == 0x0e || top == 0x0f)
            return {Kind::Furniture, 1.05f, true};
        if (top == 0x27 || top == 0x28)
            return {Kind::Furniture, .65f, true}; // Exhibits / desks.
        if (top == 0x4c)
            return {Kind::Furniture, .85f, true}; // Fossil display.
        if (top == 0x22)
            return {Kind::Wall, 2.f, true};
        break;
    case 11:
    case 17: // Underground passage and cavern: solid rock, not furniture.
        return {Kind::Wall, 2.f, true};
    case 13: // Ship cabins, dining room and corridors.
        if (top == 0x01 && bottom == 0x01)
            return {Kind::Floor, 0, true}; // Unused black void.
        if (top == 0x07 || top == 0x08)
            return {Kind::Furniture, 1.05f, true};
        if (top == 0x20)
            return {Kind::Furniture, .6f, true};
        if (top == 0x05 || top == 0x02 || top == 0x10 || top == 0x15 || top == 0x33)
            return {Kind::Wall, 2.f, true};
        break;
    case 15:
    case 22: // Tower / facility share much of their sheet.
        if (top == 0x11 && bottom == 0x11)
            return {Kind::Floor, 0, true};
        if (top == 0x09)
            return {Kind::Furniture, 1.25f, true}; // Tombstone / machine.
        if (top == 0x0d)
            return {Kind::Furniture, 1.45f, true};
        if (top == 0x2a || top == 0x2b || top == 0x2d)
            return {Kind::Wall, 2.f, true};
        if (top == 0x53 && bottom == 0x36)
            return {Kind::Furniture, .35f, true}; // Broken rock.
        break;
    case 16: // Generic interior, including the fan club.
        if (top == 0x0d || top == 0x20)
            return {Kind::Furniture, 1.05f, true};
        if (top == 0x18 || top == 0x1a || top == 0x26 || top == 0x38)
            return {Kind::Furniture, .65f, true};
        if (top == 0x0b)
            return {Kind::Furniture, 1.15f, true};
        break;
    case 18: // Lobby, department store and elevators.
        if (top == 0x06 || top == 0x22)
            return {Kind::Furniture, 1.05f, true};
        if (top == 0x2a || top == 0x2c || top == 0x3c || top == 0x40 || top == 0x41)
            return {Kind::Furniture, 1.45f, true};
        if (top == 0x26)
            return {Kind::Furniture, .65f, true};
        if (top == 0x01 || top == 0x02 || top == 0x2e)
            return {Kind::Wall, 2.f, true};
        break;
    case 19: // Mansion, game corner and wardens' house.
        if (top == 0x08 || top == 0x09 || top == 0x48)
            return {Kind::Furniture, 1.05f, true};
        if (top == 0x0f || top == 0x4c)
            return {Kind::Furniture, 1.45f, true};
        if (top == 0x22 || top == 0x26 || top == 0x27 || top == 0x37 || top == 0x38)
            return {Kind::Furniture, .65f, true};
        if (top == 0x4a)
            return {Kind::Wall, 2.f, true};
        break;
    case 20: // Research lab.
        if (top == 0x32 || top == 0x24 || top == 0x41)
            return {Kind::Furniture, .7f, true};
        if (top == 0x19 || top == 0x57 || top == 0x58)
            return {Kind::Furniture, 1.3f, true};
        break;
    case 21: // Club tables, chairs and machines.
        if (top == 0x0c || top == 0x0e || top == 0x0f)
            return {Kind::Furniture, .65f, true};
        if (top == 0x1d)
            return {Kind::Furniture, 1.3f, true};
        break;
    case 24: // Beach house.
        if (top == 0x08 || top == 0x09)
            return {Kind::Furniture, 1.05f, true};
        if (top == 0x26 || top == 0x27 || top == 0x2a || top == 0x2c)
            return {Kind::Furniture, .65f, true};
        break;
    }
    // Two graphic rows (one movement cell) form the northern wall band.
    if (z == 0 || x == 0 || x == scene.width - 1 || z == scene.height - 1)
        return {Kind::Wall, 2.f, true};
    // Unrecognized art retains its original flat texture. Collision remains
    // wholly owned by the engine; an unknown solid is not necessarily a desk.
    return {Kind::Floor, 0, false};
}
} // namespace interior
