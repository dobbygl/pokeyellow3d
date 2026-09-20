#pragma once
#include "battle_state.h"
#include "fade_state.h"
#include "interior_scene.h"

namespace pc_state {
enum class Mode {None,Center,Items,Bill,Oak,Hall};
struct Terminal {int map=-1,x=0,z=0;float height=0;bool bedroom=false;};
struct Sample {Mode mode=Mode::None;Terminal terminal;bool main=false;};
inline Terminal terminal(const GBContext* ctx) {
    if(!ctx||!ctx->wram||!ctx->rom||ctx->rom_size!=1048576||battle::read(ctx,0xc109)!=4)return {};
    auto* scene=pallet::scene(battle::read(ctx,0xd35d));
    if(!scene||!scene->interior)return {};
    int x=battle::read(ctx,0xd361),z=int(battle::read(ctx,0xd360))-1;
    if(x<0||z<0||x>=scene->width||z>=scene->height)return {};
    int tile=pallet::map_tile(ctx->rom,*scene,x*2,z*2);
    bool bedroom=(scene->tileset==1||scene->tileset==4)&&(tile==0x40||tile==0x42);
    bool center=(scene->tileset==2||scene->tileset==6)&&(tile==0x20||tile==0x21);
    auto cell=interior::classify(ctx->rom,*scene,x,z);
    if((!bedroom&&!center)||cell.kind!=interior::Kind::Furniture)return {};
    return {scene->id,x,z,cell.height,bedroom};
}
inline bool far_call(const GBContext* ctx,size_t call,int bank,int address) {
    // PC macros use LD B,bank; LD HL,address; CALL Bankswitch.
    return call>=5&&call+2<ctx->rom_size&&ctx->rom[call-5]==6&&ctx->rom[call-4]==bank&&
        ctx->rom[call-3]==0x21&&ctx->rom[call-2]==(address&255)&&ctx->rom[call-1]==(address>>8)&&
        battle::live_return(ctx,call,0x3e84);
}
inline Sample sample(const GBContext* ctx) {
    if(!ctx||!ctx->wram||!ctx->rom||!ctx->io||ctx->rom_size!=1048576||battle::read(ctx,battle::IsInBattle))return {};
    // The shared home dispatcher is also used by vending machines. A verified
    // adjacent PC in its own tileset is required, not merely this stack word.
    if(!fade::home_call(ctx,0x3408,0x3e84))return {};
    auto pc=terminal(ctx);if(pc.map<0)return {};
    Mode mode=pc.bedroom?Mode::Items:Mode::Center;
    if(far_call(ctx,0x17d3f,1,0x778e))mode=Mode::Items;
    else if(far_call(ctx,0x17d51,7,0x62ae))mode=Mode::Oak;
    else if(far_call(ctx,0x17d63,0x1d,0x5dfe))mode=Mode::Hall;
    else if(far_call(ctx,0x17d87,8,0x546f))mode=Mode::Bill;
    bool items_menu=mode==Mode::Items&&battle::read(ctx,0xcc24)==2&&battle::read(ctx,0xcc25)==1&&
        battle::read(ctx,0xcc28)==3&&battle::tile(ctx,0,0)==0x79&&battle::tile(ctx,15,9)==0x7e;
    return {mode,pc,mode==Mode::Center||items_menu};
}
// A 400-ms presentation motion driven by guest time. Pauses cannot consume it,
// and a long stall cannot skip it. The original engine's state never changes.
struct Motion {
    float amount=0;uint32_t last=0;bool initialized=false,paused=false;
    void update(bool wanted,uint32_t now,bool frozen) {
        uint32_t elapsed=initialized?now-last:0;last=now;initialized=true;
        if(frozen){paused=true;return;}
        if(paused){paused=false;return;}
        float step=std::min(.025f,float(elapsed)/4194304.f)/.4f;
        amount=std::clamp(amount+(wanted?step:-step),0.f,1.f);
    }
    float eased() const {return amount*amount*(3-2*amount);}
};
} // namespace pc_state
