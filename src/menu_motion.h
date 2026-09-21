#pragma once
#include "menu_layout.h"
#include <algorithm>
#include <cstdint>
#include <vector>

// Cosmetic state only: no text, input, framebuffer or guest memory is retained.
namespace menu_motion {
constexpr uint32_t Duration = 629146; // 150 ms at the Game Boy's 4194304 Hz.
constexpr float Travel = 8;
struct Key {
    menu_layout::Rect region;
    int placement = 0;
    bool operator==(const Key &other) const {
        return placement == other.placement && region.x == other.region.x &&
               region.y == other.region.y && region.w == other.region.w &&
               region.h == other.region.h;
    }
};
struct Bounds {
    float left = 0, top = 0, right = 0, bottom = 0;
};
struct Panel {
    Key key;
    Bounds bounds;
    float amount = 0;
    bool target = true, seen = false, visible = true;
    int order = 0;
    float opacity() const {
        return amount * amount * (3 - 2 * amount);
    }
    float offset() const {
        return Travel * (1 - opacity());
    }
};
struct Motion {
    std::vector<Panel> panels;
    uint32_t last = 0;
    bool clock_ready = false, primed = false, paused = false, animated = true;
    int order = 0;

    void reset() {
        *this = Motion{};
    }
    void begin(uint32_t cycles, bool frozen, bool animate = true) {
        if (clock_ready && cycles < last && last - cycles < 0x80000000u)
            reset();
        // Unsigned subtraction includes the guest's normal 32-bit clock wrap.
        uint32_t elapsed = clock_ready && !paused && !frozen ? cycles - last : 0;
        last = cycles;
        clock_ready = true;
        paused = frozen;
        animated = animate;
        order = 0;
        float step = float(std::min(elapsed, Duration)) / float(Duration);
        for (auto &panel : panels) {
            panel.seen = false;
            if (!frozen)
                panel.amount =
                    !animate ? float(panel.target)
                             : std::clamp(panel.amount + (panel.target ? step : -step), 0.f, 1.f);
        }
    }
    void submit(Key key, Bounds bounds) {
        auto found = std::find_if(panels.begin(), panels.end(),
                                  [&](const Panel &panel) { return panel.key == key; });
        if (found == panels.end()) {
            panels.push_back({key, bounds, primed && animated && !paused ? 0.f : 1.f});
            found = panels.end() - 1;
        }
        found->bounds = bounds;
        found->target = found->seen = true;
        found->visible = true;
        found->order = order++;
    }
    void hide(Key key) {
        auto found = std::find_if(panels.begin(), panels.end(),
                                  [&](const Panel &panel) { return panel.key == key; });
        if (found == panels.end()) {
            panels.push_back({key, {}, 1});
            found = panels.end() - 1;
        }
        // Occlusion, native fallback and an empty battle message do not create
        // a new region. Keep its phase without painting any decoration.
        found->target = found->seen = true;
        found->visible = false;
        found->order = order++;
    }
    // An eligible empty world frame seeds the history too. Loading a state with
    // an open menu snaps it; loading a world and subsequently opening one does
    // not accidentally suppress its first animation.
    void finish(bool eligible) {
        if (paused)
            return;
        primed |= eligible;
        for (auto &panel : panels)
            if (!panel.seen) {
                panel.target = false;
                if (!animated)
                    panel.amount = 0;
            }
        panels.erase(
            std::remove_if(panels.begin(), panels.end(),
                           [](const Panel &panel) { return !panel.target && panel.amount == 0; }),
            panels.end());
        // Retiring decoration stays behind the currently owned regions; current
        // regions retain the source layout's overlap order.
        std::stable_sort(panels.begin(), panels.end(), [](const Panel &a, const Panel &b) {
            if (a.target != b.target)
                return !a.target;
            return a.order < b.order;
        });
    }
};
inline Motion state;
// The private QA helper disables cosmetics to compare identical engine inputs.
// The playable application always leaves this enabled.
inline bool enabled = true;
} // namespace menu_motion
