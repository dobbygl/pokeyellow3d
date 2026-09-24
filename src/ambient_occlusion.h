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
    // Quarter-cell horizontal samples and 1/8-cell vertical slices. The
    // world-aligned lattice keeps neighbouring maps identical at a seam.
    // One column fits in a 64-bit mask: all supported occluders are below 8.
    static constexpr int Horizontal = 4, Vertical = 8, Height = 64;
    int x_ = 0, z_ = 0, width_ = 0, depth_ = 0;
    std::vector<uint64_t> columns_;
    bool occupied_ = false;
    mutable bool cache_dirty_ = true;
    mutable std::vector<uint8_t> visibility_;
    static constexpr int CacheHeight = Height / 2 + 1;
    int cache_height_ = 1;

  public:
    void reset(int x, int z, int width, int depth) {
        x_ = x;
        z_ = z;
        width_ = std::max(0, width) * Horizontal;
        depth_ = std::max(0, depth) * Horizontal;
        columns_.assign(size_t(width_) * depth_, 0);
        occupied_ = false;
        cache_dirty_ = true;
        cache_height_ = 1;
    }
    bool add(Box box) {
        for (auto pair : {std::array<float, 2>{box.low.x, box.high.x},
                          std::array<float, 2>{box.low.y, box.high.y},
                          std::array<float, 2>{box.low.z, box.high.z}})
            if (!std::isfinite(pair[0]) || !std::isfinite(pair[1]) || pair[0] >= pair[1])
                return false;
        // A sample is solid only when its centre is inside the volume. Clip
        // floats before integer conversion so distant finite boxes are safe.
        auto index = [](float value, int count) {
            return int(std::ceil(std::clamp(value - .5f, 0.f, float(count))));
        };
        const int left = index((box.low.x - x_) * Horizontal, width_);
        const int back = index((box.low.z - z_) * Horizontal, depth_);
        const int right = index((box.high.x - x_) * Horizontal, width_);
        const int front = index((box.high.z - z_) * Horizontal, depth_);
        const int bottom = index(box.low.y * Vertical, Height);
        const int top = index(box.high.y * Vertical, Height);
        const uint64_t low_mask = bottom == Height ? ~uint64_t(0) : (uint64_t(1) << bottom) - 1;
        const uint64_t high_mask = top == Height ? ~uint64_t(0) : (uint64_t(1) << top) - 1;
        const uint64_t mask = high_mask & ~low_mask;
        for (int z = back; z < front; ++z)
            for (int x = left; x < right; ++x)
                columns_[size_t(z) * width_ + x] |= mask;
        occupied_ |= mask != 0 && left < right && back < front;
        cache_dirty_ = true;
        cache_height_ = std::max(cache_height_, std::min(CacheHeight, (top + 1) / 2 + 3));
        return true;
    }
    bool contains(Point p) const {
        const float x = (p.x - x_) * Horizontal, z = (p.z - z_) * Horizontal;
        const float y = p.y * Vertical;
        // Ordered comparisons reject NaNs as well as out-of-range values.
        if (!(x >= 0 && z >= 0 && y >= 0 && x < width_ && z < depth_ && y < Height))
            return false;
        return (columns_[size_t(int(z)) * width_ + int(x)] >> int(y)) & 1;
    }
    // AO varies slowly compared with the ROM texture: cache local hemisphere
    // coverage at half-cell X/Z vertices and quarter-cell heights, separately
    // for all six face directions. Coordinates are canonical world samples,
    // never the first vertex to hit a bucket, so build order cannot add seams.
    float visibility(Point p, Point normal) const {
        if (!occupied_)
            return 1;
        const float ax = std::abs(normal.x), ay = std::abs(normal.y), az = std::abs(normal.z);
        if (!std::isfinite(ax + ay + az) || ax + ay + az < .001f)
            return 1;
        const int axis = ay >= ax && ay >= az ? 1 : ax >= az ? 0 : 2;
        const bool positive = (axis == 0 ? normal.x : axis == 1 ? normal.y : normal.z) > 0;
        const int direction = axis * 2 + positive;
        const int width = width_ / 2 + 1, depth = depth_ / 2 + 1;
        const float fx = (p.x - x_) * 2 + .5f, fz = (p.z - z_) * 2 + .5f;
        const float fy = p.y * 4 + .5f;
        if (!(fx >= 0 && fz >= 0 && fy >= 0 && fx < width && fz < depth && fy < cache_height_))
            return 1;
        const int x = int(fx), z = int(fz), y = int(fy);
        if (cache_dirty_) {
            visibility_.assign(size_t(width) * depth * cache_height_ * 6, 255);
            cache_dirty_ = false;
        }
        auto &coverage = visibility_[((size_t(z) * width + x) * cache_height_ + y) * 6 + direction];
        if (coverage != 255)
            return 1 - coverage * .08f;
        // The canonical probes land at fixed offsets in the occupancy
        // lattice. Resolve them with integer column reads rather than four
        // repeated float transforms, bounds checks and Y quantizations.
        auto column = [&](int cx, int cz) -> uint64_t {
            if (unsigned(cx) >= unsigned(width_) || unsigned(cz) >= unsigned(depth_))
                return 0;
            return columns_[size_t(cz) * width_ + cx];
        };
        auto bit = [](int slice) -> uint64_t {
            return unsigned(slice) < Height ? uint64_t(1) << slice : 0;
        };
        coverage = 0;
        if (axis == 1) {
            const uint64_t mask = bit(y * 2 - !positive);
            for (int dz : {-2, 1})
                for (int dx : {-2, 1})
                    coverage += (column(x * 2 + dx, z * 2 + dz) & mask) != 0;
        } else {
            const uint64_t low = bit(y * 2 - 3), high = bit(y * 2 + 2);
            for (int side : {-2, 1}) {
                const uint64_t mask = axis == 0 ? column(x * 2 - !positive, z * 2 + side)
                                                : column(x * 2 + side, z * 2 - !positive);
                coverage += (mask & low) != 0;
                coverage += (mask & high) != 0;
            }
        }
        return 1 - coverage * .08f;
    }
};

// Normalized distance from a contact's centre. Keep the existing footprint;
// interpolate transparency to zero at its edge instead of a uniform disk.
inline float contact_alpha(float radius) {
    const float t = std::clamp(radius, 0.f, 1.f);
    return .22f * (1 - t) * (1 - t);
}
} // namespace art::ambient
