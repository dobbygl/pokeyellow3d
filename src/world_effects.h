#pragma once
#include <array>
#include <cmath>
#include <cstdint>

// Presentation-only observation. No GBContext, RNG, heap allocation or engine
// callback is available here. Coordinates and elapsed guest cycles are inputs.
namespace world_effects {
constexpr double ClockHz = 4194304.0, Tau = 6.283185307179586;
enum class Surface { None, Grass, Surf };
struct Particle {
    float x = 0, z = 0, vx = 0, vz = 0, age = 0, life = 0;
    Surface surface = Surface::None;
};
struct State {
    std::array<Particle, 96> particles{};
    int map = -1;
    uint32_t last_cycles = 0, emitted = 0;
    size_t cursor = 0;
    double seconds = 0;
    float x = 0, z = 0, distance = 0;
    Surface surface = Surface::None;
    void reset() {
        *this = {};
    }
    size_t count() const {
        size_t result = 0;
        for (const auto &p : particles)
            result += p.life > p.age;
        return result;
    }
    float wind() const {
        return float(std::fmod(seconds, Tau));
    }
    void update(int next_map, uint32_t cycles, bool paused, float px, float pz, Surface next) {
        uint32_t delta = cycles - last_cycles; // Includes the guest's 32-bit wrap.
        if (map != next_map || delta > uint32_t(ClockHz * 2)) {
            reset();
            map = next_map;
            last_cycles = cycles;
            x = px;
            z = pz;
            surface = next;
            return;
        }
        last_cycles = cycles;
        if (paused) {
            x = px;
            z = pz;
            distance = 0;
            surface = next;
            return;
        }
        if (!delta)
            return;
        float dt = float(delta / ClockHz);
        seconds += delta / ClockHz;
        for (auto &p : particles)
            if (p.life > p.age)
                p.age += dt;
        float mx = px - x, mz = pz - z;
        float travel = std::hypot(mx, mz);
        x = px;
        z = pz;
        if (travel > 1.5f || next != surface || next == Surface::None) {
            distance = 0;
            surface = next;
            return;
        }
        // Turning, idle frames and repeated presentation cannot create a trail.
        if (travel <= .0001f)
            return;
        distance += travel;
        if (distance < .35f)
            return;
        distance = std::fmod(distance, .35f);
        float forward_x = mx / travel, forward_z = mz / travel;
        for (int i = 0; i < 3; ++i) {
            // A stateless integer mixer provides variation, not a game RNG call.
            uint32_t seed = ++emitted + uint32_t(map) * 0x9e3779b9u;
            seed = (seed ^ (seed >> 16)) * 0x7feb352du;
            seed = (seed ^ (seed >> 15)) * 0x846ca68bu;
            seed ^= seed >> 16;
            // Fragments/spray leave the leading edge of the step. A small
            // forward impulse keeps them visible from eye level as well.
            float vx = forward_x * 3.5f + (float(seed & 255) / 255 - .5f) * .75f;
            float vz = forward_z * 3.5f + (float((seed >> 8) & 255) / 255 - .5f) * .75f;
            particles[cursor] = {px + forward_x * .7f,
                                 pz + forward_z * .7f,
                                 vx,
                                 vz,
                                 0,
                                 next == Surface::Surf ? .48f : .62f,
                                 next};
            cursor = (cursor + 1) % particles.size();
        }
    }
};
} // namespace world_effects
