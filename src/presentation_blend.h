#pragma once
#include <algorithm>
#include <cstdint>

namespace presentation {
// Presentation time only; never changes the guest clock or delays its frames.
struct Blend {
    static constexpr float Duration = 200;
    bool initialized = false, target = false, running = false, paused = false;
    float elapsed = 0;
    uint32_t last = 0;
    bool begin(bool desired, bool frozen, uint32_t now, bool history) {
        if (!initialized) {
            initialized = true;
            target = desired;
            last = now;
            paused = frozen;
            return false;
        }
        float delta = paused ? 0.f : std::min(25.f, float(uint32_t(now - last)));
        last = now;
        paused = frozen;
        if (frozen)
            return false;
        if (desired != target) {
            target = desired;
            elapsed = 0;
            running = history;
            return running; // Freeze the last completed (possibly mixed) frame.
        }
        if (running) {
            elapsed = std::min(Duration, elapsed + delta);
            running = elapsed < Duration;
        }
        return false;
    }
    float progress() const {
        return std::clamp(elapsed / Duration, 0.f, 1.f);
    }
};
} // namespace presentation
