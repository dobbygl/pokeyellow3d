#pragma once
// Late-area fixtures use the engine's scripted warp only to reach the start.
// All floor changes, elevator selection and cave crossings then use real input.
inline int interior_transitions(GBContext* ctx) {
    QaWalk run{ctx};run.wait(35);
    auto capture=[&](const char* name) {
        std::string path=std::string("logs/")+name;
        capture_surface((path+".ppm").c_str());
        run.require(gb_context_save_state_file(ctx,(path+".state").c_str()),"save private transition fixture");
    };
    auto room=[&](int map,const char* name) {
        run.input(nullptr);run.wait(50);
        run.require(run.read(pallet::Map)==map&&pallet3d_active(),"correct room after transition");
        run.require(pallet3d_stats().resident_maps==1,"isolated room cache");capture(name);
    };
    if(run.read(pallet::Map)==122) {
        run.require(run.read(pallet::X)==2&&run.read(pallet::Y)==7,"department store entrance fixture");
        room(122,"mart-1f");
        run.move(122,2,5,"U");run.move(122,12,5,"R");run.move(123,12,1,"U");room(123,"mart-2f");
        // Inspect the wide-room camera while preserving the game's coordinates.
        SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_E;
        for(int i=0;i<8;i++)pallet3d_event(&event,false);
        event.type=SDL_MOUSEWHEEL;event.wheel.y=3;pallet3d_event(&event,false);
        run.wait(5);capture("mart-2f-camera");
        run.move(123,12,2,"D");run.move(122,12,1,"U");room(122,"mart-1f-return");
        run.move(122,12,5,"D");run.move(122,1,5,"L");run.move(127,1,3,"U");room(127,"mart-elevator");
        // The elevator panel is the ROM background event; derive its location.
        const auto* elevator=pallet::scene(127);run.require(elevator&&!elevator->signs.empty(),"elevator has panel");
        auto panel=elevator->signs.front();
        std::fprintf(stderr,"[INTERIOR] elevator panel=%d,%d\n",panel.x,panel.z);
        run.move(127,1,1,"U");
        if(panel.x!=1)run.move(127,panel.x,1,panel.x>1?"R":"L");
        run.press("U",4);run.press("A",8);run.wait(50);capture("elevator-menu");
        // List opens at 1F. Select 2F and let the original shake complete.
        run.press("D",4);run.press("A",8);run.wait(150);capture("elevator-selected");
        run.require(pallet3d_active(),"elevator menu returns to 3D");
        if(run.read(pallet::X)!=1)run.move(127,1,1,"L");
        run.move(123,1,1,"D");room(123,"elevator-2f");
        std::puts("PASS: real stairs, wide-room camera and elevator selection; GL, bounded cache and read-only memory");
    } else if(run.read(pallet::Map)==46) {
        auto leave_encounter=[&]() {
            run.input(nullptr);bool encountered=false;
            for(int i=0;i<6000;i++) {
                run.tick();
                encountered|=run.read(pallet::Battle)!=0;
                if(!run.read(pallet::Battle)&&pallet::view(ctx)==pallet::View::Overworld) {
                    run.input(nullptr);if(i>60)break;continue;
                }
                if(battle_menu_visible(ctx)) {
                    run.input(nullptr);run.press("D");run.press("R");run.press("A");
                } else {
                    if(i%20==0)run.input("A");if(i%20==6)run.input(nullptr);
                }
            }
            run.input(nullptr);run.wait(20);
            run.require(!run.read(pallet::Battle)&&run.read(pallet::Map)==197&&pallet3d_active(),"cave encounter ends through original Run menu");
            if(encountered)std::puts("PASS: cave wild encounter escaped through original controls");
        };
        auto cave_move=[&](int map,int x,int y,const char* direction) {
            run.input(direction);
            for(int i=0;i<1500;i++) {
                run.tick();
                if(run.read(pallet::Battle)){leave_encounter();run.input(direction);}
                if(run.read(pallet::Map)==map&&run.read(pallet::X)==x&&run.read(pallet::Y)==y&&!run.read(pallet::Walk)) {
                    run.input(nullptr);return;
                }
            }
            run.require(false,"cave walking waypoint");
        };
        room(46,"cave-entrance");
        run.move(46,2,5,"U");run.move(46,4,5,"R");cave_move(197,5,5,"U");leave_encounter();room(197,"diglett-cave");
        // Face into the corridor so the capture verifies ceiling and depth fog,
        // rather than merely showing the wall immediately beside the ladder.
        cave_move(197,5,6,"D");leave_encounter();
        SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_F3;
        pallet3d_event(&event,false);run.wait(35);capture("diglett-cave-fp");
        run.require(pallet3d_firstperson()&&pallet3d_active()&&pallet::view(ctx)==pallet::View::Overworld,"cave first person active");
        pallet3d_event(&event,false);run.wait(5);
        cave_move(46,4,4,"U");room(46,"cave-return");
        std::puts("PASS: real cave round trip and dark first-person interior; GL, bounded cache and read-only memory");
    } else run.require(false,"supported transition fixture");
    return 0;
}
