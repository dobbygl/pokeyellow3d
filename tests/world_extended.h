#pragma once
#include <chrono>
#include "read_only_memory.h"
#include "fade_state.h"
#include "menu_state.h"
inline void (*qa_frame_observer)(GBContext*,int)=nullptr;

// Controlled integration fixtures, never linked into pokeyellow3d. Granting
// field moves/items makes late-game features testable on a private early save.
// Movement, collisions, menus and all subsequent map changes run in the ROM.
struct QaWalk {
    GBContext* ctx; int frame=0; bool previous_3d=false,render_enabled=true;
    int expected_respawn=-1; // A losing battle can return to a healing location.
    int read(int address) const {return pallet::read(ctx,uint16_t(address));}
    void require(bool condition,const char* message) {
        if(!condition) {
            std::fprintf(stderr,"[EXTENDED] FAIL %s; frame=%d map=%d xy=%d,%d view=%d\n",message,frame,read(pallet::Map),read(pallet::X),read(pallet::Y),int(pallet::view(ctx)));
            std::exit(40);
        }
    }
    void input(const char* button) {
        // Hold until the next explicit input()/release. SDL's frame counter
        // outlives individual helpers and the guest cycle clock wraps at 32
        // bits. Cover its entire domain so neither can expire held controls.
        std::string script=std::string("c0:")+(button?button:"")+":4294967296";
        gb_platform_set_input_script(button?script.c_str():nullptr);
        require(gb_platform_poll_events(ctx),"input polling");
    }
    void tick() {
        int previous=read(pallet::Map);auto old=pallet::world_player(ctx);
        gb_reset_frame(ctx);ctx->stopped=0;unsigned slices=0;
        while(!ctx->frame_done) {
            gb_run_cycles(ctx,70224);
            require(gb_platform_poll_events(ctx)&&++slices<1000,"engine frame");
        }
        if(previous!=read(pallet::Map)) {
            auto current=pallet::world_player(ctx);
            const auto* a=pallet::scene(previous);const auto* b=pallet::scene(read(pallet::Map));
            bool respawn=expected_respawn==read(pallet::Map);
            if(respawn)expected_respawn=-1;
            if(a&&b&&a->component==b->component&&!respawn)
                require(std::abs(old[0]-current[0])+std::abs(old[1]-current[1])<1.5f,"continuous connection origin");
            std::fprintf(stderr,"[EXTENDED] crossing %d -> %d xy=%d,%d delta=%.2f\n",previous,read(pallet::Map),read(pallet::X),read(pallet::Y),std::abs(old[0]-current[0])+std::abs(old[1]-current[1]));
        }
        ReadOnlyMemory before{ctx};auto state=pallet::view(ctx);
        gb_platform_render_frame(gb_get_framebuffer(ctx));++frame;
        if(const char* path=std::getenv("UI_TRACE")) {
            static FILE* trace=nullptr;
            if(!trace) {
                trace=std::fopen(path,"w");require(trace,"UI frame trace");
                std::fputs("frame,cycles,map,x,y,view,bgp,lcdc,font,sprites,battle,covered,active,dialogue,warp,hero_image,sp\n",trace);
            }
            std::fprintf(trace,"%d,%u,%d,%d,%d,%d,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%u\n",frame,ctx->cycles,
                read(pallet::Map),read(pallet::X),read(pallet::Y),int(state),ctx->io[0x47],ctx->io[0x40],
                read(pallet::Font),read(pallet::UpdateSprites),read(pallet::Battle),pallet3d_covers_frame(ctx),pallet3d_active(),pallet3d_dialogue_overlay(),pallet3d_warp_overlay(),read(pallet::Sprite1+2),ctx->sp);
        }
        require(glGetError()==GL_NO_ERROR,"OpenGL");
        require(before.unchanged(ctx),"read-only WRAM, VRAM and framebuffer");
        if(pallet3d_warp_overlay())require((fade::warp(ctx)||state==pallet::View::Transition)&&!read(pallet::Battle),"warp identified by a live ROM-validated CALL");
        require(pallet3d_active()==(render_enabled&&(state==pallet::View::Overworld || state==pallet::View::Battle || pallet3d_warp_overlay() || pallet3d_menu().active || (state==pallet::View::Dialogue&&pallet::bottom_dialogue(ctx)))),"scene selection");
        if(pallet3d_menu().active)require(!read(pallet::Battle)&&pallet3d_world_frame().map==read(pallet::Map)&&
            (menu_state::running(ctx)||state==pallet::View::Dialogue||state==pallet::View::Transition),"menu has a valid retained scene and positive lifetime");
        require(pallet3d_stats().resident_maps<=5,"bounded mesh cache");
        if(pallet3d_firstperson()&&state==pallet::View::Overworld&&pallet3d_active()&&!previous_3d)
            require(std::abs(firstperson::angle_delta(pallet3d_camera().yaw,firstperson::facing_yaw(read(0xc109))))<.0001f,"first frame returning to FP matches engine facing");
        // A composed dialogue keeps the FP camera alive; a battle has its own
        // camera and must count as leaving first person even when it is 3D.
        previous_3d=pallet3d_active()&&state!=pallet::View::Battle;
        if(qa_frame_observer)qa_frame_observer(ctx,frame);
    }
    void wait(int count) {for(int i=0;i<count;i++)tick();}
    void press(const char* button,int count=4) {input(button);wait(count);input(nullptr);wait(30);}
    void move(int map,int x,int y,const char* direction) {
        input(direction);
        for(int i=0;i<1000;i++) {
            tick();require(!read(pallet::Battle),"fixture path avoids encounters");
            if(read(pallet::Map)==map&&read(pallet::X)==x&&read(pallet::Y)==y&&!read(pallet::Walk)) {
                input(nullptr);return;
            }
        }
        require(false,"walking waypoint");
    }
};

inline int extended_prepare(GBContext* ctx,const char* scenario,const char* output) {
    QaWalk run{ctx};auto set=[&](int address,int value){ctx->wram[address-0xc000]=uint8_t(value);};
    if(!std::strcmp(scenario,"cut") || !std::strcmp(scenario,"horizontal")) {
        run.require(run.read(pallet::Map)==1&&run.read(pallet::X)==20&&run.read(pallet::Y)==33,"real Viridian fixture");
        run.move(1,20,30,"U");run.move(1,19,30,"L");run.move(1,19,21,"U");
        if(!std::strcmp(scenario,"horizontal")) {
            run.move(1,8,21,"L");run.move(1,8,20,"U");run.move(1,6,20,"L");
            run.move(1,6,15,"U");run.move(33,39,7,"L");
            run.move(1,6,15,"R");
            std::fprintf(stderr,"PASS: real horizontal Viridian / Route 22 round trip\n");
        } else {
            run.move(1,9,21,"L");run.move(1,9,22,"D");run.press("L");
            set(0xd163,0x99);set(0xd16a,0x99); // Bulbasaur, compatible with Cut.
            set(0xd172,15);set(0xd355,run.read(0xd355)|2); // Cut, Cascade Badge.
        }
    } else if(!std::strcmp(scenario,"surf")) {
        run.require(run.read(pallet::Map)==0&&run.read(pallet::X)==9&&run.read(pallet::Y)==7,"Pallet fixture");
        run.move(0,9,13,"D");run.move(0,6,13,"L");run.press("D");
        set(0xd172,57);set(0xd355,run.read(0xd355)|16); // Surf Pikachu, Soul Badge.
    } else if(!std::strcmp(scenario,"bike")) {
        set(0xd31c,1);set(0xd31d,6);set(0xd31e,1);set(0xd31f,255);
    } else if(!std::strcmp(scenario,"transport")) {
        int mode=run.read(0xd6ff);
        run.require(run.read(pallet::Map)==0&&(mode==1||mode==2),"active transport fixture");
        if(mode==1) {
            run.move(0,9,7,"D");run.move(0,9,3,"U");
            run.require(run.read(0xd6ff)==1,"bike remains active during movement");
        } else {
            run.move(0,6,15,"D");run.require(run.read(0xd6ff)==2,"surf remains active on water");
            run.move(0,6,13,"U");run.require(run.read(0xd6ff)==0,"original engine dismounts at shore");
        }
        std::fprintf(stderr,"PASS: mode %d movement, original sprite, 3D view and read-only RAM\n",mode);
    } else if(!std::strcmp(scenario,"ticket")) {
        set(0xd31c,1);set(0xd31d,0x3f);set(0xd31e,1);set(0xd31f,255);
    } else return 41;
    run.input(nullptr);run.wait(35);
    run.require(gb_context_save_state_file(ctx,output),"save private prepared fixture");
    std::fprintf(stderr,"[EXTENDED] prepared %s map=%d xy=%d,%d\n",scenario,run.read(pallet::Map),run.read(pallet::X),run.read(pallet::Y));
    return 0;
}
