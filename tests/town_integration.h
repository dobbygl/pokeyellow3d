#pragma once

// Real-engine town journey, including the early parcel quest when necessary.
// No inventory, event, party, position or RNG bytes are patched by this mode.
inline int town_journey(GBContext* ctx) {
    QaWalk run{ctx};run.wait(30);
    auto item=[&](int id) {for(int i=0;i<run.read(0xd31c)&&i<20;i++)if(run.read(0xd31d+i*2)==id)return run.read(0xd31e + i*2);return 0;};
    auto capture=[&](const char* name) {
        std::string path=std::string("logs/")+name;capture_surface((path+".ppm").c_str());
        run.require(gb_context_save_state_file(ctx,(path+".state").c_str()),"save private town fixture");
        std::fprintf(stderr,"[TOWN] %s map=%d xy=%d,%d view=%d hp=%d/%d parcel=%d balls=%d script=%d\n",name,run.read(pallet::Map),run.read(pallet::X),run.read(pallet::Y),int(pallet::view(ctx)),battle::word(ctx,0xd16b),battle::word(ctx,0xd18c),item(0x46),item(4),run.read(0xd5ef));
    };
    auto settle=[&]() {
        run.input(nullptr);run.wait(45);
        for(int i=0;i<200&&pallet::view(ctx)==pallet::View::Transition;i++)run.tick();
    };
    auto fight=[&]() {
        run.input(nullptr);
        for(int i=0;i<5000;i++) {
            if(i%35==0)run.input("A");if(i%35==6)run.input(nullptr);run.tick();
            if(!run.read(pallet::Battle)&&pallet::view(ctx)==pallet::View::Overworld) {
                run.input(nullptr);run.require(battle::word(ctx,0xd16b)>0,"party survives journey encounter");return;
            }
        }
        capture("town-stuck-battle");run.require(false,"journey encounter finishes");
    };
    auto move=[&](int map,int x,int y,const char* key) {
        run.input(key);
        for(int i=0;i<1400;i++) {
            run.tick();if(run.read(pallet::Battle)){fight();run.input(key);}
            if(run.read(pallet::Map)==map&&run.read(pallet::X)==x&&run.read(pallet::Y)==y&&!run.read(pallet::Walk)) {
                run.input(nullptr);return;
            }
        }
        std::fprintf(stderr,"[TOWN] waypoint=%d/%d/%d\n",map,x,y);capture("town-stuck");run.require(false,"town walking waypoint");
    };
    auto heal=[&]() {
        move(1,23,26,"R");move(41,3,7,"U");settle();
        run.require(pallet3d_active()&&pallet3d_stats().resident_maps==1,"Pokemon Center interior active and bounded");capture("viridian-center");
        int before=battle::word(ctx,0xd16b);move(41,3,3,"U");run.press("U",8);run.press("A",12);
        for(int i=0;i<2200;i++) {
            if(i%40==0)run.input("A");if(i%40==8)run.input(nullptr);run.tick();
            if(i>150&&pallet::view(ctx)==pallet::View::Overworld&&battle::word(ctx,0xd16b)==battle::word(ctx,0xd18c))break;
        }
        run.input(nullptr);settle();
        run.require(battle::word(ctx,0xd16b)==battle::word(ctx,0xd18c)&&pallet3d_active(),"nurse completes real healing");
        std::fprintf(stderr,"[TOWN] healed %d -> %d HP\n",before,battle::word(ctx,0xd16b));capture("viridian-healed");
        move(1,23,26,"D");settle();
    };
    run.require(run.read(pallet::Map)==1&&run.read(pallet::X)==20&&run.read(pallet::Y)==33,"Viridian (20,33) fixture from original journey");
    move(1,20,30,"U");move(1,19,30,"L");move(1,19,26,"U");heal();
    move(1,19,26,"L");move(1,19,20,"U");move(1,29,20,"R");move(42,3,7,"U");
    // The first visit has an automatic conversation and scripted player walk.
    for(int i=0;i<2400;i++) {
        if(i%40==0)run.input("A");if(i%40==8)run.input(nullptr);run.tick();
        if(i>100&&item(0x46)&&pallet::view(ctx)==pallet::View::Overworld)break;
        if(i==300&&!item(0x46)&&pallet::view(ctx)==pallet::View::Overworld)break;
    }
    run.input(nullptr);settle();capture("viridian-mart");
    if(item(0x46)) {
        move(42,3,5,"R");move(1,29,20,"D");settle();
        move(1,19,20,"L");move(1,19,30,"D");move(1,20,30,"R");
        move(12,10,4,"D");move(12,14,4,"R");move(12,14,14,"D");
        move(12,9,14,"L");move(12,9,22,"D");move(12,12,22,"R");
        move(12,12,24,"D");move(12,8,24,"L");move(12,8,28,"D");move(12,10,28,"R");
        move(0,10,2,"D");move(0,9,2,"L");move(0,9,12,"D");move(0,12,12,"R");move(40,5,11,"U");settle();
        move(40,5,3,"U");run.press("U",8);run.press("A",12);
        // Receiving the Pokedex includes several long, engine-paced speeches.
        // Keep advancing text until the script releases control, not a fixed
        // guessed conversation duration.
        for(int i=0;i<12000;i++) {
            if(i%20==0)run.input("A");if(i%20==8)run.input(nullptr);run.tick();
            if(i>300&&!item(0x46)&&run.read(0xd5ef)==22&&!run.read(0xcd6b)&&pallet::view(ctx)==pallet::View::Overworld)break;
        }
        run.input(nullptr);settle();capture("parcel-delivered");
        run.require(!item(0x46)&&pallet::view(ctx)==pallet::View::Overworld,"parcel quest completed by original scripts");
        move(0,12,12,"D");move(0,9,12,"L");move(0,9,2,"U");move(0,10,2,"R");move(12,10,28,"U");
        move(12,8,28,"L");move(12,8,24,"U");move(12,12,24,"R");move(12,12,22,"U");
        move(12,9,22,"L");move(12,9,14,"U");move(12,14,14,"R");move(12,14,4,"U");move(12,10,4,"L");
        move(1,20,30,"U");move(1,19,30,"L");move(1,19,20,"U");move(1,29,20,"R");move(42,3,7,"U");settle();
        move(42,3,5,"U");move(42,2,5,"L");
    } else {
        move(42,3,5,"U");move(42,2,5,"L");
    }
    run.press("L",8);int before=item(4);run.press("A",12);run.wait(80);capture("mart-buy-menu");
    run.press("A",8);run.wait(60);capture("mart-stock");
    run.press("A",8);run.wait(60);capture("mart-quantity");
    for(int i=0;i<9;i++)run.press("U",4);
    run.press("A",8);run.wait(60);capture("mart-confirm");
    for(int i=0;i<10&&item(4)<before+10;i++)run.press("A",8);
    run.require(item(4)==before+10,"buy ten Poke Balls through the original shop");
    for(int i=0;i<8&&pallet::view(ctx)!=pallet::View::Overworld;i++)run.press("B",8);
    settle();capture("mart-purchased");run.require(pallet3d_active(),"shop returns to 3D");
    move(42,3,5,"R");move(1,29,20,"D");settle();
    move(1,19,20,"L");move(1,19,26,"D");heal();move(1,19,26,"L");move(1,19,30,"D");move(1,20,30,"R");
    move(12,10,4,"D");settle();capture("town-route1");
    run.require(pallet3d_active()&&item(4)>=10,"return to Route 1 with bought items");
    std::puts("PASS: real healing, parcel delivery, purchase and Route 1 return; GL, bounded cache and unchanged WRAM/VRAM/framebuffer per frame");return 0;
}
