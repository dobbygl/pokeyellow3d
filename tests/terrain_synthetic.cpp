#include "interior_scene.h"
#include "synthetic_rom.h"
#include <cstdio>

// Runs the renderer's own classifiers over the procedural world: pallet::terrain
// and pallet::cleared outdoors, interior::classify indoors. No rule is restated
// here, only the outcome each probe cell must produce.
static void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    try {
        const auto &plan = synthetic::layout();
        auto image = synthetic::rom();
        check(pallet::load_catalog(image.data(), image.size()), "synthetic catalog loads");
        const auto *home = pallet::scene(plan.home);
        const auto *east = pallet::scene(plan.east);
        check(home && east, "home map and eastern neighbour are resident");

        using T = pallet::Terrain;
        auto terrain = [&](synthetic::Cell c) {
            return pallet::terrain(image.data(), *home, c.x, c.z);
        };
        check(terrain(plan.ground) == T::Ground, "plain ground");
        check(terrain(plan.grass) == T::Grass, "tall grass from the tileset header");
        check(terrain(plan.tree) == T::Tree, "cuttable-looking tree pair");
        check(terrain(plan.rock) == T::Rock, "boulder pair");
        check(terrain(plan.ledge) == T::Ledge, "ledge from the ledge table");
        check(terrain(plan.water) == T::Water, "water tile");
        check(terrain(plan.fence) == T::Fence, "fence pair");
        check(terrain(plan.signpost) == T::Sign, "sign pair");
        check(terrain(plan.cut_tree) == T::CutTree, "cut tree pair");
        // Every block is uniform, so the second cell of a block agrees.
        check(terrain({plan.grass.x + 1, plan.grass.z + 1}) == T::Grass,
              "grass covers the whole block");
        check(pallet::terrain(image.data(), *east, plan.east_grass.x, plan.east_grass.z) ==
                  T::Grass,
              "the eastern neighbour carries its own grass patch");

        auto cleared = [&](synthetic::Cell c) {
            return pallet::cleared(image.data(), *home, c.x, c.z);
        };
        check(cleared(plan.tree), "a tree clears the ground quad");
        check(cleared(plan.rock), "a boulder clears the ground quad");
        check(cleared(plan.fence), "a fence clears the ground quad");
        check(cleared(plan.signpost), "a sign clears the ground quad");
        check(!cleared(plan.ground) && !cleared(plan.grass),
              "walkable ground keeps its floor quad");
        check(!cleared(plan.water) && !cleared(plan.ledge),
              "water and ledges keep their floor quad");
        check(pallet::houses(*home).empty(), "the synthetic town has no roof runs");

        // Interiors go through interior::classify on the on-demand scene.
        const auto *house = pallet::ensure_scene(plan.interior);
        check(house && house->interior && house->tileset == plan.interior_tileset,
              "interior scene available");
        using K = interior::Kind;
        auto cell = [&](synthetic::Cell c) {
            return interior::classify(image.data(), *house, c.x, c.z);
        };
        auto floor = cell(plan.interior_floor);
        check(floor.kind == K::Floor && floor.height == 0 && floor.curated,
              "walkable floor stays flat and recognised");
        auto wall = cell(plan.interior_wall);
        check(wall.kind == K::Wall && wall.height == plan.interior_wall_height && wall.curated,
              "perimeter band is a wall");
        auto furniture = cell(plan.interior_furniture);
        check(furniture.kind == K::Furniture &&
                  furniture.height == plan.interior_furniture_height && furniture.curated,
              "television has volume");
        auto warp = cell(plan.interior_warp);
        check(warp.kind == K::Warp && warp.height == 0 && warp.curated, "the exit stays flat");
        check(cell({plan.interior_floor.x + 1, plan.interior_floor.z + 1}).kind == K::Floor,
              "floor covers the whole block");
        check(cell({0, plan.interior_furniture.z}).kind == K::Wall,
              "the western band is a wall too");

        std::fprintf(stderr, "PASS: nine outdoor terrains, cleared quads and four interior cells "
                             "on the synthetic world\n");
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
