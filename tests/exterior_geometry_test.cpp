#include "exterior_geometry.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <tuple>
#include <vector>
#undef assert
#define assert(condition)                                                                          \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition);             \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)

using art::geometry::Face;
using art::geometry::Point;
using Key = std::tuple<int, int, int>;
Key key(Point p) {
    return {int(std::lround(p.x * 100000)), int(std::lround(p.y * 100000)),
            int(std::lround(p.z * 100000))};
}
double volume(const std::vector<Face> &faces) {
    double sum = 0;
    for (auto f : faces)
        for (int i = 1; i < 3; ++i) {
            auto a = f[0], b = f[i], c = f[i + 1];
            sum += double(a.x) * (double(b.y) * c.z - double(b.z) * c.y) +
                   double(a.y) * (double(b.z) * c.x - double(b.x) * c.z) +
                   double(a.z) * (double(b.x) * c.y - double(b.y) * c.x);
        }
    return std::abs(sum / 6);
}
bool closed(const std::vector<Face> &faces) {
    std::map<std::pair<Key, Key>, std::pair<int, int>> edges;
    for (auto face : faces)
        for (int i = 0; i < 4; ++i) {
            auto a = key(face[i]), b = key(face[(i + 1) % 4]);
            if (a == b)
                continue;
            const bool forward = a < b;
            if (!forward)
                std::swap(a, b);
            auto &edge = edges[{a, b}];
            ++edge.first;
            edge.second += forward ? 1 : -1;
        }
    for (auto entry : edges)
        if (entry.second.first != 2 || entry.second.second != 0)
            return false;
    return !edges.empty();
}
int main() {
    for (int sides : {6, 8}) {
        std::vector<Face> faces;
        art::geometry::frustum({0, 0, 0}, .4f, .3f, .2f, 1.2f, .45f, sides, .17f,
                               [&](Face face, float) { faces.push_back(face); });
        assert(closed(faces));
        // Independently known regular-polygon frustum volume.
        const double area = .5 * sides * std::sin(6.283185307179586 / sides) * .4 * .3;
        const double expected = area / 3 * (1 + .45 + .45 * .45);
        assert(std::abs(volume(faces) - expected) < .000001);
        faces.pop_back();
        assert(!closed(faces)); // A missing cap must be detected, not ignored.
    }
    for (int x = -80; x <= 80; ++x) {
        const float variation = art::geometry::scale(51, x, -17);
        assert(variation >= .9f && variation <= 1.1f);
        assert(variation == art::geometry::scale(51, x, -17));
        for (bool tall : {false, true}) {
            const float width = tall ? 2.f : 1.f;
            size_t vertices = 0;
            art::geometry::crown(
                float(x), -17, width, width, variation, tall, [&](Face face, float, int) {
                    vertices += key(face[2]) == key(face[3]) ? 3 : 6;
                    for (auto p : face) {
                        assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
                        assert(p.x > x && p.x < x + width);
                        assert(p.z > -17 && p.z < -17 + width);
                    }
                });
            // Includes retained trunk/blob and the footprint's ground. The
            // envelope must fit the phase's 2x budget even in a dense forest.
            const size_t ground = tall ? 96 : 24;
            assert(vertices + 90 + ground <= 2 * (180 + ground));
        }
    }
    assert(art::geometry::scale(0, 0, 0) != art::geometry::scale(0, 1, 0));
    for (int sides : {0, 2, 9}) {
        int emitted = 0;
        art::geometry::frustum({0, 0, 0}, 1, 1, 0, 1, .5f, sides, 0,
                               [&](Face, float) { ++emitted; });
        assert(emitted == 0);
    }
    for (auto style :
         {art::geometry::Roof::Gable, art::geometry::Roof::Hip, art::geometry::Roof::Flat}) {
        std::vector<Face> faces;
        art::geometry::roof(0, 0, 6, 4, 2, 1, style,
                            [&](Face face, float) { faces.push_back(face); });
        for (auto face : faces)
            for (auto p : face)
                assert(p.x >= 0 && p.x <= 6 && p.z >= 0 && p.z <= 4 && p.y >= 2 && p.y <= 3);
        assert(faces.size() == (style == art::geometry::Roof::Flat ? 1u : 4u));
    }
    std::puts("PASS: closed facet winding, analytic volume, deterministic variation, cell bounds "
              "and canopy budget");
}
