#pragma once

inline int firstperson_controls(GBContext* ctx) {
    QaWalk run{ctx};
    auto key=[&](SDL_Scancode code,bool down,bool repeat=false) {
        SDL_Event event{};event.type=down?SDL_KEYDOWN:SDL_KEYUP;
        event.key.state=down?SDL_PRESSED:SDL_RELEASED;event.key.repeat=repeat;
        event.key.keysym.scancode=code;event.key.keysym.sym=SDL_GetKeyFromScancode(code);
        run.require(SDL_PushEvent(&event)==1&&gb_platform_poll_events(ctx),"real SDL keyboard event");
    };
    auto press=[&](SDL_Scancode code,int frames=4) {
        key(code,true);run.wait(frames);key(code,false);run.wait(30);
    };
    auto capture=[&](const char* name) {
        if(const char* dir=std::getenv("FP_CAPTURE_DIR")) {
            std::string path=std::string(dir)+"/"+name+".ppm";capture_surface(path.c_str());
        }
    };
    run.require(run.read(pallet::Map)==0&&run.read(pallet::X)==9&&run.read(pallet::Y)==7,"Pallet initial fixture");
    run.wait(30);press(SDL_SCANCODE_F3);
    run.require(pallet3d_firstperson(),"F3 enables first person");
    if(const char* path=std::getenv("FP_RECORD"))gb_platform_set_input_record_file(path);
    auto turn=[&](SDL_Scancode code,int amount,const char* name) {
        int before=run.read(0xc109),x=run.read(pallet::X),y=run.read(pallet::Y);
        press(code);
        run.require(run.read(0xc109)==firstperson::turn(before,amount),"relative turn reaches engine orientation");
        run.require(run.read(pallet::X)==x&&run.read(pallet::Y)==y&&!run.read(pallet::Walk),"turn does not advance a tile");
        std::fprintf(stderr,"[FP] %s facing=%d -> %d xy=%d,%d mask=%02x\n",name,before,run.read(0xc109),x,y,pallet3d_input_mask());
    };
    turn(SDL_SCANCODE_A,-1,"left");turn(SDL_SCANCODE_D,1,"right");
    turn(SDL_SCANCODE_S,2,"half-turn");turn(SDL_SCANCODE_S,2,"half-turn back");
    for(int i=0;i<16;i++) {run.wait(i%5);turn(SDL_SCANCODE_D,1,"phase-shifted turn");}
    int facing=run.read(0xc109),x=run.read(pallet::X),y=run.read(pallet::Y);
    key(SDL_SCANCODE_D,true);
    for(int i=0;i<8;i++){key(SDL_SCANCODE_D,true,true);key(SDL_SCANCODE_A,true);key(SDL_SCANCODE_A,false);}
    run.wait(30);key(SDL_SCANCODE_D,false);
    run.require(run.read(0xc109)==firstperson::turn(facing,1)&&run.read(pallet::X)==x&&run.read(pallet::Y)==y,"rapid repeat creates one turn without movement");
    while(run.read(0xc109)!=4)turn(SDL_SCANCODE_D,1,"face north");
    key(SDL_SCANCODE_W,true);
    for(int i=0;i<120&&run.read(pallet::Y)>4;i++)run.tick();
    run.require(run.read(pallet::Y)<=4&&run.read(pallet::X)==9,"W moves forward north");
    int stop=run.read(pallet::Y)-(run.read(pallet::Walk)?1:0);
    key(SDL_SCANCODE_W,false);run.wait(35);
    run.require(run.read(pallet::Y)==stop&&!run.read(pallet::Walk),"W release finishes only current step");
    if(std::getenv("FP_RECORD"))gb_platform_set_input_record_file(nullptr);
    key(SDL_SCANCODE_DOWN,true);
    for(int i=0;i<120;i++){run.tick();if(run.read(pallet::Y)==7&&!run.read(pallet::Walk))break;}
    key(SDL_SCANCODE_DOWN,false);run.wait(30);
    run.require(run.read(pallet::Y)==7&&run.read(0xc109)==0,"absolute arrows preserve original controls");
    // A held relative key must be neutralized by the runtime settings menu.
    key(SDL_SCANCODE_W,true);key(SDL_SCANCODE_ESCAPE,true);key(SDL_SCANCODE_ESCAPE,false);
    run.wait(15);run.require(pallet3d_input_mask()==255,"Esc settings mask relative input");
    key(SDL_SCANCODE_W,false);key(SDL_SCANCODE_ESCAPE,true);key(SDL_SCANCODE_ESCAPE,false);run.wait(20);
    run.require(pallet3d_input_mask()==255&&run.read(pallet::Y)==7,"closing settings does not resurrect held W");
    // Original Start menu: W/A/S/D take the native menu keyboard path even
    // though the retained 3D scene remains behind the original LCD regions.
    key(SDL_SCANCODE_W,true);key(SDL_SCANCODE_RETURN,true);run.wait(8);
    key(SDL_SCANCODE_RETURN,false);key(SDL_SCANCODE_W,false);run.wait(30);
    run.require(pallet::view(ctx)==pallet::View::Dialogue&&pallet3d_input_mask()==255&&pallet3d_active()&&pallet3d_menu().active,"Start menu retains 3D with a neutral mask");
    SDL_Event native{};native.type=SDL_KEYDOWN;native.key.keysym.scancode=SDL_SCANCODE_D;
    run.require(!pallet3d_event(&native,false),"WASD not consumed in dialogue");
    press(SDL_SCANCODE_X,12);run.require(pallet::view(ctx)==pallet::View::Overworld,"return from Start menu");
    // Reach Pallet's sign using the original movement controller, then talk
    // along the engine's facing direction at the center of the FP image.
    run.move(0,9,10,"D");run.move(0,7,10,"L");run.press("U",16);run.wait(30);
    run.require(run.read(0xc109)==4,"face sign north");
    capture("sign-before");press(SDL_SCANCODE_Z,12);
    run.require(pallet::view(ctx)==pallet::View::Dialogue&&pallet3d_input_mask()==255,"Z reads centered sign in original dialogue");
    run.wait(140);capture("sign-dialogue");
    run.require(pallet3d_dialogue_overlay()&&pallet3d_active(),"recognized sign dialogue remains over the 3D scene");
    for(int i=0;i<6&&pallet::view(ctx)==pallet::View::Dialogue;i++)press(SDL_SCANCODE_X,12);
    run.require(pallet::view(ctx)==pallet::View::Overworld,"dialogue returns to FP");
    key(SDL_SCANCODE_W,true);run.render_enabled=false;press(SDL_SCANCODE_F2);
    for(int i=0;i<150&&pallet3d_blend().active;i++){SDL_Delay(4);run.tick();}
    run.require(!pallet3d_active()&&pallet3d_input_mask()==255,"F2 neutralizes relative controls in 2D");
    capture("f2-original");
    key(SDL_SCANCODE_W,false);run.render_enabled=true;press(SDL_SCANCODE_F2);
    for(int i=0;i<150&&pallet3d_blend().active;i++){SDL_Delay(4);run.tick();}
    run.require(pallet3d_active()&&pallet3d_firstperson(),"F2 restores the first-person preference");
    capture("sign-after");
    std::puts("PASS: SDL relative controls, repeated turns, W release, absolute arrows, Esc, Start and centered sign interaction");
    return 0;
}

inline int firstperson_views(GBContext* ctx) {
    QaWalk run{ctx};run.wait(30);
    SDL_Event toggle{};toggle.type=SDL_KEYDOWN;toggle.key.keysym.scancode=SDL_SCANCODE_F3;
    pallet3d_event(&toggle,false);
    bool saw_actor=false;
    const int x=run.read(pallet::X),y=run.read(pallet::Y);
    for(int direction:{4,12,0,8}) {
        for(int attempt=0;run.read(0xc109)!=direction&&attempt<5;attempt++) {
            SDL_Event event{};event.type=SDL_KEYDOWN;event.key.keysym.scancode=SDL_SCANCODE_D;
            run.require(SDL_PushEvent(&event)==1&&gb_platform_poll_events(ctx),"SDL turn input");
            run.wait(32);event.type=SDL_KEYUP;SDL_PushEvent(&event);gb_platform_poll_events(ctx);run.wait(5);
        }
        run.require(run.read(0xc109)==direction,"cardinal view reached without patching RAM");
        run.require(run.read(pallet::X)==x&&run.read(pallet::Y)==y&&!run.read(pallet::Walk),"cardinal views preserve player position");
        auto camera_start=SDL_GetTicks();
        for(int i=0;i<600;i++) {
            run.tick();
            if(std::abs(firstperson::angle_delta(pallet3d_camera().yaw,firstperson::facing_yaw(direction)))<.005f)break;
            // Camera easing uses real ImGui time. Offscreen frames can run
            // faster than 600 frames per second, so give the real clock room
            // to advance instead of making this assertion GPU-speed dependent.
            SDL_Delay(2);
        }
        auto camera=pallet3d_camera();
        std::fprintf(stderr,"[FPVIEW] facing=%d yaw=%.5f target=%.5f elapsed_ms=%u\n",direction,camera.yaw,firstperson::facing_yaw(direction),SDL_GetTicks()-camera_start);
        run.require(std::abs(firstperson::angle_delta(camera.yaw,firstperson::facing_yaw(direction)))<.005f,"camera converges to cardinal orientation");
        run.require(!camera.player_drawn,"player omitted from perspective");saw_actor|=camera.actors>0;
        if(const char* dir=std::getenv("FP_CAPTURE_DIR")) {
            std::string file=std::string(dir)+"/map-"+std::to_string(run.read(pallet::Map))+"-facing-"+std::to_string(direction)+".ppm";
            capture_surface(file.c_str());
        }
    }
    run.require(saw_actor,"NPCs or Pikachu retain visible actor geometry");
    std::puts("PASS: four camera orientations, stable engine position, player hidden, other actors present");return 0;
}

inline int firstperson_house(GBContext* ctx) {
    QaWalk run{ctx};run.wait(30);
    SDL_Event toggle{};toggle.type=SDL_KEYDOWN;toggle.key.keysym.scancode=SDL_SCANCODE_F3;
    pallet3d_event(&toggle,false);
    run.move(0,5,8,"L");run.move(37,2,7,"U");run.input(nullptr);run.wait(70);
    run.require(pallet3d_active()&&pallet3d_firstperson()&&pallet3d_input_mask()==255,"house inherits first person with no stale movement");
    if(const char* dir=std::getenv("FP_CAPTURE_DIR"))capture_surface((std::string(dir)+"/house-3d.ppm").c_str());
    run.move(0,5,6,"D");run.wait(35);
    run.require(pallet3d_active()&&pallet3d_firstperson(),"exit restores first-person preference");
    auto camera=pallet3d_camera();
    run.require(std::abs(firstperson::angle_delta(camera.yaw,firstperson::facing_yaw(run.read(0xc109))))<.001f,"exit camera matches real orientation");
    std::puts("PASS: FP -> house FP -> exterior FP, neutral controls and restored camera");return 0;
}

inline int firstperson_benchmark(GBContext* ctx,bool fp) {
    if(fp) {
        SDL_Event toggle{};toggle.type=SDL_KEYDOWN;toggle.key.keysym.scancode=SDL_SCANCODE_F3;
        pallet3d_event(&toggle,false);
    }
    auto original=std::vector<uint8_t>(ctx->wram,ctx->wram+8192);
    for(int i=0;i<15;i++)gb_platform_render_frame(gb_get_framebuffer(ctx));
    auto stats=pallet3d_stats();
    if(stats.resident_maps!=5||!pallet3d_active())return 45;
    std::array<double,7> times{};
    for(auto& ms:times) {
        glFinish();auto start=std::chrono::steady_clock::now();
        for(int i=0;i<100;i++){gb_platform_render_frame(gb_get_framebuffer(ctx));glFinish();}
        ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/100;
        if(glGetError()!=GL_NO_ERROR||std::memcmp(original.data(),ctx->wram,8192))return 46;
    }
    std::sort(times.begin(),times.end());
    std::fprintf(stderr,"[FPBENCH] mode=%s maps=%zu vertices=%zu bytes=%zu min=%.3f median=%.3f max=%.3f ms GPU=%s\n",fp?"FP":"ortho",stats.resident_maps,stats.vertices,stats.bytes,times.front(),times[3],times.back(),glGetString(GL_RENDERER));
    if(const char* path=std::getenv("SMOKE_CAPTURE"))capture_surface(path);
    return 0;
}
