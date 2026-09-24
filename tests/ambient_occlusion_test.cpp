#include "ambient_occlusion.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

void check(bool value, const char *message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
int main() {
    using art::ambient::Field;
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

    // A raised canopy is not an infinitely tall collision column. Its lower
    // contact band shades surfaces close to it, not a floor a tile below.
    field.reset(-3, -3, 6, 6);
    check(field.add({{-.8f, 1, -.8f}, {.8f, 2, .8f}}), "raised canopy");
    check(field.visibility({0, 0, 0}, {0, 1, 0}) == 1, "air below canopy remains air");
    check(field.visibility({0, .9f, 0}, {0, 1, 0}) < 1, "nearby overhead volume occludes");

    // Splitting a volume at a map border leaves the same samples in world
    // coordinates, including negative origins and boundary points.
    Field whole, split;
    whole.reset(-5, -5, 10, 10);
    split.reset(-5, -5, 10, 10);
    check(whole.add({{-2, 0, -2}, {2, 2, 2}}), "whole volume");
    check(split.add({{-2, 0, -2}, {0, 2, 2}}) && split.add({{0, 0, -2}, {2, 2, 2}}),
          "split volumes");
    for (int x = -12; x <= 12; ++x)
        for (int z = -12; z <= 12; ++z)
            check(whole.visibility({x * .25f, 0, z * .25f}, {0, 1, 0}) ==
                      split.visibility({x * .25f, 0, z * .25f}, {0, 1, 0}),
                  "no seam from map partition");
    // Independent oracle for the cached integer probes: explicitly sample
    // all four float positions on the canonical lattice in all six directions.
    for (int axis = 0; axis < 3; ++axis)
        for (float sign : {-1.f, 1.f})
            for (int x = -8; x <= 8; ++x)
                for (int y = 0; y <= 12; ++y)
                    for (int z = -8; z <= 8; ++z) {
                        art::ambient::Point normal{}, u{}, v{};
                        if (axis == 0) {
                            normal.x = sign;
                            u.y = .35f;
                            v.z = .35f;
                        }
                        if (axis == 1) {
                            normal.y = sign;
                            u.x = .35f;
                            v.z = .35f;
                        }
                        if (axis == 2) {
                            normal.z = sign;
                            u.x = .35f;
                            v.y = .35f;
                        }
                        const art::ambient::Point p{x * .5f, y * .25f, z * .5f};
                        int covered = 0;
                        for (float a : {-1.f, 1.f})
                            for (float b : {-1.f, 1.f})
                                covered +=
                                    split.contains({p.x + normal.x * .12f + a * u.x + b * v.x,
                                                    p.y + normal.y * .12f + a * u.y + b * v.y,
                                                    p.z + normal.z * .12f + a * u.z + b * v.z});
                        check(split.visibility(p, normal) == 1 - covered * .08f,
                              "cached coverage matches independent float probes");
                    }
    split.reset(-5, -5, 10, 10);
    check(split.visibility({0, 0, 0}, {0, 1, 0}) == 1, "rebuild removes cleared occluder");
    check(art::ambient::contact_alpha(0) == .22f && art::ambient::contact_alpha(1) == 0,
          "contact has a dark centre and transparent edge");
    check(art::ambient::contact_alpha(.25f) > art::ambient::contact_alpha(.5f) &&
              art::ambient::contact_alpha(.5f) > art::ambient::contact_alpha(.75f),
          "contact fades monotonically");
    std::puts("PASS: finite occluders, corners, no self-shadow, map seams and soft contact");
}
