#include "menu_motion.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void require(bool value, const char *message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
int main() {
    using namespace menu_motion;
    const Key start{{10, 0, 10, 16}, 0}, bottom{{0, 12, 20, 6}, 0};
    const Key quantity{{6, 10, 14, 3}, 0};
    const Bounds box{20, 30, 220, 330};
    Motion motion;
    uint32_t cycles = 1000000;
    motion.begin(cycles, false);
    motion.finish(true); // A visible world, with no menu yet.
    motion.begin(cycles += 70224, false);
    motion.submit(start, box);
    motion.finish(true);
    require(motion.panels.size() == 1 && motion.panels[0].opacity() == 0 &&
                motion.panels[0].offset() == 8,
            "new panel begins its fade and displacement immediately");
    motion.begin(cycles += Duration / 2, false);
    motion.submit(start, box);
    motion.finish(true);
    require(std::abs(motion.panels[0].opacity() - .5f) < .00001f &&
                std::abs(motion.panels[0].offset() - 4.f) < .0001f,
            "half of the emulated duration gives a half-open panel");
    float half = motion.panels[0].amount;
    motion.begin(cycles += Duration * 10, true);
    motion.finish(true);
    require(motion.panels[0].amount == half && motion.panels[0].target,
            "Esc or focus loss freezes progress and does not close an undrawn menu");
    motion.begin(cycles += Duration * 10, false);
    motion.submit(start, box);
    motion.finish(true);
    require(motion.panels[0].amount == half, "resume discards time elapsed while paused");
    motion.begin(cycles += Duration / 2, false);
    motion.submit(start, box);
    motion.submit(bottom, box);
    motion.finish(true);
    require(motion.panels[0].opacity() == 1 && motion.panels[0].offset() == 0 &&
                motion.panels[1].opacity() == 0,
            "Start retains its phase while its confirmation region opens");
    motion.begin(cycles += Duration, false);
    motion.submit(start, box);
    motion.submit(bottom, box);
    motion.finish(true);
    motion.begin(cycles += 70224, false);
    motion.submit(bottom, {10, 10, 250, 350});
    motion.submit(quantity, box);
    motion.finish(true);
    require(motion.panels.size() == 3 && !motion.panels[0].target &&
                motion.panels[1].key == bottom && motion.panels[1].opacity() == 1 &&
                motion.panels[1].bounds.left == 10 && motion.panels[2].opacity() == 0,
            "retiring panels stay behind retained regions and only new regions animate");
    motion.begin(cycles += Duration, false);
    motion.submit(bottom, box);
    motion.submit(quantity, box);
    motion.finish(true);
    require(motion.panels.size() == 2, "closed decoration expires after 150 ms");
    motion.begin(cycles += 70224, false);
    motion.finish(true);
    motion.begin(cycles += Duration / 2, false);
    motion.finish(true);
    require(motion.panels.size() == 2 && std::abs(motion.panels[0].opacity() - .5f) < .00001f,
            "closing continues even when no recognized region is submitted");
    motion.begin(cycles += Duration / 2, false);
    motion.finish(true);
    require(motion.panels.empty(), "fully closed regions leave no persistent decoration");
    motion.reset(); // Explicit successful savestate load, including a forward clock jump.
    motion.begin(cycles += Duration * 100, false);
    motion.finish(false); // Cold presentation is not ready yet.
    motion.begin(cycles += 70224, false);
    motion.submit(start, box);
    motion.finish(true);
    require(motion.panels[0].opacity() == 1, "loaded open menu is visible without animation");
    motion.begin(1, false);
    motion.submit(bottom, box);
    motion.finish(true);
    require(motion.panels.size() == 1 && motion.panels[0].key == bottom &&
                motion.panels[0].opacity() == 1,
            "clock rollback discards old presentation and snaps the loaded menu");
    motion.begin(2, false, false);
    motion.submit(start, box);
    motion.finish(true);
    require(motion.panels.size() == 1 && motion.panels[0].opacity() == 1,
            "unanimated comparison presents new regions immediately");
    motion.begin(3, false, false);
    motion.finish(true);
    require(motion.panels.empty(), "unanimated comparison closes immediately");
    motion.begin(4, false);
    motion.submit(start, box);
    motion.submit({start.region, 1}, box);
    motion.finish(true);
    require(motion.panels.size() == 2, "world and battle regions have independent identities");
    motion.hide(start);
    motion.finish(true);
    require(motion.panels.size() == 2 && !motion.panels[1].visible,
            "occluded or native regions retain identity without drawing decoration");
    motion.begin(4 + Duration, false);
    motion.hide(start);
    motion.submit({start.region, 1}, box);
    motion.finish(true);
    motion.begin(5 + Duration, false);
    motion.submit(start, box);
    motion.finish(true);
    auto restored = std::find_if(motion.panels.begin(), motion.panels.end(),
                                 [&](const Panel &panel) { return panel.key == start; });
    require(restored != motion.panels.end() && restored->visible && restored->opacity() == 1,
            "uncovering a shared region does not restart its appearance");
    Motion wrapped;
    uint32_t near_wrap = UINT32_MAX - 200000;
    wrapped.begin(near_wrap, false);
    wrapped.finish(true);
    wrapped.begin(near_wrap += 10000, false);
    wrapped.submit(start, box);
    wrapped.finish(true);
    wrapped.begin(near_wrap += Duration / 2, false);
    wrapped.submit(start, box);
    wrapped.finish(true);
    require(wrapped.panels.size() == 1 && std::abs(wrapped.panels[0].opacity() - .5f) < .00001f,
            "normal 32-bit clock wrap advances instead of snapping the animation");
    std::puts("PASS: cycle timing/wrap, pause/resume, shared regions, closing and load seeding");
}
