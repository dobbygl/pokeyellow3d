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
        room(46,"cave-entrance");
        run.move(46,2,5,"U");run.move(46,4,5,"R");run.move(197,5,5,"U");room(197,"diglett-cave");
        // Face into the corridor so the capture verifies ceiling and depth fog,
        // rather than merely showing the wall immediately beside the ladder.
        run.move(197,5,6,"D");
        SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_F3;
        pallet3d_event(&event,false);run.wait(35);capture("diglett-cave-fp");
        run.require(pallet3d_firstperson(),"cave first person active");
        pallet3d_event(&event,false);run.wait(5);
        run.move(46,4,4,"U");room(46,"cave-return");
        std::puts("PASS: real cave round trip and dark first-person interior; GL, bounded cache and read-only memory");
    } else run.require(false,"supported transition fixture");
    return 0;
}
