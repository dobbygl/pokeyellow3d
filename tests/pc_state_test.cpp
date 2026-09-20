#include "pc_state.h"
#include "assets_manifest_pokeyellow.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
static void check(bool value,const char* message) {
    if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int main(int argc,char** argv) {
    if(argc!=2)return 77;
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<uint8_t> rom(std::istreambuf_iterator<char>(input),{}),ram(32768),io(128),vram(16384),sparse(1048576,255);
    check(rom.size()==1048576&&pallet::load_catalog(rom.data(),rom.size()),"canonical ROM catalog");
    for(const auto& e:POKEYELLOW_ASSETS_MANIFEST)std::copy_n(rom.data()+e.rom_offset,e.size,sparse.data()+e.rom_offset);
    GBContext ctx{};ctx.rom=rom.data();ctx.rom_size=rom.size();ctx.wram=ram.data();ctx.io=io.data();ctx.vram=vram.data();ctx.sp=0xdfe0;
    auto set=[&](int address,int value){ram[address-0xc000]=uint8_t(value);};
    set(0xdfe0,0x0b);set(0xdfe1,0x34);set(0xc109,4);
    for(int map:{41,38}) {
        const auto* room=pallet::ensure_scene(map);check(room&&room->interior,"room exists");set(0xd35d,map);
        bool found=false;
        for(int z=1;z<room->height&&!found;z++)for(int x=0;x<room->width&&!found;x++) {
            set(0xd361,x);set(0xd360,z);auto t=pc_state::terminal(&ctx);
            if(t.map==map){found=true;std::printf("PC map=%d player=%d,%d terminal=%d,%d height=%.2f\n",map,x,z,t.x,t.z,t.height);}
        }
        check(found,"PC furniture found from original ROM");
        auto expected=map==41?pc_state::Mode::Center:pc_state::Mode::Items;
        check(pc_state::sample(&ctx).mode==expected,"root PC mode differs between Center and bedroom");
        ctx.rom=sparse.data();check(pc_state::sample(&ctx).mode==expected,"sparse runtime contains verified call anchors");ctx.rom=rom.data();
        set(0xc109,0);check(pc_state::sample(&ctx).mode==pc_state::Mode::None,"must face adjacent monitor");set(0xc109,4);
        ctx.sp=0xdfe2;check(pc_state::sample(&ctx).mode==pc_state::Mode::None,"popped root cannot detect PC");ctx.sp=0xdfe0;
    }
    set(0xd35d,41);set(0xd361,13);set(0xd360,4);
    for(auto call:{0x17d3f,0x17d51,0x17d63,0x17d87}) {
        int ret=call%0x4000+0x4003;set(0xdfe2,ret&255);set(0xdfe3,ret>>8);
        auto mode=pc_state::sample(&ctx).mode;
        check(mode!=pc_state::Mode::None&&mode!=pc_state::Mode::Center,"each original submenu is detected");
        uint8_t old=rom[call-4];rom[call-4]=255;check(pc_state::sample(&ctx).mode==pc_state::Mode::Center,"wrong target bank cannot select submenu");rom[call-4]=old;
    }
    auto before=ram;pc_state::sample(&ctx);check(ram==before,"state reader never writes WRAM");
    pc_state::Motion motion;motion.update(true,0,false);
    for(int i=1;i<=24;i++)motion.update(true,i*70224,false);
    check(motion.amount==1,"approach completes in 24 normal guest frames");
    motion.update(false,25*70224,false);float held=motion.amount;
    motion.update(false,40*70224,true);motion.update(false,60*70224,true);motion.update(false,61*70224,false);
    check(motion.amount==held,"pause and first resumed frame preserve progress");
    for(int i=62;i<87;i++)motion.update(false,i*70224,false);
    check(motion.amount==0,"closing restores the original camera endpoint");
    std::puts("PASS: PC furniture, root/submenu calls, bank verification, read-only state and 400ms motion");
}
