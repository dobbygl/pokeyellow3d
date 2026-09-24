#include "ambient_occlusion.h"
#include "exterior_geometry.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

void check(bool value, const char *message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
using art::ambient::Box;
using art::ambient::Field;
using art::ambient::Point;

// Independent oracle: the four probe points written out explicitly, counting
// every occupied probe that no self volume covers at lattice resolution.
float expected(const Field &field, Point p, Point normal, const std::vector<Box> &self) {
    const float ax = std::abs(normal.x), ay = std::abs(normal.y), az = std::abs(normal.z);
    Point n{}, u{}, v{};
    if (ay >= ax && ay >= az) {
        n.y = normal.y > 0 ? 1.f : -1.f;
        u.x = v.z = 1;
    } else if (ax >= az) {
        n.x = normal.x > 0 ? 1.f : -1.f;
        u.y = v.z = 1;
    } else {
        n.z = normal.z > 0 ? 1.f : -1.f;
        u.x = v.y = 1;
    }
    int covered = 0;
    for (float a : {-1.f, 1.f})
        for (float b : {-1.f, 1.f}) {
            const float s = Field::Spread, o = Field::Offset;
            const Point q{p.x + n.x * o + a * s * u.x + b * s * v.x,
                          p.y + n.y * o + a * s * u.y + b * s * v.y,
                          p.z + n.z * o + a * s * u.z + b * s * v.z};
            if (!field.contains(q))
                continue;
            bool mine = false;
            for (const auto &box : self) {
                Field alone = field;
                alone.reset(-8, -8, 16, 16);
                alone.add(box);
                mine |= alone.contains(q);
            }
            covered += !mine;
        }
    return 1 - covered * Field::Strength;
}

int main() {
    Field field;
    field.reset(-3, -3, 6, 6);
    check(field.visibility({0, 0, 0}, {0, 1, 0}) == 1, "open ground is unoccluded");
    check(field.add({{0, 0, -2}, {1, 2, 2}}), "wall accepted");
    const float wall = field.visibility({0, 0, 0}, {0, 1, 0});
    check(wall < 1 && wall > .7f, "wall darkens adjacent ground");
    check(field.visibility({0, 1, 0}, {-1, 0, 0}) == 1, "wall cannot occlude its own face");
    check(field.visibility({-.8f, 0, 0}, {0, 1, 0}) == 1, "ground outside radius stays clear");
    check(field.visibility({0, 3, 0}, {0, 1, 0}) == 1, "wall does not extend infinitely up");
    check(field.add({{-2, 0, 0}, {0, 2, 1}}), "perpendicular wall accepted");
    check(field.visibility({0, 0, 0}, {0, 1, 0}) < wall, "inside corner darker than single wall");
    check(!field.contains({3, 1, 0}) && !field.contains({-4, 1, 0}), "finite field bounds");
    check(!field.add({{0, 0, 0}, {1, 0, 1}}), "zero height rejected");
    check(!field.add({{0, 0, 0}, {1, std::numeric_limits<float>::infinity(), 1}}),
          "nonfinite volume rejected");
    check(field.add({{1e30f, 0, 1e30f}, {2e30f, 1, 2e30f}}),
          "finite distant volume clips safely without integer overflow");

    // A raised canopy is not an infinitely tall collision column.
    field.reset(-3, -3, 6, 6);
    check(field.add({{-.8f, 1, -.8f}, {.8f, 2, .8f}}), "raised canopy");
    check(field.visibility({0, 0, 0}, {0, 1, 0}) == 1, "air below canopy remains air");
    check(field.visibility({0, .9f, 0}, {0, 1, 0}) < 1, "nearby overhead volume occludes");

    // Faces off the lattice never probe inside their own solid: the top of a
    // block whose height rounds down, and a side face between samples.
    field.reset(-3, -3, 6, 6);
    const Box block{{-1, 0, -1}, {1, 1.6f, 1}};
    field.add(block);
    for (float x : {-.9f, -.37f, .3f, .61f, .99f})
        check(field.visibility({x, 1.6f, x * .5f}, {0, 1, 0}, &block, 1) == 1,
              "exposed top of a block is never self-occluded");
    field.reset(-3, -3, 6, 6);
    const Box side{{.3f, 0, -2}, {1, 2, 2}};
    field.add(side);
    for (float x : {.3f, .31f, .45f})
        check(field.visibility({x, 1, 0}, {-1, 0, 0}, &side, 1) == 1,
              "off-lattice face is not darkened by its own volume");

    // Thin posts, trunks and boards occupy the lattice cells they touch.
    for (const Box &thin :
         {Box{{.4f, 0, .4f}, {.6f, .65f, .6f}}, Box{{.1f, .45f, .4f}, {.9f, .98f, .56f}},
          Box{{-.1f, 0, .65f}, {.08f, 1.7f, 1.f}}}) {
        field.reset(-3, -3, 6, 6);
        field.add(thin);
        check(field.contains({(thin.low.x + thin.high.x) / 2, (thin.low.y + thin.high.y) / 2,
                              (thin.low.z + thin.high.z) / 2}),
              "thin volume is registered");
    }

    // Real C1 crowns: an isolated tree is only darkened by other solids, so
    // none of its faces lose light; a neighbouring tree darkens the facing side.
    auto tree = [](float x, float z) {
        return std::vector<Box>{{{x + .4f, 0, z + .4f}, {x + .6f, .65f, z + .6f}},
                                {{x + .12f, .5f, z + .12f}, {x + .88f, 1.6f, z + .88f}}};
    };
    auto normal = [](const art::geometry::Face &f) {
        const float ux = f[3].x - f[0].x, uy = f[3].y - f[0].y, uz = f[3].z - f[0].z;
        const float wx = f[1].x - f[0].x, wy = f[1].y - f[0].y, wz = f[1].z - f[0].z;
        return Point{uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx};
    };
    int isolated_dark = 0, paired_dark = 0, samples = 0;
    for (int map = 0; map < 3; ++map)
        for (int cx = -2; cx <= 1; ++cx) {
            const auto own = tree(float(cx), 0);
            Field alone, pair;
            alone.reset(cx - 2, -2, 5, 4);
            pair.reset(cx - 2, -2, 5, 4);
            for (const auto &box : own) {
                alone.add(box);
                pair.add(box);
            }
            for (const auto &box : tree(float(cx + 1), 0))
                pair.add(box);
            art::geometry::crown(
                float(cx), 0, 1, 1, art::geometry::scale(map, cx, 0), false,
                [&](const art::geometry::Face &face, float, int) {
                    const Point n = normal(face);
                    for (int i = 0; i < 4; ++i) {
                        const Point p{face[i].x, face[i].y, face[i].z};
                        const float a = alone.visibility(p, n, own.data(), own.size());
                        const float b = pair.visibility(p, n, own.data(), own.size());
                        check(a == expected(alone, p, n, own), "crown matches float oracle");
                        check(b == expected(pair, p, n, own), "paired crown matches oracle");
                        isolated_dark += a < 1;
                        paired_dark += b < 1;
                        ++samples;
                    }
                });
        }
    check(samples > 0 && isolated_dark == 0, "an isolated crown is never self-occluded");
    check(paired_dark > 0, "adjacent crowns occlude each other");

    // Splitting a volume at a map border leaves the same samples in world
    // coordinates, including negative origins and points off the lattice.
    Field whole, split;
    whole.reset(-5, -5, 10, 10);
    split.reset(-5, -5, 10, 10);
    check(whole.add({{-2, 0, -2}, {2, 2, 2}}), "whole volume");
    check(split.add({{-2, 0, -2}, {0, 2, 2}}) && split.add({{0, 0, -2}, {2, 2, 2}}),
          "split volumes");
    for (int x = -30; x <= 30; ++x)
        for (int z = -30; z <= 30; ++z)
            for (float y : {0.f, .37f, 1.1f}) {
                const Point p{x * .1f + .013f, y, z * .1f - .007f};
                for (const Point n : {Point{0, 1, 0}, Point{1, 0, 0}, Point{0, 0, -1}}) {
                    check(whole.visibility(p, n) == split.visibility(p, n),
                          "no seam from map partition");
                    check(split.visibility(p, n) == expected(split, p, n, {}),
                          "visibility matches independent float probes");
                }
            }
    split.reset(-5, -5, 10, 10);
    check(split.visibility({0, 0, 0}, {0, 1, 0}) == 1, "rebuild removes cleared occluder");

    // Contacts are fans interpolated linearly between centre and rim.
    check(art::ambient::contact_alpha(0) == .22f && art::ambient::contact_alpha(1) == 0,
          "contact has a dark centre and transparent edge");
    check(std::abs(art::ambient::contact_alpha(.5f) - .11f) < 1e-6f,
          "contact profile is the linear one the GPU draws");
    std::puts("PASS: finite occluders, corners, no self-shadow on real crowns and off-lattice "
              "faces, thin volumes, map seams and soft contact");
}
