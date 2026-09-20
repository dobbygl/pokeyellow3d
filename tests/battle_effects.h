#pragma once
// Controlled QA fixtures only: grant four moves on a disposable battle state.
// Actual menu input and the original engine execute each attack. Production
// renderers only observe the resulting animation, HP and VRAM.
inline int battle_effects(GBContext* ctx,const char* original) {
    constexpr int moves[]={33,84,45,97};
    const char* names[]={"physical","projectile","status","self","original"};
    for(int test=0;test<5;test++) {
        if(!gb_context_load_state_file(ctx,original))return 5;
        for(int i=0;i<4;i++) {
            ctx->wram[0x101b+i]=uint8_t(moves[i]);ctx->wram[0x102c+i]=30;
            ctx->wram[0x1172+i]=uint8_t(moves[i]);ctx->wram[0x1187+i]=30;
        }
        if(test==4){ctx->wram[0x101b]=19;ctx->wram[0x1172]=19;ctx->wram[0x102c]=15;ctx->wram[0x1187]=15;} // Fly retains its two-turn animation.
        ctx->wram[0xc2e]=0; // Initial remembered move cursor in the private fixture.
        QaWalk run{ctx};run.wait(30);run.require(battle_menu_visible(ctx),"effect fixture at FIGHT menu");
        run.require(pallet3d_battle().terrain==1,"loading Route 1 battle restores grass rather than the previous scene terrain");
        run.press("U");run.press("L");run.press("A");run.wait(25);
        for(int i=0;i<(test==4?0:test);i++)run.press("D");
        run.press("A");bool captured=false,started=false,damage_captured=false;int visible=0,damage_frames=0;
        std::string trace_path=std::string("logs/effect-")+names[test]+".csv";
        FILE* trace=std::fopen(trace_path.c_str(),"w");run.require(trace,"effect trace");
        std::fputs("frame,animation,running,category,actor,overlay,alpha,time,view,enemyrect,playerrect\n",trace);
        for(int i=0;i<2500;i++) {
            if(i%20==0)run.input("A");if(i%20==6)run.input(nullptr);run.tick();
            auto info=pallet3d_battle();
            if(info.active&&!info.full_overlay&&info.enemy_damage>0) {
                ++damage_frames;
                auto enemy=battle::mon(ctx,true);
                run.require(std::isfinite(info.enemy_hp)&&info.enemy_hp>=enemy.hp&&info.enemy_hp<=enemy.max_hp,
                    "damage HP interpolation stays between live HP and maximum");
                if(!damage_captured) {
                    std::string path=std::string("logs/effect-")+names[test]+"-damage.ppm";
                    capture_surface(path.c_str());damage_captured=true;
                }
            }
            std::fprintf(trace,"%d,%d,%d,%d,%d,%d,%.3f,%.3f,%d,%d,%d\n",i,run.read(battle::Animation),battle::animation_running(ctx),info.effect,info.effect_actor,info.full_overlay,info.overlay_alpha,info.effect_time,int(pallet::view(ctx)),battle::rectangle(ctx,battle::Enemy,true),battle::rectangle(ctx,battle::Player,true));
            bool presentation=test==4?(info.full_overlay&&info.overlay_alpha==1&&battle::animation_running(ctx)&&run.read(battle::Animation)==19):(!info.full_overlay&&info.effect==test+1);
            if(info.active&&presentation&&info.effect_actor==0) {
                ++visible;started=true;
                if(!captured&&info.effect_time>.16f) {
                    std::string path=std::string("logs/effect-")+names[test];capture_surface((path+".ppm").c_str());
                    run.require(gb_context_save_state_file(ctx,(path+".state").c_str()),"private effect fixture");captured=true;
                    if(test==4) {
                        GLint viewport[4];glGetIntegerv(GL_VIEWPORT,viewport);
                        int w=viewport[2],h=viewport[3],scale=std::max(1,std::min(w/160,h/144));
                        std::vector<uint8_t> rgba(size_t(w)*h*4);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
                        const auto* lcd=gb_get_framebuffer(ctx);int differences=0;
                        for(int y=0;y<144;y++)for(int x=0;x<160;x++) {
                            int xx=(w-160*scale)/2+x*scale+scale/2,yy=(h-144*scale)/2+y*scale+scale/2;
                            auto* p=&rgba[((h-1-yy)*w+xx)*4];uint32_t c=lcd[y*160+x];
                            differences+=(p[0]!=(c>>16&255)||p[1]!=(c>>8&255)||p[2]!=(c&255));
                        }
                        std::fprintf(stderr,"[EFFECT] full LCD pixel differences=%d\n",differences);
                        run.require(!differences,"original animation composite equals all 23040 LCD pixels");
                    }
                }
            }
            if(started&&!battle::animation_running(ctx)&&(battle_menu_visible(ctx)||!run.read(pallet::Battle)))break;
        }
        run.input(nullptr);
        std::fclose(trace);
        std::fprintf(stderr,"[EFFECT] %s frames=%d captured=%d\n",names[test],visible,captured);
        if(test<2) {
            std::fprintf(stderr,"[EFFECT] %s damage feedback frames=%d\n",names[test],damage_frames);
            run.require(damage_frames>0,"real damaging attack produces visible shake/blink feedback and bounded HP bar");
        }
        run.require(captured&&visible>5,"3D effect visible during actual engine animation");
        run.require(!battle::animation_running(ctx),"animation completes before advancing to next fixture");
    }
    std::puts("PASS: four ROM-classified effect categories and exact full-LCD fallback, original attacks, GL and read-only rendering");return 0;
}
