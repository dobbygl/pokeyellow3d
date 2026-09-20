#pragma once

#include "gbrt.h"
#include "world_scene.h"
#include "battle_state.h"
#include "dex_state.h"
#include "dex_area_state.h"
#include "pc_state.h"
#include <array>
#include <cstdint>

namespace pallet {
// Canonical UE Yellow, SHA-1 cc7d03262ebfaf2f06772c1a480c7d9d5f4a38e1.
// Addresses verified against pokeyellow_internal.h and pret's symbols branch.
constexpr uint16_t Sprite1 = 0xc100, Sprite2 = 0xc200;
constexpr uint16_t Font = 0xcfc3, Walk = 0xcfc4, UpdateSprites = 0xcfca;
constexpr uint16_t Battle = 0xd056, Map = 0xd35d, Y = 0xd360, X = 0xd361;
constexpr uint16_t Tileset = 0xd366, Height = 0xd367, Width = 0xd368;
constexpr uint16_t HiddenFlags = 0xd5a5, HiddenList = 0xd5cd;
constexpr size_t FacingTable = 0x4000;

inline uint8_t read(const GBContext* ctx, uint16_t addr) {
    return ctx->wram[addr - 0xc000];
}

enum class View { Unsupported, Transition, Dialogue, Overworld, Battle, Pokedex, Computer, PokedexArea };

inline bool valid_live_map(const GBContext* ctx,const Scene& s) {
    const int bw=s.width/2,bh=s.height/2;
    if((bw+6)*(bh+6)>1300)return false; // wOverworldMap's canonical allocation.
    const auto& valid=tileset(s).valid_blocks;
    for(int z=0;z<bh;z++)for(int x=0;x<bw;x++)
        if(!valid[read(ctx,uint16_t(0xc6e8+(z+3)*(bw+6)+x+3))])return false;
    return true;
}

inline View view(const GBContext* ctx) {
    if (!ctx || !ctx->wram || !ctx->rom || !ctx->io || !ctx->vram ||
        ctx->rom_size != 1048576) return View::Unsupported;
    if(!load_catalog(ctx->rom,ctx->rom_size))return View::Unsupported;
    const Scene* s=ensure_scene(read(ctx,Map));
    if(read(ctx,Battle))return s&&battle::ready(ctx)?View::Battle:View::Unsupported;
    if(dex_area_state::active(ctx))return View::PokedexArea;
    if(dex_state::data(ctx)||dex_state::list(ctx).number)return View::Pokedex;
    if(pc_state::sample(ctx).mode!=pc_state::Mode::None)return View::Computer;
    if (!s || read(ctx, Width)*2 != s->width || read(ctx, Height)*2 != s->height ||
        read(ctx, Tileset) != s->tileset || read(ctx, Battle) != 0 ||
        read(ctx, X) >= s->width || read(ctx, Y) >= s->height)
        return View::Unsupported;
    if (!(ctx->io[0x40] & 0x80) || (ctx->io[0x47] != 0xe4) ||
        read(ctx, UpdateSprites) == 0 || !read(ctx, Sprite1))
        return View::Transition;
    // The font is loaded for both conversations and the original Start menu.
    if (read(ctx, Font) & 1) return View::Dialogue;
    // Overworld tiles are 00..5F. Text/window tiles also catch the Continue menu
    // before its font flag is set, without treating temporary sprite-update FF
    // (normal during every walking step) as a map transition.
    for (int i=0;i<20*18;i++)
        if (read(ctx,0xc3a0+i)>=0x60) return View::Dialogue;
    if(!valid_live_map(ctx,*s))return View::Transition;
    return View::Overworld;
}

struct Actor {
    int slot;
    float x, z;
    uint8_t image;
};

// This predicate identifies the lower dialogue whose live map/sprites remain
// usable. Other menus use menu_layout and the previously retained world frame.
inline bool bottom_dialogue(const GBContext* ctx) {
    if(!ctx || !ctx->wram || !ctx->io || read(ctx,Battle) || ctx->io[0x47]!=0xe4)return false;
    auto tile=[&](int x,int y){return read(ctx,uint16_t(0xc3a0+y*20+x));};
    for(int y=0;y<12;y++)for(int x=0;x<20;x++)if(tile(x,y)>=0x60)return false;
    if(tile(0,12)!=0x79||tile(19,12)!=0x7b||tile(0,17)!=0x7d||tile(19,17)!=0x7e)return false;
    for(int x=1;x<19;x++)if(tile(x,12)!=0x7a||tile(x,17)!=0x7a)return false;
    for(int y=13;y<17;y++)if(tile(0,y)!=0x7c||tile(19,y)!=0x7c)return false;
    return true;
}

inline std::array<float, 2> player(const GBContext* ctx) {
    // _AdvancePlayerSprite changes map coordinates only at the end of a step.
    int remaining = read(ctx, Walk);
    float fraction = remaining > 0 && remaining <= 8 ? (8 - remaining) / 8.f : 0.f;
    return {read(ctx, X) + .5f + int8_t(read(ctx, Sprite1 + 5)) * fraction,
            read(ctx, Y) + .5f + int8_t(read(ctx, Sprite1 + 3)) * fraction};
}

inline bool actor(const GBContext* ctx, int slot, Actor& out) {
    uint16_t a = Sprite1 + slot * 16, b = Sprite2 + slot * 16;
    if (!read(ctx, a)) return false;
    // An image index of FF means either offscreen OR hidden by a story event.
    // Respect the same object flags as IsObjectHidden before expanding visibility.
    if (slot > 0 && slot < 15) {
        for (int i=0;i<17;i++) {
            int id=read(ctx,HiddenList+i*2);
            if(id==255)break;
            int flag=read(ctx,HiddenList+i*2+1);
            if(id==slot && (read(ctx,HiddenFlags+flag/8)&(1<<(flag%8))))return false;
        }
    }
    const auto p = player(ctx);
    int image = read(ctx, a + 2);
    float x, z;
    if (slot == 0) { x = p[0]; z = p[1]; }
    else if (image != 255) {
        // Screen positions include the engine's walking and scrolling offsets.
        x = p[0] + int8_t(read(ctx, a + 6) - read(ctx, Sprite1 + 6)) / 16.f;
        z = p[1] + int8_t(read(ctx, a + 4) - read(ctx, Sprite1 + 4)) / 16.f;
    } else {
        // Offscreen NPCs still have map coordinates, but no rendered image index.
        x = int(read(ctx, b + 5)) - 4 + .5f;
        z = int(read(ctx, b + 4)) - 4 + .5f;
        int base = read(ctx, b + 14);
        if (slot == 15 || base < 1 || base > 11) return false;
        image = ((base - 1) << 4) | (read(ctx, a + 9) & 12);
    }
    const Scene* s=scene(read(ctx,Map));
    if (!s || x < 0 || z < 0 || x >= s->width || z >= s->height) return false;
    out = {slot, x, z, uint8_t(image)};
    return true;
}

inline std::array<float,2> world_player(const GBContext* ctx) {
    auto p=player(ctx);
    if(const auto* s=scene(read(ctx,Map))) { p[0]+=s->origin_x; p[1]+=s->origin_z; }
    return p;
}
} // namespace pallet
