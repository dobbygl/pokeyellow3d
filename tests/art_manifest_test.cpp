#include "art_manifest.h"
#include <cstdio>
#include <cstdlib>
static void check(bool value, const char *message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
int main() {
    check(art::find(pallet::Terrain::Water)->material == art::Material::Water,
          "water has its own material policy");
    check(art::find(interior::Kind::Warp)->occlusion == art::Occlusion::None,
          "warps remain flat and unoccluded");
    check(!art::find(pallet::Terrain(-1)) && !art::find(pallet::Terrain::Count) &&
              !art::find(interior::Kind(255)),
          "unknown families have no partial substitute");
    auto duplicate = art::Exterior;
    duplicate[0] = duplicate[1];
    check(!art::complete(duplicate, pallet::Terrain::Count), "duplicate cannot hide missing row");
    auto invalid = art::Interiors;
    invalid[2].reference_mesh = interior::Kind(255);
    check(!art::complete(invalid, interior::Kind::Count), "invalid mesh rejects manifest");
    invalid = art::Interiors;
    invalid[2].name = nullptr;
    check(!art::complete(invalid, interior::Kind::Count), "unnamed row rejects manifest");
    auto bad_recipe = art::Exterior;
    bad_recipe[1].mesh = art::Mesh(255);
    check(!art::complete(bad_recipe, pallet::Terrain::Count),
          "unknown procedure rejects the complete pass");
    pallet::Scene city;
    for (auto entry : art::Roofs) {
        city.id = entry.map;
        pallet::House landmark{float(entry.x), float(entry.z), 6, 4, true};
        check(art::roof(city, landmark) == art::geometry::Roof::Flat,
              "named landmark keeps its flat roof");
        landmark.x += 1;
        check(art::roof(city, landmark) == art::geometry::Roof::Hip,
              "roof override does not leak to another footprint");
    }
    std::puts("PASS: complete manifest, explicit water/warp policy and malformed rows rejected");
}
