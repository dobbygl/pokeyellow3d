#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Presentation-only solid volumes, indexed in world coordinates. No cartridge,
// GL resources, guest memory or clock dependencies. Built with the map mesh.
namespace art::ambient {
struct Point {
    float x, y, z;
};
struct Box {
    Point low, high;
};
class Field {
    // Quarter-cell horizontal columns and 1/8-cell vertical slices. The
    // world-aligned lattice keeps neighbouring maps identical at a seam.
    // One column fits in a 64-bit mask: all supported occluders are below 8.
    static constexpr int Horizontal = 4, Vertical = 8, Height = 64;
    int x_ = 0, z_ = 0, width_ = 0, depth_ = 0;
    std::vector<uint64_t> columns_;
    bool occupied_ = false;
    // Whole world cells within one cell of any solid column. Derived from
    // the columns alone, lazily after the last add: probes never reach
    // further than one cell, so other vertices are unoccluded.
    mutable std::vector<uint8_t> near_;
    mutable bool near_dirty_ = true;
    bool near(Point p) const {
        if (near_dirty_) {
            const int w = width_ / Horizontal, d = depth_ / Horizontal;
            std::vector<uint8_t> solid(size_t(w) * d, 0);
            for (int z = 0; z < depth_; ++z)
                for (int x = 0; x < width_; ++x)
                    if (columns_[size_t(z) * width_ + x])
                        solid[size_t(z / Horizontal) * w + x / Horizontal] = 1;
            near_.assign(solid.size(), 0);
            for (int z = 0; z < d; ++z)
                for (int x = 0; x < w; ++x)
                    for (int dz = -1; dz <= 1 && !near_[size_t(z) * w + x]; ++dz)
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int nx = x + dx, nz = z + dz;
                            if (nx >= 0 && nz >= 0 && nx < w && nz < d &&
                                solid[size_t(nz) * w + nx]) {
                                near_[size_t(z) * w + x] = 1;
                                break;
                            }
                        }
            near_dirty_ = false;
        }
        const float fx = p.x - x_, fz = p.z - z_;
        if (!(fx >= 0 && fz >= 0 && fx < width_ / Horizontal && fz < depth_ / Horizontal))
            return true; // Outside: let the probes decide (they read nothing).
        return near_[size_t(int(fz)) * (width_ / Horizontal) + int(fx)];
    }

    // Lattice cells touched by a volume: [left, right) x [back, front) and a
    // slice mask. Conservative, so posts and trunks thinner than a lattice
    // cell still occupy it. Floats are clipped before integer conversion.
    struct Span {
        int left = 0, right = 0, back = 0, front = 0;
        uint64_t mask = 0;
        bool empty() const {
            return left >= right || back >= front || !mask;
        }
    };
    Span span(Box box) const {
        Span s;
        for (auto pair : {std::array<float, 2>{box.low.x, box.high.x},
                          std::array<float, 2>{box.low.y, box.high.y},
                          std::array<float, 2>{box.low.z, box.high.z}})
            if (!std::isfinite(pair[0]) || !std::isfinite(pair[1]) || pair[0] >= pair[1])
                return s;
        auto low = [](float value, int count) {
            return int(std::floor(std::clamp(value, 0.f, float(count))));
        };
        auto high = [](float value, int count) {
            return int(std::ceil(std::clamp(value, 0.f, float(count))));
        };
        s.left = low((box.low.x - x_) * Horizontal, width_);
        s.back = low((box.low.z - z_) * Horizontal, depth_);
        s.right = high((box.high.x - x_) * Horizontal, width_);
        s.front = high((box.high.z - z_) * Horizontal, depth_);
        const int bottom = low(box.low.y * Vertical, Height);
        const int top = high(box.high.y * Vertical, Height);
        const uint64_t low_mask = bottom == Height ? ~uint64_t(0) : (uint64_t(1) << bottom) - 1;
        const uint64_t high_mask = top == Height ? ~uint64_t(0) : (uint64_t(1) << top) - 1;
        s.mask = high_mask & ~low_mask;
        return s;
    }
    // Lattice cell of a point, or false when it lies outside the field.
    bool cell(Point p, int &x, int &z, int &slice) const {
        const float fx = (p.x - x_) * Horizontal, fz = (p.z - z_) * Horizontal;
        const float fy = p.y * Vertical;
        // Ordered comparisons reject NaNs as well as out-of-range values.
        if (!(fx >= 0 && fz >= 0 && fy >= 0 && fx < width_ && fz < depth_ && fy < Height))
            return false;
        x = int(fx);
        z = int(fz);
        slice = int(fy);
        return true;
    }

  public:
    // Lattice cells of an emitting solid's own volumes, resolved once per
    // solid and skipped by its faces' probes.
    struct Owner {
        std::array<Span, 8> spans{};
        size_t count = 0;
    };
    Owner owner(const Box *boxes, size_t count) const {
        Owner result;
        result.count = std::min(count, result.spans.size());
        for (size_t i = 0; i < result.count; ++i)
            result.spans[i] = span(boxes[i]);
        return result;
    }
    void reset(int x, int z, int width, int depth) {
        x_ = x;
        z_ = z;
        width_ = std::max(0, width) * Horizontal;
        depth_ = std::max(0, depth) * Horizontal;
        columns_.assign(size_t(width_) * depth_, 0);
        occupied_ = false;
        near_dirty_ = true;
    }
    bool add(Box box) {
        for (auto pair : {std::array<float, 2>{box.low.x, box.high.x},
                          std::array<float, 2>{box.low.y, box.high.y},
                          std::array<float, 2>{box.low.z, box.high.z}})
            if (!std::isfinite(pair[0]) || !std::isfinite(pair[1]) || pair[0] >= pair[1])
                return false;
        const Span s = span(box);
        if (s.empty())
            return true; // Valid volume entirely outside this field.
        for (int z = s.back; z < s.front; ++z)
            for (int x = s.left; x < s.right; ++x)
                columns_[size_t(z) * width_ + x] |= s.mask;
        occupied_ = true;
        near_dirty_ = true;
        return true;
    }
    bool contains(Point p) const {
        int x, z, slice;
        return cell(p, x, z, slice) && (columns_[size_t(z) * width_ + x] >> slice) & 1;
    }
    // Fraction of light reaching a surface point. Four probes sit just in
    // front of the surface, along its dominant axis, spread tangentially.
    // Probes start from the exact vertex, never a snapped lattice point, and
    // skip the lattice cells of the emitting solid's own volumes: a surface
    // is only darkened by other solids. Pure function of its inputs, so
    // build order, caches and map partition cannot change the result.
    float visibility(Point p, Point normal, const Box *self = nullptr, size_t selves = 0) const {
        if (!occupied_)
            return 1;
        return visibility(p, normal, owner(self, selves));
    }
    float visibility(Point p, Point normal, const Owner &own) const {
        if (!occupied_ || !near(p))
            return 1;
        const float ax = std::abs(normal.x), ay = std::abs(normal.y), az = std::abs(normal.z);
        if (!std::isfinite(ax + ay + az) || ax + ay + az < .001f)
            return 1;
        const int axis = ay >= ax && ay >= az ? 1 : ax >= az ? 0 : 2;
        const float sign = (axis == 0   ? normal.x
                            : axis == 1 ? normal.y
                                        : normal.z) > 0
                               ? 1.f
                               : -1.f;
        int covered = 0;
        for (float a : {-1.f, 1.f})
            for (float b : {-1.f, 1.f}) {
                Point q = p;
                float *along = axis == 0 ? &q.x : axis == 1 ? &q.y : &q.z;
                float *first = axis == 0 ? &q.y : &q.x;
                float *second = axis == 2 ? &q.y : &q.z;
                *along += sign * Offset;
                *first += a * Spread;
                *second += b * Spread;
                int x, z, slice;
                if (!cell(q, x, z, slice) || !((columns_[size_t(z) * width_ + x] >> slice) & 1))
                    continue;
                bool mine = false;
                for (size_t i = 0; i < own.count && !mine; ++i) {
                    const Span &s = own.spans[i];
                    mine = x >= s.left && x < s.right && z >= s.back && z < s.front &&
                           ((s.mask >> slice) & 1);
                }
                covered += !mine;
            }
        return 1 - covered * Strength;
    }
    static constexpr float Offset = .12f, Spread = .35f, Strength = .08f;
};

// Normalized distance from a contact's centre. Contacts are triangle fans
// whose alpha is interpolated linearly by the GPU between the centre and the
// rim, so the profile is linear: a dark centre fading to zero at the edge.
inline float contact_alpha(float radius) {
    const float t = std::clamp(radius, 0.f, 1.f);
    return .22f * (1 - t);
}
} // namespace art::ambient
