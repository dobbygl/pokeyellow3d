#pragma once
#include "gb_presentation.h"
#include "imgui.h"
#include "pallet3d.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Only the smoke executable includes this adapter. The playable application
// keeps SDL/ImGui's real-time clock and the production presentation callbacks.
// Frame stepping alone does not fix camera easing: host rendering cost would
// otherwise change the accumulated presentation time at a capture waypoint.
namespace qa_clock {
inline bool (*original_frame)(GBContext *, int, int, bool) = nullptr;
inline GBContext *context = nullptr;
inline float seconds = 1.f / 60.f;
inline bool fixed = true;
inline bool frame(GBContext *ctx, int width, int height, bool menu_open) {
    context = ctx;
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
            if (!end || end == value || *end || !std::isfinite(seconds) || seconds <= 0 ||
                seconds > .1f) {
                std::fprintf(stderr, "QA_FRAME_SECONDS must be wall or a number in (0, 0.1]\n");
                return false;
            }
        }
    }
    auto hooks = pallet3d_presentation_hooks();
    original_frame = hooks.frame;
    hooks.frame = frame;
    std::fprintf(stderr, "[QA-CLOCK] presentation=%s seconds=%.9g; guest clock unchanged\n",
                 fixed ? "fixed" : "wall", seconds);
    return gb_platform_set_presentation(&hooks);
}
inline void capture_state(const char *image_path) {
    if (context && std::getenv("QA_CAPTURE_STATES")) {
        std::string path = std::string(image_path) + ".machine";
        if (!gb_context_save_state_file(context, path.c_str())) {
            std::fprintf(stderr, "Failed to capture private machine state: %s\n", path.c_str());
            std::exit(19);
        }
    }
}
} // namespace qa_clock
