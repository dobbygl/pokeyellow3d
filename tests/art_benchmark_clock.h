#pragma once
#include "gb_presentation.h"
#include "imgui.h"
#include "pallet3d.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef QA_DETERMINISTIC_SDL_CLOCK
void qa_advance_sdl_clock(float seconds);
#endif

// Shared by the current benchmark and the frozen reference renderer. Keep
// this adapter independent of menu oracles, whose APIs evolve between phases.
// Menu correctness is checked by the smoke suites outside the timed runs.
namespace benchmark_clock {
inline bool (*original_frame)(GBContext *, int, int, bool) = nullptr;
inline bool (*original_begin)(GBContext *, bool, const uint32_t *) = nullptr;
inline float seconds = 1.f / 60.f;
inline bool fixed = true;
inline bool begin(GBContext *ctx, bool menu_open, const uint32_t *framebuffer) {
#ifdef QA_DETERMINISTIC_SDL_CLOCK
    if (fixed)
        qa_advance_sdl_clock(seconds);
#endif
    return original_begin(ctx, menu_open, framebuffer);
}
inline bool frame(GBContext *ctx, int width, int height, bool menu_open) {
    if (fixed)
        ImGui::GetIO().DeltaTime = seconds;
    return original_frame(ctx, width, height, menu_open);
}
inline bool install() {
    if (const char *value = std::getenv("QA_FRAME_SECONDS")) {
        if (!std::strcmp(value, "wall"))
            fixed = false;
        else {
            char *end = nullptr;
            seconds = std::strtof(value, &end);
            if (end == value || *end || !std::isfinite(seconds) || seconds <= 0 || seconds > .1f)
                return false;
        }
    }
    auto hooks = pallet3d_presentation_hooks();
    original_begin = hooks.begin_frame;
    hooks.begin_frame = begin;
    original_frame = hooks.frame;
    hooks.frame = frame;
    std::fprintf(stderr, "[QA-CLOCK] presentation=%s seconds=%.9g; guest clock unchanged\n",
                 fixed ? "fixed" : "wall", seconds);
    return gb_platform_set_presentation(&hooks);
}
} // namespace benchmark_clock
