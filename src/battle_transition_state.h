#pragma once
#include "battle_state.h"

// UE Yellow: engine/battle/init_battle.asm and battle_transitions.asm.
// This is a read-only phase probe. A trainer's IsInBattle is still zero while
// its wipe runs, so the positive lifetime is the caller of the transition,
// not that flag, a timer, or leftover tile IDs.
namespace battle_transition {
enum class Phase { None, Preparing, Flash, Wipe, Loading, Introduction, Battle, Exit };
struct Sample {Phase phase=Phase::None;float wipe=0;bool hud=false;};
inline bool supported(const GBContext* ctx) {
    return ctx&&ctx->rom&&ctx->rom_size==1048576&&ctx->wram&&ctx->vram&&ctx->io&&
        !battle::read(ctx,battle::BattleType)&&!battle::read(ctx,battle::Link);
}
inline bool far_call(const GBContext* ctx,size_t call,int target,int bank) {
    // Verify LD HL,target; LD B,bank; CALL Bankswitch together. A generic
    // Bankswitch return by itself cannot identify the callee or its bank.
    return supported(ctx)&&call>=5&&call+2<ctx->rom_size&&
        ctx->rom[call-5]==0x21&&ctx->rom[call-4]==(target&255)&&ctx->rom[call-3]==(target>>8)&&
        ctx->rom[call-2]==0x06&&ctx->rom[call-1]==bank&&battle::live_return(ctx,call,0x3e84);
}
inline bool entry(const GBContext* ctx) {
    return far_call(ctx,0xf6046,0x6db8,0x0f)||far_call(ctx,0xf608e,0x6db8,0x0f);
}
inline bool introduction(const GBContext* ctx) {
    return battle::normal(ctx)&&far_call(ctx,0xf60f5,0x404c,0x0f);
}
inline bool loop(const GBContext* ctx) {
    return battle::normal(ctx)&&far_call(ctx,0xf613f,0x4127,0x0f);
}
inline bool ending(const GBContext* ctx) {
    return far_call(ctx,0xf6147,0x7765,0x04);
}
inline bool visible_hud(const GBContext* ctx,const uint32_t* lcd) {
    if(!lcd||!introduction(ctx)||!(ctx->io[0x40]&0x80)||ctx->io[0x47]!=0xe4)return false;
    // The first E4 write precedes the first visible LCD by one frame. The
    // original message box is visible when its lower region has contrast;
    // before that the LCD is still uniformly black. Checking the rendered
    // pixels prevents presenting the arena one frame before the original HUD.
    const uint32_t first=lcd[96*160]&0xffffff;
    for(int y=96;y<144;y++)for(int x=0;x<160;x++)
        if((lcd[y*160+x]&0xffffff)!=first)return true;
    return false;
}
inline Sample sample(const GBContext* ctx,const uint32_t* lcd=nullptr) {
    if(!supported(ctx))return {};
    if(entry(ctx)) {
        bool transition=battle::live_return(ctx,0x3edd8,0x3eb4);
        if(transition&&battle::live_return(ctx,0x70d75,0x4bd0))return {Phase::Flash};
        if(ctx->io[0x47]==255)return {Phase::Loading};
        // These are the real yielding calls for all eight table entries:
        // inward/outward spiral, shrink/split, stripes and both circles.
        constexpr std::array<std::array<int,2>,8> calls{{
            {0x70aeb,0x4b1d},{0x70b0c,0x1e64},{0x70c31,0x372f},{0x70c76,0x4d8f},
            {0x70d0a,0x4d8f},{0x70d40,0x4d8f},{0x70d87,0x4d8f},{0x70dbc,0x4d8f}}};
        if(transition)for(auto call:calls)if(battle::live_return(ctx,call[0],call[1])) {
            int filled=0;
            for(int i=0;i<360;i++)filled+=battle::read(ctx,battle::TileMap+i)==255;
            return {Phase::Wipe,float(filled)/360.f};
        }
        return {Phase::Preparing};
    }
    if(introduction(ctx))return {Phase::Introduction,0,visible_hud(ctx,lcd)};
    if(loop(ctx))return {Phase::Battle};
    if(ending(ctx))return {Phase::Exit};
    return {};
}
} // namespace battle_transition
