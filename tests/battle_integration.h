#pragma once

inline bool battle_menu_visible(const GBContext* ctx) {
    const char* word="FIGHT";
    for(int i=0;i<5;i++)if(battle::tile(ctx,10+i,14)!=word[i]-'A'+0x80)return false;
    return battle::normal(ctx)&&battle::ready(ctx)&&!battle::animation_running(ctx);
}

// Obtain a second party member for swap/defeat regression through the actual
// capture rules. The input fixture comes from town's real shop purchase.
inline int battle_capture(GBContext* ctx) {
    QaWalk run{ctx};run.wait(30);
    run.require(run.read(pallet::Map)==12&&run.read(pallet::X)==10&&run.read(pallet::Y)==4,"Route 1 town fixture");
    int party=run.read(0xd162),arena_frames=0,capture_frames=0;
    bool trajectory=false,shake=false;
    auto capture=[&](const char* name) {
        std::string path=std::string("logs/")+name;capture_surface((path+".ppm").c_str());
        run.require(gb_context_save_state_file(ctx,(path+".state").c_str()),"save private capture fixture");
    };
    auto tick=[&]() {
        run.tick();auto info=pallet3d_battle();arena_frames+=info.active;
        if(info.active&&info.capture&&!info.full_overlay) {
            ++capture_frames;
            int id=run.read(battle::Animation);
            if(!trajectory&&id==0xc1&&run.read(battle::AnimCounter)<=7&&run.read(battle::AnimCounter)>=4){capture("capture-trajectory");trajectory=true;}
            if(!shake&&id==0xc2&&run.read(battle::AnimCounter)==3){capture("capture-shake");shake=true;}
        }
    };
    run.move(12,14,4,"R");run.input("D");bool south=true;
    for(int i=0;i<4000&&!run.read(pallet::Battle);i++) {
        if(south&&run.read(pallet::Y)>=12&&!run.read(pallet::Walk)){south=false;run.input("U");}
        if(!south&&run.read(pallet::Y)<=4&&!run.read(pallet::Walk)){south=true;run.input("D");}
        tick();
    }
    run.input(nullptr);run.require(run.read(pallet::Battle)==1,"real wild encounter");
    for(int i=0;i<2000&&!battle_menu_visible(ctx);i++) {
        if(i%20==0)run.input("A");if(i%20==6)run.input(nullptr);tick();
    }
    run.input(nullptr);run.wait(15);capture("capture-start");
    int throws=0;
    for(;throws<10&&run.read(0xd162)==party;throws++) {
        run.require(battle_menu_visible(ctx),"original fight menu before throw");
        int slot=-1;
        for(int i=0;i<run.read(0xd31c)&&i<20;i++)if(run.read(0xd31d+i*2)==4&&run.read(0xd31e + i*2))slot=i;
        run.require(slot>=0,"bought Poke Balls remain");
        run.press("L");run.press("D");run.press("A");run.wait(20);
        // Item menus remember their cursor; normalize it using the real list.
        for(int i=0;i<25&&(run.read(0xcc26)||run.read(0xcc36));i++)run.press("U");
        for(int i=0;i<slot;i++)run.press("D");
        capture("capture-bag");run.press("A");
        bool saved=false;
        for(int i=0;i<4000;i++) {
            if(i%20==0)run.input(run.read(0xd162)>party?"B":"A");
            if(i%20==6)run.input(nullptr);tick();
            if(i==180&&!saved){capture("capture-throw");saved=true;}
            if(i>120&&(battle_menu_visible(ctx)||!run.read(pallet::Battle)))break;
        }
        run.input(nullptr);
        std::fprintf(stderr,"[CAPTURE] throw=%d party=%d battle=%d view=%d\n",throws+1,run.read(0xd162),run.read(pallet::Battle),int(pallet::view(ctx)));
    }
    for(int i=0;i<4000&&(run.read(pallet::Battle)||pallet::view(ctx)!=pallet::View::Overworld);i++) {
        if(i%20==0)run.input("B");if(i%20==6)run.input(nullptr);tick();
    }
    run.input(nullptr);run.wait(40);capture("capture-complete");
    run.require(run.read(0xd162)==party+1&&!run.read(pallet::Battle)&&pallet3d_active(),"real capture adds Pokemon and restores world");
    run.require(arena_frames>30,"capture encounter uses 3D battle scene");
    run.require(capture_frames>20&&trajectory&&shake,"3D ball trajectory and engine-driven shake are visible");
    std::fprintf(stderr,"PASS: wild capture after %d throws, %d scene frames, %d ball frames, unchanged WRAM/VRAM/framebuffer and no GL errors\n",throws,arena_frames,capture_frames);
    return 0;
}

inline int battle_menus(GBContext* ctx) {
    QaWalk run{ctx};
    run.require(run.read(pallet::Map)==12&&run.read(pallet::X)==10&&run.read(pallet::Y)==28,"Route 1 fixture");
    bool south=true;run.input("D");
    for(int i=0;i<4000&&!run.read(pallet::Battle);i++) {
        if(south&&run.read(pallet::Y)>=32&&!run.read(pallet::Walk)){south=false;run.input("U");}
        if(!south&&run.read(pallet::Y)<=28&&!run.read(pallet::Walk)){south=true;run.input("D");}
        run.tick();
    }
    run.require(run.read(pallet::Battle),"encounter through real movement");run.input(nullptr);
    for(int i=0;i<2500&&!battle_menu_visible(ctx);i++) {
        if(i%35==0)run.input("A");if(i%35==6)run.input(nullptr);run.tick();
    }
    run.input(nullptr);run.wait(12);
    auto menu=[&](){run.require(battle_menu_visible(ctx),"original FIGHT menu ready");};
    auto capture=[&](const char* name) {
        std::string path=std::string("logs/")+name;capture_surface((path+".ppm").c_str());
        run.require(gb_context_save_state_file(ctx,(path+".state").c_str()),"private menu fixture");
        std::fprintf(stderr,"[BATTLE-MENU] %s cursor=%d x=%d y=%d scene=%d overlay=%d\n",name,run.read(0xcc26),run.read(0xcc25),run.read(0xcc24),pallet3d_battle().active,pallet3d_battle().full_overlay);
    };
    menu();run.require(pallet3d_battle().active&&!pallet3d_battle().full_overlay,"arena with original fight menu");capture("battle-fight");
    run.press("A");run.wait(35);run.require(!battle_menu_visible(ctx),"move selection opened");capture("battle-moves");
    run.press("B");run.wait(20);menu();
    run.press("D");run.press("A");run.wait(35);
    run.require(!battle_menu_visible(ctx),"item list opened");capture("battle-bag");
    run.press("B");run.wait(35);menu();
    run.press("U");run.press("R");run.press("A");run.wait(35);
    run.require(!battle_menu_visible(ctx),"party list opened");capture("battle-party");
    run.press("B");run.wait(35);menu();
    run.press("D");run.press("A");
    for(int i=0;i<1800&&run.read(pallet::Battle);i++) {
        if(i%40==0)run.input("A");if(i%40==6)run.input(nullptr);run.tick();
    }
    run.input(nullptr);run.wait(50);
    run.require(!run.read(pallet::Battle)&&pallet::view(ctx)==pallet::View::Overworld&&pallet3d_active(),"run escapes and restores Route 1 3D");
    capture("battle-escaped");
    std::puts("PASS: fight, move selection, bag, party, run and return to 3D; original controls, GL and memory checked every frame");return 0;
}

// Private battle fixtures are obtained through normal movement and buttons.
// This diagnostic also records the tile rectangles before adding a renderer.
inline int battle_probe(GBContext* ctx) {
    QaWalk run{ctx};
    run.require(run.read(pallet::Map)==12&&run.read(pallet::X)==10&&run.read(pallet::Y)==28,"Route 1 (10,28) fixture");
    FILE* trace=std::fopen("logs/battle-probe.csv","w");run.require(trace,"trace file");
    std::fprintf(trace,"frame,battle,bgp,scx,scy,first,enemy,player,enemyhp,playerhp,anim,delay,counter,pc,bank,enemytiles,playertiles\n");
    int battle_frames=0,scene_frames=0,arena_frames=0;bool started=false;int serial=0;bool south=true;run.input("D");
    for(int i=0;i<8000;i++) {
        if(!started) {
            if(south&&run.read(pallet::Y)>=32&&!run.read(pallet::Walk)){south=false;run.input("U");}
            if(!south&&run.read(pallet::Y)<=28&&!run.read(pallet::Walk)){south=true;run.input("D");}
        } else {
            if(battle_frames%35==0)run.input("A");
            if(battle_frames%35==6)run.input(nullptr);
        }
        run.tick();
        if(run.read(pallet::Battle)) {
            if(!started){started=true;run.input(nullptr);}
            ++battle_frames;
            if(pallet::view(ctx)==pallet::View::Battle)++scene_frames;
            if(pallet3d_battle().active&&!pallet3d_battle().full_overlay)++arena_frames;
            int enemy=0,player=0;
            for(int y=0;y<7;y++)for(int x=0;x<7;x++) {
                enemy+=run.read(0xc3a0+y*20+x+12)==x*7+y;
                player+=run.read(0xc3a0+(y+5)*20+x+1)==0x31+x*7+y;
            }
            std::fprintf(trace,"%d,%d,%02x,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%04x,%d,%d,%d\n",battle_frames,run.read(pallet::Battle),ctx->io[0x47],ctx->io[0x43],ctx->io[0x42],run.read(0xd11c),run.read(0xcfe4),run.read(0xd013),run.read(0xcfe5)*256+run.read(0xcfe6),run.read(0xd014)*256+run.read(0xd015),run.read(0xd07b),run.read(0xd085),run.read(0xd086),ctx->pc,ctx->rom_bank,enemy,player);
            if(enemy==49&&player==49&&battle_frames%70==0&&serial<12) {
                auto name=std::string("logs/battle-probe-")+std::to_string(++serial);
                run.require(gb_context_save_state_file(ctx,(name+".state").c_str()),"save private battle fixture");
                capture_surface((name+".ppm").c_str());
            }
        } else if(started&&pallet::view(ctx)==pallet::View::Overworld)break;
    }
    std::fclose(trace);run.input(nullptr);
    run.require(started&&battle_frames>100&&!run.read(pallet::Battle),"actual encounter completed");
    std::fprintf(stderr,"[BATTLE] scene_frames=%d arena_frames=%d\n",scene_frames,arena_frames);
    run.require(scene_frames>100&&arena_frames>100,"real encounter presents its arena and portraits");
    std::puts("PASS: real battle probe, GL and read-only WRAM/VRAM/framebuffer");return 0;
}
