#pragma once
#include "battle_transition_state.h"

// Private, read-only baseline for C2. The normal encounter drivers still own
// all inputs and assertions. Record the actual LCD, not the renderer's guess
// at when a battle has become ready, and preserve the live stack for mapping
// each phase to a ROM-validated CALL after the run.
namespace battle_transition_trace {
inline FILE* trace=nullptr;
inline FILE* lcd=nullptr;
inline int battle_frame=0;
inline bool started=false;
inline void observe(GBContext* ctx,int frame) {
    const int flag=pallet::read(ctx,pallet::Battle);
    // Trainer battles do not set IsInBattle until AFTER their wipe. The
    // transition predef's validated caller captures that earlier lifetime.
    if(flag==1||flag==2||battle_transition::entry(ctx))started=true;
    if(!started)return;
    ++battle_frame;
    const bool window=(ctx->io[0x40]&0x20)&&ctx->io[0x4a]==0&&ctx->io[0x4b]==7;
    const int map=(ctx->io[0x40]&(window?0x40:8))?0x1c00:0x1800;
    int black_tiles=0;
    for(int i=0;i<360;i++)black_tiles+=ctx->wram[battle::TileMap-0xc000+i]==255;
    const auto phase=battle_transition::sample(ctx,gb_get_framebuffer(ctx));
    std::fprintf(trace,"%d,%d,%u,%d,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%u,%u,%d,%.6f,%d,",
        frame,battle_frame,ctx->cycles,flag,ctx->io[0x47],ctx->io[0x40],ctx->io[0x43],ctx->io[0x42],ctx->io[0x4a],ctx->io[0x4b],
        battle::read(ctx,0xd11c),battle::read(ctx,0xcfe7),black_tiles,battle::rectangle(ctx,battle::Enemy,true),
        battle::rectangle(ctx,battle::Player,true),battle::ready(ctx),pallet3d_covers_frame(ctx),pallet3d_battle().active,ctx->sp,ctx->rom_bank,
        int(phase.phase),phase.wipe,phase.hud);
    if(ctx->sp>=0xd000&&ctx->sp<0xe000)
        for(int a=ctx->sp;a<0xdfff;a+=2)std::fprintf(trace,"%04x ",battle::read(ctx,a)|(battle::read(ctx,a+1)<<8));
    std::fputc(',',trace);
    for(int y=0;y<18;y++)for(int x=0;x<20;x++)std::fprintf(trace,"%02x",ctx->vram[map+y*32+x]);
    std::fputc('\n',trace);
    if(battle_frame<=1000) {
        const auto* framebuffer=gb_get_framebuffer(ctx);
        uint8_t rgb[160*144*3];
        for(int i=0;i<160*144;i++) {
            rgb[i*3]=uint8_t(framebuffer[i]>>16);
            rgb[i*3+1]=uint8_t(framebuffer[i]>>8);
            rgb[i*3+2]=uint8_t(framebuffer[i]);
        }
        if(std::fwrite(rgb,1,sizeof(rgb),lcd)!=sizeof(rgb))std::exit(42);
        if(battle_frame%12==1) {
            char path[96];std::snprintf(path,sizeof(path),"logs/lcd-%04d.ppm",battle_frame);
            FILE* image=std::fopen(path,"wb");if(!image)std::exit(42);
            std::fputs("P6\n160 144\n255\n",image);std::fwrite(rgb,1,sizeof(rgb),image);std::fclose(image);
        }
    }
}
inline int run(GBContext* ctx,bool trainer) {
    trace=std::fopen("logs/battle-timing.csv","w");
    lcd=popen("ffmpeg -v error -y -f rawvideo -pixel_format rgb24 -video_size 160x144 -framerate 60 -i - -an -c:v libx264 -preset veryfast -crf 0 logs/original-intro.mp4","w");
    if(!trace||!lcd)return 42;
    std::fputs("frame,battle_frame,cycles,battle,bgp,lcdc,scx,scy,wy,wx,first,enemy_party_pos,black_tiles,enemy_rect,player_rect,ready,covered,arena,sp,bank,phase,wipe,hud,stack,displayed_tiles\n",trace);
    battle_frame=0;started=false;qa_frame_observer=observe;
    int result=trainer?battle_trainer(ctx):battle_menus(ctx);
    qa_frame_observer=nullptr;std::fclose(trace);trace=nullptr;
    if(pclose(lcd)!=0)return 42;
    lcd=nullptr;return result;
}
} // namespace battle_transition_trace
