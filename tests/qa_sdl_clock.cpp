// Link only into the Linux smoke test. Both the unmodified release renderer
// and its candidate use this adapter; the playable executable never does.
#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>

namespace {
uint64_t elapsed_us = 1000000;
bool deterministic() {
    const char *value = std::getenv("QA_FRAME_SECONDS");
    return !value || std::strcmp(value, "wall");
}
} // namespace

extern "C" Uint32 __real_SDL_GetTicks();
extern "C" void __real_SDL_Delay(Uint32 ms);
extern "C" Uint32 __wrap_SDL_GetTicks() {
    return deterministic() ? Uint32(elapsed_us / 1000) : __real_SDL_GetTicks();
}
extern "C" void __wrap_SDL_Delay(Uint32 ms) {
    if (deterministic())
        elapsed_us += uint64_t(ms) * 1000;
    else
        __real_SDL_Delay(ms);
}
void qa_advance_sdl_clock(float seconds) {
    if (deterministic())
        elapsed_us += uint64_t(seconds * 1000000 + .5f);
}
