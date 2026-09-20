#include "pallet3d.h"
#include "gb_presentation.h"

namespace {
bool relative_controls = false;
void attributes() {
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
}
bool begin(GBContext *ctx, bool menu_open, const uint32_t *framebuffer) {
    pallet3d_begin_frame(ctx, menu_open, framebuffer);
    return pallet3d_covers_frame(ctx);
}
bool frame(GBContext *ctx, int width, int height, bool menu_open) {
    pallet3d_draw(ctx, width, height, menu_open);
    return pallet3d_active();
}
void before_swap(int width, int height, bool menu_open) {
    pallet3d_finish_frame(width, height, menu_open);
    pallet3d_capture(width, height);
}
} // namespace

bool pallet3d_register() {
    GBPresentationHooks hooks{};
    hooks.api_version = GB_PRESENTATION_API_VERSION;
    hooks.struct_size = sizeof(hooks);
    hooks.gl_attributes = attributes;
    hooks.begin_frame = begin;
    hooks.frame = frame;
    hooks.event = pallet3d_event;
    hooks.before_swap = before_swap;
    hooks.shutdown = pallet3d_shutdown;
    hooks.input_poll = pallet3d_poll_controls;
    hooks.state_loaded = pallet3d_state_loaded;
    return gb_platform_set_presentation(&hooks);
}

void pallet3d_set_input_mask(uint8_t mask, bool relative) {
    if (relative != relative_controls) {
        const SDL_Scancode keys[] = {SDL_SCANCODE_W, SDL_SCANCODE_A, SDL_SCANCODE_S,
                                     SDL_SCANCODE_D};
        gb_platform_release_keys(keys, sizeof(keys) / sizeof(keys[0]));
    }
    relative_controls = relative;
    gb_platform_set_external_dpad(mask);
}
