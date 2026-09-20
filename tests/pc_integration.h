#pragma once
namespace pc_qa {
inline int approaching=0,returning=0;
inline bool reached=false,closed=false;
inline float previous=0;
inline uint64_t camera=0;
inline size_t builds=0;
inline void observe(GBContext* ctx,int frame) {
    observe_menu_frame(ctx,frame);
    auto pc=pallet3d_pc();
    auto fail=[&](bool ok,const char* why){if(!ok){std::fprintf(stderr,"[PC] FAIL %s frame=%d progress=%.4f mode=%d\n",why,frame,pc.progress,pc.mode);std::exit(48);}};
    if(pc.active) {
        if(!camera){camera=pallet3d_world_frame().camera;builds=pallet3d_stats().mesh_builds;}
        fail(camera==pallet3d_world_frame().camera&&builds==pallet3d_stats().mesh_builds,"resident scene unchanged");
        fail(pallet3d_input_mask()==255,"relative input neutralized");
        for(float v:pc.matrix)fail(std::isfinite(v),"finite camera projection");
        if(pc.open&&!reached) {
            ++approaching;fail(pc.progress>=previous&&pc.progress-previous<=.063f,"continuous approach");
            if(pc.progress==1)reached=true;
        } else if(!pc.open) {
            ++returning;fail(pc.progress<=previous&&previous-pc.progress<=.063f,"continuous return");
        }
        if((!reached&&approaching==12)||(reached&&!pc.open&&returning==12))
            capture_surface(pc.open?"logs/pc-approach-half.ppm":"logs/pc-return-half.ppm");
    } else if(returning)closed=true;
    previous=pc.progress;
}
inline int focus(GBContext* ctx,bool fp) {
    QaWalk run{ctx};run.wait(30);int map=run.read(pallet::Map);
    run.require(map==41||map==38,"Center or bedroom fixture");
    if(fp){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_F3;pallet3d_event(&e,false);run.wait(30);}
    if(map==41) {
        run.move(map,3,5,"U");run.move(map,13,5,"R");run.move(map,13,4,"U");
    } else {
        run.move(map,run.read(pallet::X),2,"D");run.move(map,0,2,"L");
    }
    run.press("U");run.wait(45);capture_surface("logs/pc-world-before.ppm");
    approaching=returning=0;reached=closed=false;previous=0;camera=0;qa_frame_observer=observe;
    run.press("A");run.wait(180);
    verify_menu_overlay(run,"pc-power-on",1);
    for(int i=0;i<8&&(!pallet3d_pc().monitor||run.read(0xcc28)!=3);i++){run.press("A");run.wait(80);}
    run.require(pallet3d_pc().active&&pallet3d_pc().monitor&&pallet3d_pc().progress==1,"main menu on focused monitor");
    run.require(approaching>=23&&approaching<=25,"approach lasts about 400ms");
    verify_menu_overlay(run,"pc-focused-main",2);
    // Native settings and lost focus freeze presentation time; repeated draws
    // do not advance the guest or rebuild the retained scene.
    auto before=pallet3d_pc();ReadOnlyMemory memory{ctx};
    pallet3d_draw(ctx,800,720,true);run.require(pallet3d_pc().progress==before.progress,"settings pause");
    SDL_Event event{};event.type=SDL_WINDOWEVENT;event.window.event=SDL_WINDOWEVENT_FOCUS_LOST;pallet3d_event(&event,false);
    pallet3d_draw(ctx,800,720,false);run.require(pallet3d_pc().progress==before.progress,"lost focus pause");
    event.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;pallet3d_event(&event,false);
    run.require(memory.unchanged(ctx),"paused PC rendering is read only");run.wait(2);
    for(int i=0;i<15&&(!closed||pallet3d_pc().active);i++){run.press("B");run.wait(30);}
    run.wait(30);qa_frame_observer=nullptr;
    run.require(closed&&!pallet3d_pc().active&&pallet::view(ctx)==pallet::View::Overworld,"log off restores world and HUD");
    run.require(returning>=23&&returning<=25,"return lasts about 400ms");
    run.require(pallet3d_firstperson()==fp,"camera preference preserved");
    capture_surface("logs/pc-world-return.ppm");
    std::printf("PASS: PC map=%d camera=%s approach=%d return=%d guest frames; exact LCD, retained world, GL and read-only rendering\n",map,fp?"first-person":"orthographic",approaching,returning);
    return 0;
}
inline bool bill_main(const GBContext* ctx) {
    return pc_state::sample(ctx).mode==pc_state::Mode::Bill&&battle::live_return(ctx,0x21515,0x3aab);
}
struct Session {
    QaWalk run;
    explicit Session(GBContext* ctx,bool fp):run{ctx} {
        run.wait(30);run.require(run.read(pallet::Map)==41&&run.read(pallet::X)==3&&run.read(pallet::Y)==7,"Center entrance fixture");
        if(fp){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_F3;pallet3d_event(&e,false);run.wait(30);}
        run.move(41,3,5,"U");run.move(41,13,5,"R");run.move(41,13,4,"U");run.press("U");
        qa_frame_observer=observe_menu_frame;run.press("A");run.wait(180);
        until([&](){return pc_state::sample(run.ctx).mode==pc_state::Mode::Center&&run.read(0xcc28)==3;},"Center main menu");
        run.press("A");wait_bill();
    }
    template<class Predicate> void until(Predicate predicate,const char* message) {
        for(int i=0;i<16&&!predicate();i++){run.press("A");run.wait(80);}
        run.require(predicate(),message);run.wait(35);
    }
    void wait_bill(){until([&](){return bill_main(run.ctx);},"Bill main menu");}
    void choose(int index) {
        run.require(bill_main(run.ctx),"Bill menu before operation");
        for(int i=0;i<8&&run.read(0xcc26);i++)run.press("U");
        for(int i=0;i<index;i++)run.press("D");run.press("A");run.wait(65);
    }
    void deposit(int index) {
        int before=run.read(0xda7f),party=run.read(0xd162);choose(1);
        for(int i=0;i<index;i++)run.press("D");
        verify_menu_overlay(run,"box-deposit-list",2);
        auto selected=pallet3d_storage();run.require(selected.active&&selected.party&&selected.selected==index,"deposit cursor highlights its party portrait");
        run.press("A");run.wait(35);verify_menu_overlay(run,"box-deposit-confirm",2);
        run.require(pallet3d_storage().party&&pallet3d_storage().selected==index,"action cursor keeps the selected Pokemon");
        run.press("A");run.wait(200);wait_bill();
        run.require(run.read(0xda7f)==before+1&&run.read(0xd162)==party-1,"original deposit updates box and party");
        verify_menu_overlay(run,"box-deposited",2);
    }
    void withdraw(int index) {
        int before=run.read(0xda7f),party=run.read(0xd162);choose(0);
        for(int i=0;i<index;i++)run.press("D");
        verify_menu_overlay(run,"box-withdraw-list",2);
        run.require(!pallet3d_storage().party&&pallet3d_storage().selected==index,"withdraw cursor highlights the stored portrait");
        run.press("A");run.wait(35);run.press("A");run.wait(200);wait_bill();
        run.require(run.read(0xda7f)==before-1&&run.read(0xd162)==party+1,"original withdrawal updates box and party");
        verify_menu_overlay(run,"box-withdrawn",2);
    }
    void change(int target) {
        choose(3);
        until([&](){return run.read(0xcc28)==11;},"original twelve-box choice");
        for(int i=0;i<15&&run.read(0xcc26)>0;i++)run.press("U");
        for(int i=0;i<target;i++)run.press("D");
        verify_menu_overlay(run,"box-change-list",2);
        run.require(run.read(0xcc26)==target,"original box cursor");
        run.press("A");run.wait(180);wait_bill();
        run.require((run.read(0xd59f)&0x7f)==target&&pallet3d_storage().active&&pallet3d_storage().box==target,"saved box becomes the active WRAM grid");
        verify_menu_overlay(run,"box-changed",2);
    }
    void release(int index) {
        int before=run.read(0xda7f);choose(2);for(int i=0;i<index;i++)run.press("D");run.press("A");run.wait(80);
        until([&](){return battle::tile(run.ctx,14,7)==0x79&&run.read(0xcc28)==1;},"release Yes/No confirmation");
        verify_menu_overlay(run,"box-release-confirm",2);run.press("A");run.wait(180);wait_bill();
        run.require(run.read(0xda7f)==before-1,"original confirmed release removes the Pokemon");
        verify_menu_overlay(run,"box-released",2);
    }
    void close() {
        for(int i=0;i<15&&(pallet3d_pc().active||menu_state::running(run.ctx));i++){run.press("B");run.wait(40);}
        run.wait(30);qa_frame_observer=nullptr;
        run.require(pallet::view(run.ctx)==pallet::View::Overworld&&!pallet3d_pc().active,"storage returns to the interior");
        capture_surface("logs/box-world-return.ppm");
    }
};
inline int storage(GBContext* ctx,bool fp) {
    Session session(ctx,fp);auto& run=session.run;
    int pidgey=mon_pic::species(ctx->rom,ctx->rom_size,16);
    run.require(run.read(0xd162)==2&&run.read(0xd164)==pidgey&&run.read(0xda7f)==0,"naturally caught Pidgey and an empty box");
    verify_menu_overlay(run,"box-empty",2);session.deposit(1);
    run.require(run.read(0xda80)==pidgey,"that captured Pidgey is stored");session.change(6);
    run.require(pallet3d_storage().counts[0]==1&&pallet3d_storage().counts[6]==0,"bank-two saved Pidgey remains visible while bank three is active");
    run.require(gb_context_save_state_file(ctx,"logs/box-bank-three.state"),"private original save/change-box evidence");
    for(auto memory:std::array<std::pair<const char*,std::pair<const uint8_t*,size_t>>,2>{{{"logs/boxes.sram",{ctx->eram,ctx->eram_size}},{"logs/boxes.wram",{ctx->wram,32768}}}}) {
        FILE* file=std::fopen(memory.first,"wb");run.require(file,"private storage evidence");std::fwrite(memory.second.first,1,memory.second.second,file);std::fclose(file);
    }
    session.change(0);session.withdraw(0);run.require(run.read(0xd164)==pidgey,"same Pidgey withdrawn from saved box");
    session.deposit(1);session.release(0);
    run.require(run.read(0xda7f)==0&&run.read(0xd162)==1,"released Pidgey is absent from box and party");
    session.close();
    std::puts("PASS: captured Pidgey deposited, saved to bank two, switched through bank three, withdrawn and released; live shelf, selection, original pixels, GL and read-only rendering");return 0;
}
inline int storage_stress(GBContext* ctx,bool fp) {
    QaWalk prepare{ctx};prepare.wait(30);
    int pidgey=mon_pic::species(ctx->rom,ctx->rom_size,16);
    prepare.require(prepare.read(0xd162)==2&&prepare.read(0xd164)==pidgey,"captured Pidgey source for explicit synthetic multibox fixture");
    std::array<uint8_t,33> original{};std::copy_n(ctx->wram+0x1196,33,original.begin());
    // Party records carry the current level at +33; MoveMon writes it into
    // the box-only +3 field when depositing. The unused party field can be 0.
    original[3]=ctx->wram[0x11b7];
    auto fill=[&](uint8_t* box,int count) {
        std::fill_n(box,pc_storage::BoxBytes,0);box[0]=uint8_t(count);box[count+1]=255;
        for(int i=0;i<count;i++) {
            box[1+i]=uint8_t(pidgey);std::copy(original.begin(),original.end(),box+22+33*i);
            const char* name="PIDGEY";for(int j=0;j<6;j++)box[902+11*i+j]=uint8_t(0x80+name[j]-'A');
            box[908+11*i]=uint8_t(0xf6+(i+1)/10);box[909+11*i]=uint8_t(0xf6+(i+1)%10);box[910+11*i]=0x50;
            std::copy_n(ctx->wram+0x127d,11,box+682+11*i);
        }
    };
    fill(ctx->wram+0x1a7f,20);ctx->wram[0x159f]=0x80;
    for(int bank=2;bank<=3;bank++) {
        auto* base=ctx->eram+bank*0x2000;
        for(int i=0;i<6;i++)fill(base+i*0x462,(bank-2)*6+i+1);
        unsigned whole=255;
        for(int i=0;i<6;i++) {unsigned sum=255;for(int j=0;j<0x462;j++){sum-=base[i*0x462+j];whole-=base[i*0x462+j];}base[0x1a4d+i]=uint8_t(sum);}
        base[0x1a4c]=uint8_t(whole);
    }
    // Only this setup constructs stress data. Every subsequent cursor, transfer,
    // save and box switch below is performed by the unmodified game engine.
    prepare.require(gb_context_save_state_file(ctx,"logs/storage-multibox-world.state"),"private clearly synthetic multibox fixture");
    Session session(ctx,fp);auto& run=session.run;
    run.require(pallet3d_storage().active&&pallet3d_storage().counts[0]==20,"all twenty stress records render in the shelf");
    verify_menu_overlay(run,"box-full-grid",2);session.choose(0);
    for(int i=0;i<20;i++) {
        auto selection=pallet3d_storage();
        std::fprintf(stderr,"[PC] scroll wanted=%d shown=%d party=%d cursor=%d offset=%d\n",i,selection.selected,selection.party,run.read(0xcc26),run.read(0xcc36));
        if(selection.selected!=i){capture_surface("logs/box-scroll-failure.ppm");gb_context_save_state_file(ctx,"logs/box-scroll-failure.state");}
        run.require(selection.active&&!selection.party&&selection.selected==i,"scroll cursor highlights the corresponding one of twenty portraits");
        if(i<19)run.press("D");
    }
    verify_menu_overlay(run,"box-full-scroll",2);run.press("D");run.require(pallet3d_storage().selected==-1,"Cancel has no Pokemon highlight");
    run.press("B");session.wait_bill();session.change(6);run.require(run.read(0xda7f)==7,"original engine loads seven records from bank three");
    session.change(11);run.require(run.read(0xda7f)==12,"twelfth saved box loads all its records");
    session.change(0);run.require(run.read(0xda7f)==20,"first saved box restores all twenty records");
    uint8_t checksum=ctx->eram[0x5a4c];ctx->eram[0x5a4c]^=1;run.wait(2);
    run.require(!pallet3d_storage().active&&pallet3d_pc().active,"invalid bank checksum falls back to original menu over interior");
    verify_menu_overlay(run,"box-invalid-bank",2);ctx->eram[0x5a4c]=checksum;run.wait(2);
    run.require(pallet3d_storage().active,"verified bank restores shelf");session.close();
    std::puts("PASS: explicit synthetic twelve occupied boxes, twenty portraits and cursor scrolling, both SRAM banks, original box switching and corrupt-bank fallback");return 0;
}
} // namespace pc_qa
