#pragma once
#include "mon_pic.h"
#include "dex_nests.h"

namespace dex_qa {
inline int invisible_selection_frames=0;
inline void observe(GBContext* ctx,int frame) {
    auto info=pallet3d_dex();
    if(info.active&&info.list) {
        auto selected=dex_state::list(ctx);
        if(!dex_state::visible(ctx,gb_get_framebuffer(ctx),selected)) {
            if(invisible_selection_frames++<8)std::fprintf(stderr,"[DEX] cursor/LCD transfer frame=%d number=%d row=%d\n",frame,selected.number,selected.row);
        }
        auto shown=selected;shown.number=info.number;shown.row=info.row;
        bool visible=!info.number||dex_state::visible(ctx,gb_get_framebuffer(ctx),shown);
        bool correct=!info.number||mon_pic::number(ctx->rom,ctx->rom_size,info.species)==info.number;
        bool current=!dex_state::visible(ctx,gb_get_framebuffer(ctx),selected)||info.number==selected.number;
        if(!selected.number||!visible||!correct||!current||info.cached>mon_pic::Cache::Capacity) {
            std::fprintf(stderr,"[DEX] list selection/cache mismatch at frame %d\n",frame);std::exit(43);
        }
        return;
    }
    if(info.active&&(!info.verified||info.species!=battle::read(ctx,dex_state::Current)||
       info.image!=battle::fingerprint(battle::portrait(ctx,dex_state::Portrait,info.species)))) {
        std::fprintf(stderr,"[DEX] stale or unverified portrait at frame %d\n",frame);std::exit(43);
    }
}
inline int lists(GBContext* ctx,bool fp) {
    QaWalk run{ctx};run.wait(30);
    run.require(pallet::view(ctx)==pallet::View::Overworld,"world fixture before original list");
    if(fp){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_F3;pallet3d_event(&e,false);run.wait(10);}
    // Private bitmap fixture: dex 1 caught, 2 seen, 3 absent, 151 seen so the
    // original menu permits traversal of all 151 numbers. No list/cursor writes.
    std::fill_n(ctx->wram+0x12f6,38,0);
    ctx->wram[0x12f6]=1;ctx->wram[0x1309]=3;ctx->wram[0x131b]=0x40;
    auto initial=pallet3d_world_frame();auto meshes=pallet3d_stats().mesh_builds;
    run.press("S");for(int i=0;i<10&&run.read(0xcc26);i++)run.press("U");run.press("A");run.wait(50);
    qa_frame_observer=observe;
    auto verify=[&](int number) {
        auto info=pallet3d_dex();int expected=number==1?2:(number==2||number==151)?1:0;
        run.require(info.active&&info.list&&info.number==number&&info.registration==expected,"original cursor and caught/seen/absent presentation");
        run.require(info.seen==3&&info.caught==1,"bitmap counters remain exact");
        run.require(battle::tile(ctx,18,2)==0xf9&&battle::tile(ctx,18,5)==0xf7,"original displayed counters match bitmaps");
        mon_pic::Picture picture=mon_pic::front(ctx->rom,ctx->rom_size,info.species,true);
        auto image=mon_pic::rgba(picture,battle::palette(ctx,info.species),expected==1);
        if(!expected)image={};
        run.require(info.image==battle::fingerprint(image),"correct colored, silhouette or empty image uploaded");
        run.require(info.cached<=32&&pallet3d_input_mask()==255,"bounded portrait cache and original controls");
        if(number<=3||number==151) {
            char path[100];std::snprintf(path,sizeof(path),"logs/dex-list-%03d.ppm",number);capture_surface(path);
        }
    };
    verify(1);
    for(int n=2;n<=151;n++){run.press("D",2);verify(n);}
    for(int n=150;n>=1;n--){run.press("U",2);verify(n);}
    for(int n=2;n<=151;n++){run.press("D",2);verify(n);}
    run.require(pallet3d_dex().cached==32,"three complete traversals retain the fixed cache capacity");
    // Exercise the original side menu and CRY on Mew without changing its input.
    run.press("A");run.press("D");run.press("A");run.wait(120);verify(151);
    run.press("B");run.press("B");run.press("B");run.wait(40);
    qa_frame_observer=nullptr;
    run.require(pallet::view(ctx)==pallet::View::Overworld&&!pallet3d_dex().active,"list restores original world");
    run.require(pallet3d_stats().mesh_builds==meshes&&pallet3d_world_frame().camera==initial.camera,"resident meshes and world camera retained");
    run.require(pallet3d_firstperson()==fp,"list preserves camera preference");
    std::fprintf(stderr,"[DEX] cursor/LCD transfer frames=%d\n",invisible_selection_frames);
    std::puts("PASS: original list, three 151-entry traversals, bitmap counters, colored/seen/absent images, cry and bounded cache");return 0;
}
inline bool data(const GBContext* ctx) {
    return battle::live_return(ctx,0x40104,0x4323)&&ctx->io[0x47]==0xe4&&
        battle::tile(ctx,0,0)==0x63&&battle::tile(ctx,19,17)==0x6e;
}
inline int portraits(GBContext* ctx,bool device=false) {
    QaWalk run{ctx};run.wait(30);
    run.require(pallet::view(ctx)==pallet::View::Overworld,"private world fixture with Pokedex");
    if(device)qa_frame_observer=observe;
    // Fixture setup only: let the original menu visit all 151 entries. Neither
    // portrait tiles nor the decompressor's output are ever injected into VRAM.
    std::fill_n(ctx->wram+0x12f6,19,255);ctx->wram[0x1308]=127;
    std::fill_n(ctx->wram+0x1309,19,255);ctx->wram[0x131b]=127;
    run.press("S");run.require(start_visible(ctx),"original Start menu");
    for(int i=0;i<10&&run.read(0xcc26);i++)run.press("U");
    run.press("A");run.wait(50);
    run.require(battle::live_return(ctx,0x4003d,0x4140),"original Pokedex list");
    FILE* out=std::fopen("logs/front-vram.bin","wb");run.require(out,"private portrait evidence");
    std::fwrite("MPVR1",1,5,out);
    for(int n=1;n<=151;n++) {
        run.require(run.read(0xcc26)+run.read(0xcc36)+1==n,"original list selects expected dex number");
        run.press("A");run.press("A");run.wait(140);
        int species=run.read(0xd11d);
        run.require(data(ctx)&&mon_pic::number(ctx->rom,ctx->rom_size,species)==n,"original dex data screen and species");
        ReadOnlyMemory before{ctx};auto picture=mon_pic::front(ctx->rom,ctx->rom_size,species,true);
        run.require(before.unchanged(ctx),"portrait decoder never writes machine memory");
        int differences=0;for(size_t i=0;i<mon_pic::TileBytes;i++)differences+=picture.tiles[i]!=ctx->vram[0x1000+i];
        std::fprintf(stderr,"[DEX-PIC] number=%d species=%d size=%dx%d mode=%d valid=%d differences=%d\n",
            n,species,picture.width,picture.height,picture.mode,picture.valid,differences);
        run.require(picture.valid&&!differences,"ROM decompression equals original vFrontPic byte for byte");
        if(device) {
            run.require(pallet::view(ctx)==pallet::View::Pokedex&&pallet3d_dex().active,"verified original data screen presents the device");
            run.require(pallet3d_covers_frame(ctx)&&pallet3d_input_mask()==255,"device owns the frame with original menu controls");
        }
        uint8_t record[]={uint8_t(n),uint8_t(species),uint8_t(picture.width),uint8_t(picture.height),1};
        std::fwrite(record,1,sizeof(record),out);std::fwrite(ctx->vram+0x1000,1,mon_pic::TileBytes,out);std::fflush(out);
        if(n<=3||n==25||n==151) {
            char path[100];std::snprintf(path,sizeof(path),"logs/dex-data-%03d.ppm",n);capture_surface(path);
            std::snprintf(path,sizeof(path),"logs/dex-data-%03d.state",n);
            run.require(gb_context_save_state_file(ctx,path),"private original dex data fixture");
        }
        for(int i=0;i<6&&!battle::live_return(ctx,0x4003d,0x4140);i++)run.press("B");
        run.wait(40);
        run.require(battle::live_return(ctx,0x4003d,0x4140),"original A/B exits the data text pages to the list");
        if(n!=151)run.press("D");
    }
    std::fclose(out);run.press("B");run.press("B");run.wait(40);
    qa_frame_observer=nullptr;
    std::puts("PASS: 151 original Pokedex data screens, byte-exact portrait decoding and read-only rendering");return 0;
}
inline int screen(GBContext* ctx,bool fp) {
    QaWalk run{ctx};
    if(fp){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_F3;pallet3d_event(&e,false);}
    run.require(data(ctx),"private original data-screen fixture");
    // Render a frozen original frame: no engine input or memory is fabricated.
    auto verify=[&]() {
        ReadOnlyMemory before{ctx};gb_platform_render_frame(gb_get_framebuffer(ctx));
        run.require(glGetError()==GL_NO_ERROR&&before.unchanged(ctx),"device GL and read-only full machine memory");
    };
    verify();run.require(pallet3d_dex().active&&pallet3d_dex().verified,"data fixture independently initializes a device");
    observe(ctx,0);auto uploads=pallet3d_dex().uploads;
    verify();verify();run.require(pallet3d_dex().uploads==uploads,"unchanged data frame does not reupload textures");
    capture_surface(fp?"logs/dex-device-fp.ppm":"logs/dex-device.ppm");
    int original=ctx->wram[dex_state::Current-0xc000];
    // Deliberately mismatch selection and pixels in a private detector fixture.
    // This is not gameplay: it proves a partial transfer cannot show stale art.
    ctx->wram[dex_state::Current-0xc000]=original==84?153:84;
    verify();run.require(!pallet3d_dex().active,"mismatched VRAM falls back without stale portrait");
    ctx->wram[dex_state::Current-0xc000]=uint8_t(original);verify();
    run.require(pallet3d_dex().active,"restored original species presents its own portrait");
    std::puts("PASS: independent data-screen load, exact portrait, bounded texture uploads and stale-VRAM fallback");return 0;
}
inline int areas(GBContext* ctx,bool fp=false) {
    QaWalk run{ctx};run.wait(30);run.require(pallet::view(ctx)==pallet::View::Overworld,"world before original AREA screens");
    if(fp){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_F3;pallet3d_event(&e,false);run.wait(20);}
    // Private seen/owned flags only unlock original menu navigation. No area
    // list, nest coordinates, OAM entries or encounter tables are fabricated.
    std::fill_n(ctx->wram+0x12f6,19,255);ctx->wram[0x1308]=127;
    std::fill_n(ctx->wram+0x1309,19,255);ctx->wram[0x131b]=127;
    run.press("S");for(int i=0;i<10&&run.read(0xcc26);i++)run.press("U");run.press("A");run.wait(50);
    FILE* evidence=std::fopen("logs/areas.bin","wb");run.require(evidence,"private original-area evidence");std::fwrite("DXAR1",1,5,evidence);
    int selected=1;
    for(int number:{16,41,129}) {
        for(;selected<number;selected++)run.press("D",2);
        run.require(run.read(0xcc26)+run.read(0xcc36)+1==number,"original list selects AREA species");
        run.press("A");run.press("D");run.press("D");run.press("A");run.wait(140);
        run.require(battle::live_return(ctx,0x71003,0x3852)&&ctx->io[0x47]==0xe4,"original AREA wait routine");
        std::set<uint8_t> original;
        for(int i=0;i<36;i++) {
            int at=0xc508+4*i,y=run.read(at),x=run.read(at+1),tile=run.read(at+2);
            if(tile!=4||y<24||x<24)continue;
            run.require((y-24)%8==0&&(x-24)%8==0,"original nest OAM coordinates align with town map");
            original.insert(uint8_t(((y-24)/8)*16+(x-24)/8));
        }
        int species=mon_pic::species(ctx->rom,ctx->rom_size,number);auto parsed=dex_nests::read(ctx->rom,ctx->rom_size,species);
        run.require(parsed.valid&&parsed.coordinates==original,"ROM nest reader matches the original AREA sprites");
        std::fprintf(stderr,"[AREA] dex=%d maps=%zu original-locations=%zu coordinates=",number,parsed.locations.size(),original.size());
        for(auto value:original)std::fprintf(stderr,"%02x,",value);std::fputc('\n',stderr);
        uint8_t header[]={uint8_t(number),uint8_t(original.size())};std::fwrite(header,1,2,evidence);
        for(auto value:original)std::fwrite(&value,1,1,evidence);std::fflush(evidence);
        char path[100];std::snprintf(path,sizeof(path),"logs/area-%03d.ppm",number);capture_surface(path);
        std::snprintf(path,sizeof(path),"logs/area-%03d.state",number);run.require(gb_context_save_state_file(ctx,path),"private original area fixture");
        run.press("B");run.wait(40);run.require(battle::live_return(ctx,0x4003d,0x4140),"AREA returns to original list");
    }
    std::fclose(evidence);run.press("B");run.press("B");run.wait(30);
    run.require(pallet::view(ctx)==pallet::View::Overworld,"AREA journey returns to world");
    std::puts("PASS: Pidgey, Zubat and Magikarp nest coordinates match the original AREA sprites");return 0;
}
} // namespace dex_qa
