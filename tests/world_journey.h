#pragma once
#include <array>
#include <cmath>
#include <string>
#include "firstperson.h"
#include "read_only_memory.h"

// A real-engine journey. Inputs, wild encounters and battle results are produced
// by the ROM; this harness never patches map, party, RNG or collision memory.
inline int world_journey(GBContext* ctx,bool viridian=false) {
    auto byte=[&](int addr){return pallet::read(ctx,uint16_t(addr));};
    auto require=[](bool ok,const char* message) {
        if(!ok) {std::fprintf(stderr,"[JOURNEY] FAIL: %s\n",message);std::exit(30);}
    };
    require(byte(pallet::Map)==0 && byte(pallet::X)==9 && byte(pallet::Y)==7 && byte(0xd162)>0,
        "Fixture must start in Pallet (9,7) with a battle-ready party");
    int frame=0, crossings=0, victories=0, battle_frames=0, overworld_frames=0;
    auto input=[&](const char* button) {
        if(!button)gb_platform_set_input_script(nullptr);
        else {
            std::string script=std::to_string(frame+1)+":"+button+":20000";
            gb_platform_set_input_script(script.c_str());
        }
        require(gb_platform_poll_events(ctx),"SDL input available");
    };
    bool previous_3d=false;int fp_frames=0,jump_frames=0,battle_3d=0;
    auto tick=[&]() {
        auto prev=pallet::world_player(ctx);int prev_map=byte(pallet::Map);
        gb_reset_frame(ctx);ctx->stopped=0;
        unsigned slices=0;
        while(!ctx->frame_done) {
            gb_run_cycles(ctx,70224);
            require(gb_platform_poll_events(ctx)&&++slices<1000,"Engine completes frame");
        }
        auto state=pallet::view(ctx);
        if(prev_map!=byte(pallet::Map)) {
            auto now=pallet::world_player(ctx);
            require(pallet::scene(prev_map)&&pallet::scene(byte(pallet::Map)),"Journey stays in supported maps");
            require(std::abs(prev[0]-now[0])+std::abs(prev[1]-now[1])<1.5f,"No coordinate jump at map seam");
            ++crossings;
        }
        if(byte(pallet::Battle)) {
            ++battle_frames;
            require(state!=pallet::View::Overworld,"Combat never draws an overworld map");
        }
        if(state==pallet::View::Overworld)++overworld_frames;
        ReadOnlyMemory before{ctx};
        gb_platform_render_frame(gb_get_framebuffer(ctx));++frame;
        require(glGetError()==GL_NO_ERROR,"No OpenGL error");
        require(pallet3d_stats().resident_maps<=5,"Mesh cache bounded to current plus four neighbors");
        require(before.unchanged(ctx),"Renderer leaves WRAM, VRAM and framebuffer unchanged");
        require(pallet3d_active()==(state==pallet::View::Overworld||state==pallet::View::Battle),"Real renderer follows scene selection");
        if(state==pallet::View::Battle)++battle_3d;
        if(pallet3d_firstperson() && state==pallet::View::Overworld) {
            auto camera=pallet3d_camera();auto position=pallet::world_player(ctx);++fp_frames;
            require(std::abs(camera.x-position[0])<.0001f&&std::abs(camera.z-position[1])<.0001f,"FP camera follows interpolated engine position exactly");
            require(!camera.player_drawn,"FP never draws the player or its x-ray");
            require(camera.y>=1.099f&&camera.y<=1.241f,"Ledge bob stays within its visual height range");
            if(camera.y>1.101f)++jump_frames;
            if(!previous_3d)require(std::abs(firstperson::angle_delta(camera.yaw,firstperson::facing_yaw(byte(0xc109))))<.0001f,"Return from 2D restores engine-facing camera immediately");
        }
        if(state!=pallet::View::Overworld)require(pallet3d_input_mask()==255,"Relative dpad is neutral outside overworld");
        previous_3d=pallet3d_active()&&state==pallet::View::Overworld;
    };
    auto experience=[&](){return (byte(0xd178)<<16)|(byte(0xd179)<<8)|byte(0xd17a);};
    auto fight=[&]() {
        input(nullptr);
        int before_exp=experience();
        for(int i=0;i<5000;i++) {
            if(i%35==0)input("A");
            if(i%35==6)input(nullptr);
            tick();
            if(!byte(pallet::Battle)&&pallet::view(ctx)==pallet::View::Overworld) {
                input(nullptr);
                require(experience()>before_exp,"Win an actual battle and gain experience");
                ++victories;
                std::fprintf(stderr,"[JOURNEY] victory=%d frame=%d exp=%d map=%u xy=%u,%u\n",victories,frame,experience(),byte(pallet::Map),byte(pallet::X),byte(pallet::Y));
                return;
            }
        }
        require(false,"Battle finishes within frame budget");
    };
    auto move=[&](int map,int x,int y,const char* direction) {
        input(direction);
        for(int i=0;i<1200;i++) {
            tick();
            if(byte(pallet::Battle)) {fight();input(direction);}
            if(byte(pallet::Map)==map && byte(pallet::X)==x && byte(pallet::Y)==y && !byte(pallet::Walk)) {
                input(nullptr);
                std::fprintf(stderr,"[JOURNEY] waypoint map=%d xy=%d,%d frame=%d\n",map,x,y,frame);
                return;
            }
        }
        std::fprintf(stderr,"[JOURNEY] stuck map=%u xy=%u,%u target=%d,%d,%d\n",byte(pallet::Map),byte(pallet::X),byte(pallet::Y),map,x,y);
        require(false,"Reach walking waypoint");
    };
    move(0,9,2,"U");move(0,10,2,"R");move(12,10,28,"U");
    input("U");for(int i=0;i<40;i++)tick();input(nullptr);
    require(byte(pallet::Y)==28,"Cannot walk north through a one-way ledge");
    // Walk around the ledge, then let the original engine jump south over it.
    move(12,8,28,"L");move(12,8,26,"U");move(12,10,26,"R");move(12,10,28,"D");
    for(int i=0;victories==0 && i<10;i++) {
        move(12,10,32,"D");move(12,10,28,"U");
    }
    require(victories>0 && battle_3d>100,"Encounter and defeat a wild Pokemon with the 3D scene active");
    if(viridian) {
        move(12,8,28,"L");move(12,8,24,"U");move(12,12,24,"R");
        move(12,12,22,"U");move(12,9,22,"L");move(12,9,14,"U");
        move(12,14,14,"R");move(12,14,4,"U");move(12,10,4,"L");
        move(1,20,33,"U");
        input(nullptr);for(int i=0;i<35;i++)tick();
        if(const char* out=std::getenv("JOURNEY_CITY_STATE"))
            require(gb_context_save_state_file(ctx,out),"Save isolated Viridian QA fixture");
        move(12,10,4,"D");move(12,14,4,"R");move(12,14,14,"D");
        move(12,9,14,"L");move(12,9,22,"D");move(12,12,22,"R");
        move(12,12,24,"D");move(12,8,24,"L");move(12,8,28,"D");move(12,10,28,"R");
    }
    move(0,10,2,"D");
    input(nullptr);for(int i=0;i<35;i++)tick();
    auto builds=pallet3d_stats().mesh_builds;
    for(int i=0;i<35;i++)tick();
    require(pallet3d_stats().mesh_builds==builds,"Static geometry remains cached while idle");
    require(crossings==(viridian?4:2) && overworld_frames>200,"Cross both ways and render the connected overworld");
    if(viridian)std::fprintf(stderr,"PASS: Viridian round trip, four seam crossings including horizontal displacement\n");
    std::fprintf(stderr,"PASS: Pallet -> Route 1, ledge blocks north/jumps south, %d real wild victories, 3D combat -> overworld, Route 1 -> Pallet; frames=%d 3D=%d battle=%d battle3d=%d, GL and memory checks every frame\n",victories,frame,overworld_frames,battle_frames,battle_3d);
    if(pallet3d_firstperson()) {require(fp_frames>200&&jump_frames>0,"First-person journey includes a real ledge bob");std::fprintf(stderr,"[FP] frames=%d jump_frames=%d\n",fp_frames,jump_frames);}
    uint64_t digest=1469598103934665603ull;
    for(int i=0;i<8192;i++)digest=(digest^ctx->wram[i])*1099511628211ull;
    std::fprintf(stderr,"[JOURNEY] final_wram=%016llx\n",(unsigned long long)digest);
    return 0;
}
