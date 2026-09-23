#pragma once
#include "firstperson.h"
#include <limits>
#include <vector>

// Presentation-only light-space fitting. No cartridge or GPU dependencies.
namespace sunlight {
using Point = std::array<double, 3>;
inline double dot(Point a, Point b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
inline Point cross(Point a, Point b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
inline Point unit(Point p) {
    const double length = std::sqrt(dot(p, p));
    return length > 1e-12 ? Point{p[0] / length, p[1] / length, p[2] / length} : Point{};
}
struct Bounds {
    Point low{INFINITY, INFINITY, INFINITY}, high{-INFINITY, -INFINITY, -INFINITY};
    void add(Point p) {
        for (int i = 0; i < 3; ++i) {
            low[i] = std::min(low[i], p[i]);
            high[i] = std::max(high[i], p[i]);
        }
    }
    bool valid() const {
        for (int i = 0; i < 3; ++i)
            if (!std::isfinite(low[i]) || !std::isfinite(high[i]) || low[i] > high[i])
                return false;
        return true;
    }
    std::array<Point, 8> corners() const {
        std::array<Point, 8> result{};
        for (int i = 0; i < 8; ++i)
            result[i] = {i & 1 ? high[0] : low[0], i & 2 ? high[1] : low[1],
                         i & 4 ? high[2] : low[2]};
        return result;
    }
};
struct Plane {
    Point n;
    double d;
};
// Intersection of the resident volume and the actual camera's six clip planes.
// Enumerating plane triples also handles a camera inside the volume, a frustum
// containing the whole map, and thin orthographic windows with no map corner in view.
inline std::vector<Point> receivers(const Bounds &bounds, const firstperson::Matrix &camera) {
    std::vector<Plane> planes;
    for (int axis = 0; axis < 3; ++axis) {
        Point n{};
        n[axis] = 1;
        planes.push_back({n, -bounds.low[axis]});
        n[axis] = -1;
        planes.push_back({n, bounds.high[axis]});
        for (int sign : {-1, 1}) {
            Plane p{{camera[3] + sign * camera[axis], camera[7] + sign * camera[4 + axis],
                     camera[11] + sign * camera[8 + axis]},
                    camera[15] + sign * camera[12 + axis]};
            double length = std::sqrt(dot(p.n, p.n));
            if (length < 1e-12)
                return {};
            for (double &value : p.n)
                value /= length;
            p.d /= length;
            planes.push_back(p);
        }
    }
    std::vector<Point> result;
    for (size_t i = 0; i < planes.size(); ++i)
        for (size_t j = i + 1; j < planes.size(); ++j)
            for (size_t k = j + 1; k < planes.size(); ++k) {
                const auto &a = planes[i], &b = planes[j], &c = planes[k];
                auto bc = cross(b.n, c.n), ca = cross(c.n, a.n), ab = cross(a.n, b.n);
                double determinant = dot(a.n, bc);
                if (std::abs(determinant) < 1e-10)
                    continue;
                Point p{};
                for (int axis = 0; axis < 3; ++axis)
                    p[axis] = (-a.d * bc[axis] - b.d * ca[axis] - c.d * ab[axis]) / determinant;
                bool inside = true;
                for (const auto &plane : planes)
                    inside &= dot(plane.n, p) + plane.d >= -1e-6;
                if (inside)
                    result.push_back(p);
            }
    return result;
}
struct Projection {
    bool valid = false;
    firstperson::Matrix matrix{};
    float depth_span = 1, texel_world = 1;
};
inline Projection fit(const Bounds &resident, const firstperson::Matrix &camera, Point sun,
                      int resolution = 1024) {
    Projection out;
    if (!resident.valid() || resolution < 4 || dot(sun, sun) < .5 || !std::isfinite(dot(sun, sun)))
        return out;
    for (float value : camera)
        if (!std::isfinite(value))
            return out;
    auto points = receivers(resident, camera);
    if (points.empty())
        return out;
    Point forward = unit(sun);
    Point right =
        unit(cross(std::abs(forward[1]) < .99 ? Point{0, 1, 0} : Point{0, 0, 1}, forward));
    Point up = cross(forward, right);
    Bounds light;
    for (auto p : points)
        light.add({dot(right, p), dot(up, p), dot(forward, p)});
    // Offscreen casters along the sun ray remain eligible: crop only light X/Y,
    // and retain the complete resident depth interval, not the camera's depth.
    for (auto p : resident.corners()) {
        double z = dot(forward, p);
        light.low[2] = std::min(light.low[2], z);
        light.high[2] = std::max(light.high[2], z);
    }
    for (int axis = 0; axis < 3; ++axis) {
        light.low[axis] -= .15;
        light.high[axis] += .15;
    }
    for (int row = 0; row < 3; ++row) {
        const auto &basis = row == 0 ? right : row == 1 ? up : forward;
        double span = light.high[row] - light.low[row];
        double scale = (row == 2 ? -2.0 : 2.0) / span;
        for (int col = 0; col < 3; ++col)
            out.matrix[col * 4 + row] = float(basis[col] * scale);
        out.matrix[12 + row] = float(-.5 * (light.low[row] + light.high[row]) * scale);
    }
    out.matrix[15] = 1;
    out.depth_span = float(light.high[2] - light.low[2]);
    out.texel_world =
        float(std::max(light.high[0] - light.low[0], light.high[1] - light.low[1]) / resolution);
    out.valid = true;
    return out;
}
} // namespace sunlight
