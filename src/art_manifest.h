#pragma once
#include "interior_scene.h"
#include <array>

// Presentation policy only. No ROM bytes, collision overrides or external assets.
// A1 deliberately selects the existing meshes. Later phases change the rows and
// their procedural builders, while the disabled path keeps its original geometry.
namespace art {
enum class Material { Earth, Foliage, Stone, Water, Wood, Wall };
enum class Occlusion { None, Solid };
template <typename Family> struct Row {
    Family family;
    const char *name;
    Family reference_mesh;
    Material material;
    Occlusion occlusion;
};
using Terrain = pallet::Terrain;
using Interior = interior::Kind;
inline constexpr std::array<Row<Terrain>, 14> Exterior{{
    {Terrain::Ground, "ground", Terrain::Ground, Material::Earth, Occlusion::None},
    {Terrain::Tree, "tree", Terrain::Tree, Material::Foliage, Occlusion::Solid},
    {Terrain::TallTree, "tall-tree", Terrain::TallTree, Material::Foliage, Occlusion::Solid},
    {Terrain::Canopy, "canopy", Terrain::Canopy, Material::Foliage, Occlusion::Solid},
    {Terrain::CutTree, "cut-tree", Terrain::CutTree, Material::Foliage, Occlusion::Solid},
    {Terrain::Rock, "rock", Terrain::Rock, Material::Stone, Occlusion::Solid},
    {Terrain::Grass, "grass", Terrain::Grass, Material::Foliage, Occlusion::None},
    {Terrain::Ledge, "ledge", Terrain::Ledge, Material::Earth, Occlusion::Solid},
    {Terrain::Water, "water", Terrain::Water, Material::Water, Occlusion::None},
    {Terrain::Fence, "fence", Terrain::Fence, Material::Wood, Occlusion::Solid},
    {Terrain::Sign, "sign", Terrain::Sign, Material::Wood, Occlusion::Solid},
    {Terrain::Pillar, "pillar", Terrain::Pillar, Material::Stone, Occlusion::Solid},
    {Terrain::Wall, "wall", Terrain::Wall, Material::Wall, Occlusion::Solid},
    {Terrain::Portal, "portal", Terrain::Portal, Material::Stone, Occlusion::Solid},
}};
inline constexpr std::array<Row<Interior>, 6> Interiors{{
    {Interior::Floor, "floor", Interior::Floor, Material::Earth, Occlusion::None},
    {Interior::Warp, "warp", Interior::Warp, Material::Earth, Occlusion::None},
    {Interior::Wall, "wall", Interior::Wall, Material::Wall, Occlusion::Solid},
    {Interior::Furniture, "furniture", Interior::Furniture, Material::Wood, Occlusion::Solid},
    {Interior::Counter, "counter", Interior::Counter, Material::Wood, Occlusion::Solid},
    {Interior::Water, "water", Interior::Water, Material::Water, Occlusion::None},
}};
template <typename Family, size_t N>
constexpr const Row<Family> *find(const std::array<Row<Family>, N> &rows, Family family) {
    for (const auto &row : rows)
        if (row.family == family)
            return &row;
    return nullptr;
}
inline constexpr const Row<Terrain> *find(Terrain family) {
    return find(Exterior, family);
}
inline constexpr const Row<Interior> *find(Interior family) {
    return find(Interiors, family);
}
template <typename Family, size_t N>
constexpr bool complete(const std::array<Row<Family>, N> &rows, Family limit) {
    if (N != size_t(limit))
        return false;
    for (int id = 0; id < int(limit); ++id) {
        int count = 0;
        for (const auto &row : rows)
            if (row.family == Family(id)) {
                ++count;
                if (!row.name || !*row.name || int(row.reference_mesh) < 0 ||
                    int(row.reference_mesh) >= int(limit))
                    return false;
            }
        if (count != 1)
            return false;
    }
    return true;
}
static_assert(complete(Exterior, Terrain::Count), "Every terrain family needs one art row");
static_assert(complete(Interiors, Interior::Count), "Every interior family needs one art row");
} // namespace art
