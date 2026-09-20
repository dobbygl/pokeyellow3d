#pragma once
#include "fade_state.h"

namespace menu_state {
inline bool running(const GBContext* ctx) {
    // OverworldLoop's Start/NPC call and PrintPredefTextID's hidden-event/PC
    // call. Both instructions are in the manifest's home bank. Their live
    // stack returns survive party/PC screens that reuse the map's WRAM.
    return ctx&&ctx->wram&&!ctx->wram[0xd056-0xc000]&&
        (fade::home_call(ctx,0x2de,0x2817)||fade::home_call(ctx,0x3f47,0x2817));
}
} // namespace menu_state
