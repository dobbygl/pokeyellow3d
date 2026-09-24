#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Pure construction recipes. Coordinates describe presentation geometry only;
// these builders neither read nor write the cartridge or the guest context.
namespace art::geometry {
struct Point {
    float x, y, z;
};
using Face = std::array<Point, 4>;
enum class Roof { Gable, Hip, Flat };

inline float scale(int map, int x, int z) {
    // Unsigned arithmetic is defined for negative world coordinates as well.
    uint32_t value =
        uint32_t(map) * 0x9e3779b9u ^ uint32_t(x) * 0x85ebca6bu ^ uint32_t(z) * 0xc2b2ae35u;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    return .9f + float(value % 2001u) * .0001f;
}

// Closed faceted frustum, emitted with the renderer's clockwise face winding.
// Six sides suffice for layered foliage; rocks use eight. Caps are triangle
// fans represented as quads with the last point repeated, like existing meshes.
template <typename Emit>
void frustum(Point center, float rx, float rz, float bottom, float top, float ratio, int sides,
             float rotation, Emit emit) {
    if (sides < 3 || sides > 8 || rx <= 0 || rz <= 0 || top <= bottom || ratio <= 0)
        return;
    // A few (rotation, sides) pairs repeat for every tree and rock: keep their
    // unit directions, computed by the same expression as before.
    struct Directions {
        float rotation;
        int sides;
        std::array<float, 8> cos, sin;
    };
    static std::array<Directions, 8> cache{};
    static int cached = 0;
    const Directions *unit = nullptr;
    for (int i = 0; i < cached && !unit; ++i)
        if (cache[i].rotation == rotation && cache[i].sides == sides)
            unit = &cache[i];
    Directions local{rotation, sides, {}, {}};
    for (int i = 0; !unit && i < sides; ++i) {
        const float angle = rotation + float(i) * 6.28318530718f / float(sides);
        local.cos[i] = std::cos(angle);
        local.sin[i] = std::sin(angle);
    }
    if (!unit) {
        if (cached < int(cache.size()))
            cache[cached++] = local;
        unit = &local;
    }
    std::array<Point, 8> low{}, high{};
    for (int i = 0; i < sides; ++i) {
        const float dx = unit->cos[i] * rx, dz = unit->sin[i] * rz;
        low[i] = {center.x + dx, bottom, center.z + dz};
        high[i] = {center.x + dx * ratio, top, center.z + dz * ratio};
    }
    for (int i = 0; i < sides; ++i) {
        const int next = (i + 1) % sides;
        emit(Face{high[next], high[i], low[i], low[next]}, .88f);
        emit(Face{Point{center.x, top, center.z}, high[i], high[next], high[next]}, 1.08f);
        emit(Face{Point{center.x, bottom, center.z}, low[next], low[i], low[i]}, .72f);
    }
}

template <typename Emit>
void crown(float x, float z, float width, float depth, float variation, bool tall, Emit emit) {
    const int layers = tall ? 4 : 3;
    const float height = (tall ? 2.7f : 1.65f) * variation;
    const float base = tall ? .7f : .5f;
    for (int layer = 0; layer < layers; ++layer) {
        const float fraction = float(layer) / float(layers);
        const float radius = .4f * (1.f - .55f * fraction) * variation;
        const float bottom = base + fraction * (height - base);
        const float top = base + float(layer + 1) / float(layers) * (height - base) + .10f;
        frustum(Point{x + width * .5f, 0, z + depth * .5f}, width * radius, depth * radius, bottom,
                top, .42f, 6, float(layer % 2) * .5235987756f,
                [&](const Face &face, float light) { emit(face, light, layer); });
    }
}

template <typename Emit>
void roof(float x, float z, float width, float depth, float eave, float rise, Roof style,
          Emit emit) {
    if (width <= 0 || depth <= 0)
        return;
    const float right = x + width, front = z + depth, mid = z + depth * .5f;
    if (style == Roof::Flat || rise <= 0) {
        emit(Face{Point{x, eave, z}, Point{right, eave, z}, Point{right, eave, front},
                  Point{x, eave, front}},
             1.f);
        return;
    }
    const float ridge = eave + rise;
    const float inset = style == Roof::Hip ? std::min(width * .28f, depth * .5f) : 0.f;
    const Point west{x + inset, ridge, mid}, east{right - inset, ridge, mid};
    emit(Face{west, east, Point{right, eave, front}, Point{x, eave, front}}, 1.f);
    emit(Face{east, west, Point{x, eave, z}, Point{right, eave, z}}, .78f);
    emit(Face{Point{x, eave, z}, west, Point{x, eave, front}, Point{x, eave, front}}, .88f);
    emit(Face{Point{right, eave, front}, east, Point{right, eave, z}, Point{right, eave, z}}, .72f);
}
} // namespace art::geometry
