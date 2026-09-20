#pragma once
inline int battle_swap(GBContext* ctx) {
    QaWalk run{ctx};run.wait(30);
    run.require(run.read(pallet::Map)==12&&run.read(pallet::X)==14&&run.read(0xd162)==2,"Route 1 captured-party fixture");
    auto capture=[&](const char* name) {
        std::string path=std::string("logs/")+name;capture_surface((path+".ppm").c_str());
        run.require(gb_context_save_state_file(ctx,(path+".state").c_str()),"private swap fixture");
        std::fprintf(stderr,"[SWAP] %s party_index=%d mon=%d hp=%d enemy=%d hp=%d view=%d\n",name,run.read(0xcc2f),run.read(0xd013),battle::word(ctx,0xd014),run.read(0xcfe4),battle::word(ctx,0xcfe5),int(pallet::view(ctx)));
    };
    int checked=0;
    auto tick=[&]() {
        run.tick();auto info=pallet3d_battle();
        if(info.active&&!info.full_overlay&&!battle::animation_running(ctx)) {
            run.require(info.player_species==run.read(0xd013)&&info.enemy_species==run.read(0xcfe4),"billboard species agrees with live engine");
            run.require(info.player_image==battle::fingerprint(battle::portrait(ctx,battle::Player,info.player_species)),"uploaded player portrait agrees with VRAM");
            run.require(info.enemy_image==battle::fingerprint(battle::portrait(ctx,battle::Enemy,info.enemy_species)),"uploaded enemy portrait agrees with VRAM");++checked;
        }
    };
    auto await_menu=[&]() {
        for(int i=0;i<4000;i++) {
            if(i%20==0)run.input("A");if(i%20==6)run.input(nullptr);tick();
            if(i>100&&battle_menu_visible(ctx)){run.input(nullptr);run.wait(12);return;}
        }
        capture("swap-stuck");run.require(false,"battle returns to command menu");
    };
    run.input("D");bool south=true;
    for(int i=0;i<4000&&!run.read(pallet::Battle);i++) {
        if(south&&run.read(pallet::Y)>=12&&!run.read(pallet::Walk)){south=false;run.input("U");}
        if(!south&&run.read(pallet::Y)<=4&&!run.read(pallet::Walk)){south=true;run.input("D");}
        tick();
    }
    run.require(run.read(pallet::Battle)==1,"real encounter");run.input(nullptr);await_menu();capture("swap-start");
    for(int slot:{1,0}) {
        run.press("U");run.press("R");run.press("A");run.wait(30);capture("swap-party-menu");
        for(int i=0;i<6&&run.read(0xcc26);i++)run.press("U");
        for(int i=0;i<slot;i++)run.press("D");
        run.press("A");run.wait(20);capture("swap-action-menu");run.press("A");
        await_menu();
        run.require(run.read(0xcc2f)==slot,"requested party member is sent out");
        run.require(pallet3d_battle().active&&!pallet3d_battle().full_overlay,"new Pokemon visible in arena");
        capture(slot?"swap-pidgey":"swap-pikachu");
    }
    // Escape after the reversible swaps; a separate scenario covers fainting.
    run.press("D");run.press("R");run.press("A");
    for(int i=0;i<2200&&run.read(pallet::Battle);i++) {
        if(i%20==0)run.input("A");if(i%20==6)run.input(nullptr);tick();
    }
    run.input(nullptr);run.wait(50);capture("swap-return");
    run.require(!run.read(pallet::Battle)&&pallet3d_active()&&checked>30,"swap journey returns to world with verified portraits");
    std::fprintf(stderr,"PASS: real Pikachu/Pidgey swaps, %d exact VRAM portrait checks, original menus, GL and read-only memory\n",checked);return 0;
}
