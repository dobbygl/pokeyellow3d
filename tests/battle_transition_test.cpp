#include "battle_transition_state.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static void check(bool ok,const char* message) {
    if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int main(int argc,char** argv) {
    using battle_transition::Phase;
    check(argc==2,"private ROM argument");
    std::vector<uint8_t> rom(1048576),wram(8192),vram(8192),io(128);
    std::vector<uint32_t> lcd(160*144,0xff181818);
    FILE* f=std::fopen(argv[1],"rb");check(f,"ROM file");
    check(std::fread(rom.data(),1,rom.size(),f)==rom.size(),"ROM size");std::fclose(f);
    GBContext ctx{};ctx.rom=rom.data();ctx.rom_size=rom.size();ctx.wram=wram.data();ctx.vram=vram.data();ctx.io=io.data();
    ctx.sp=0xdfd1;io[0x40]=0xe3;io[0x47]=0xe4;
    auto stack=[&](std::initializer_list<size_t> calls) {
        std::fill(wram.begin()+0x1fd0,wram.end(),0);
        int pos=ctx.sp-0xc000;
        for(size_t call:calls) {int ret=int(call%0x4000)+0x4003;wram[pos++]=ret&255;wram[pos++]=ret>>8;}
    };
    check(battle_transition::sample(nullptr).phase==Phase::None,"null context");
    // The trainer still has IsInBattle=0; requiring normal() here misses its wipe.
    stack({0xf6046,0x3edd8,0x70aeb});
    check(battle_transition::sample(&ctx).phase==Phase::Wipe,"trainer wipe before battle flag");
    auto saved=rom[0xf6045];rom[0xf6045]^=1;
    check(!battle_transition::entry(&ctx),"far-call bank is verified");rom[0xf6045]=saved;
    saved=rom[0xf6042];rom[0xf6042]^=1;
    check(!battle_transition::entry(&ctx),"far-call destination is verified");rom[0xf6042]=saved;
    ctx.sp=0xdfff;check(!battle_transition::entry(&ctx),"popped stack words ignored");ctx.sp=0xdfd1;
    wram[battle::BattleType-0xc000]=1;
    check(battle_transition::sample(&ctx).phase==Phase::None,"tutorial excluded");wram[battle::BattleType-0xc000]=0;
    wram[battle::Link-0xc000]=1;
    check(battle_transition::sample(&ctx).phase==Phase::None,"link excluded");wram[battle::Link-0xc000]=0;
    for(size_t call:{0x70aebu,0x70b0cu,0x70c31u,0x70c76u,0x70d0au,0x70d40u,0x70d87u,0x70dbcu}) {
        stack({0xf608e,0x3edd8,call});
        check(battle_transition::sample(&ctx).phase==Phase::Wipe,"every ROM wipe yield is recognized");
        saved=rom[call];rom[call]=0;
        check(battle_transition::sample(&ctx).phase!=Phase::Wipe,"wipe CALL instruction verified");rom[call]=saved;
    }
    std::fill_n(wram.begin()+battle::TileMap-0xc000,180,255);
    check(battle_transition::sample(&ctx).wipe==.5f,"progress follows real tile writes");
    stack({0xf608e,0x3edd8,0x70d75});io[0x47]=255;
    check(battle_transition::sample(&ctx).phase==Phase::Flash,"black flash is not loading");
    stack({0xf608e});check(battle_transition::sample(&ctx).phase==Phase::Loading,"post-wipe black is loading");
    stack({0xf60f5});wram[battle::IsInBattle-0xc000]=1;io[0x47]=0xe4;
    check(battle_transition::sample(&ctx,lcd.data()).phase==Phase::Introduction,"silhouette call lifetime");
    check(!battle_transition::sample(&ctx,lcd.data()).hud,"black LCD after first E4 write stays hidden");
    lcd[110*160+20]=0xffffffff;
    check(battle_transition::sample(&ctx,lcd.data()).hud,"HUD appears on first rendered contrast");
    stack({0xf613f});check(battle_transition::sample(&ctx).phase==Phase::Battle,"battle loop lifetime");
    stack({0xf6147});wram[battle::IsInBattle-0xc000]=0;
    check(battle_transition::sample(&ctx).phase==Phase::Exit,"exit remains recognized after flag clears");
    stack({});check(battle_transition::sample(&ctx,lcd.data()).phase==Phase::None,"stale flag and pixels are insufficient");
    std::puts("PASS: canonical calls, trainer pre-flag entry, wipe variants, stack lifetime and LCD latency");
}
