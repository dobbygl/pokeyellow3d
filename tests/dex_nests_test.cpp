#include "dex_nests.h"
#include "dex_area_state.h"
#include "assets_manifest_pokeyellow.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
static void check(bool value,const char* why){if(!value){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(int argc,char** argv) {
    if(argc<2)return 77;
    std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> rom(std::istreambuf_iterator<char>(input),{}),sparse(1048576,255);
    check(rom.size()==1048576,"canonical ROM");
    for(auto entry:POKEYELLOW_ASSETS_MANIFEST)std::copy_n(rom.data()+entry.rom_offset,entry.size,sparse.data()+entry.rom_offset);
    kanto::World world(kanto::Rom(rom.data(),rom.size()));world.discover_warps(kanto::Rom(rom.data(),rom.size()));auto entrances=dex_nests::entrances(world);
    for(int n=1;n<=151;n++) {
        int species=mon_pic::species(rom.data(),rom.size(),n);
        auto a=dex_nests::read(rom.data(),rom.size(),species),b=dex_nests::read(sparse.data(),sparse.size(),species);
        check(a.valid&&b.valid&&a.coordinates==b.coordinates&&a.locations.size()==b.locations.size(),"canonical and sparse nest tables agree for all species");
        for(auto place:a.locations)check(entrances.count(place.map)&&place.coordinate!=0x19,"every visible nest maps to an exterior or entrance; Cerulean Cave is suppressed");
    }
    auto broken=rom;broken[0xcb96]=0x80;
    check(!dex_nests::read(broken.data(),broken.size(),mon_pic::species(rom.data(),rom.size(),16)).valid,"out-of-bank encounter pointer rejected");
    check(!dex_nests::read(nullptr,0,84).valid&&!dex_nests::read(rom.data(),rom.size(),0).valid,"invalid image/species rejected");
    if(argc==3) {
        std::ifstream file(argv[2],std::ios::binary);if(!file){std::puts("SKIP: generate original AREA evidence");return 77;}
        std::vector<uint8_t> evidence(std::istreambuf_iterator<char>(file),{});
        check(evidence.size()>=5&&std::string(evidence.begin(),evidence.begin()+5)=="DXAR1","original AREA evidence header");size_t pos=5;
        for(int expected:{16,41,129}) {
            check(pos+2<=evidence.size()&&evidence[pos]==expected,"three original AREA species");int number=evidence[pos++],count=evidence[pos++];
            check(pos+count<=evidence.size(),"bounded original coordinates");std::set<uint8_t> original(evidence.begin()+pos,evidence.begin()+pos+count);pos+=count;
            auto nests=dex_nests::read(rom.data(),rom.size(),mon_pic::species(rom.data(),rom.size(),number));
            check(nests.valid&&nests.coordinates==original,"nests match the original engine's AREA sprites");
        }
        check(pos==evidence.size(),"complete original evidence");
    }
    std::vector<uint8_t> ram(32768),io(128),vram(16384);GBContext ctx{};
    ctx.rom=rom.data();ctx.rom_size=rom.size();ctx.wram=ram.data();ctx.io=io.data();ctx.vram=vram.data();ctx.sp=0xdfe0;
    auto word=[&](int at,int value){ram[at-0xc000]=uint8_t(value);ram[at-0xbfff]=uint8_t(value>>8);};
    word(0xdfe0,0x4064);word(0xdfe2,0x411b);word(0xdfe4,0x5006);ram[0x111d]=84;
    io[0x40]=0x80;io[0x47]=0xe4;ram[0x109a]=1;
    check(dex_area_state::active(&ctx)&&dex_area_state::ready(&ctx),"specific nested AREA calls identify the view");
    for(int i=0;i<50;i++){ram[0x108a]=uint8_t(i);check(dex_area_state::blink(&ctx)==(i<25),"original 25/50 blink phase");}
    rom[0x40117]=0x49;check(!dex_area_state::active(&ctx),"another predef cannot detect AREA");rom[0x40117]=0x4a;
    ctx.sp=0xdfe2;check(!dex_area_state::active(&ctx),"AREA must be nested inside the Pokedex side menu");
    std::puts("PASS: 151 nest tables, sparse ROM, all entrances, bounds and original AREA lifetime/blink");
}
