#include "pallet_state.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <iterator>

static void check(bool condition,const char* message) {
    if(!condition) { std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1); }
}
int main(int argc,char** argv) {
    if(argc!=2){std::puts("SKIP: supply a local ROM for scene-state fixtures");return 77;}
    GBContext ctx{};
    std::vector<uint8_t> ram(32768),rom(1048576),vram(16384),io(128);
    std::ifstream input(argv[1],std::ios::binary);
    rom.assign(std::istreambuf_iterator<char>(input),{});
    check(rom.size()==1048576,"Local ROM available");
    ctx.wram=ram.data();ctx.rom=rom.data();ctx.rom_size=rom.size();
    ctx.vram=vram.data();ctx.io=io.data();
    auto set=[&](uint16_t a,int v){ram[a-0xc000]=uint8_t(v);};
    set(pallet::Width,10);set(pallet::Height,9);set(pallet::X,5);set(pallet::Y,6);
    set(pallet::Sprite1,1);set(pallet::UpdateSprites,1);io[0x40]=0x91;io[0x47]=0xe4;
    check(pallet::view(&ctx)==pallet::View::Overworld,"Recognize loaded Pallet Town");
    set(pallet::UpdateSprites,255);check(pallet::view(&ctx)==pallet::View::Overworld,"Do not flicker to 2D during walking updates");set(pallet::UpdateSprites,1);
    set(0xc3a0,0x79);check(pallet::view(&ctx)==pallet::View::Dialogue,"Detect visible menu border before font flag");set(0xc3a0,0);
    set(pallet::Font,1);check(pallet::view(&ctx)==pallet::View::Dialogue,"Distinguish dialogue and Start menu from an unobstructed world");set(pallet::Font,0);
    set(pallet::Battle,1);check(pallet::view(&ctx)==pallet::View::Unsupported,"Keep wild battle in 2D");set(pallet::Battle,0);
    set(pallet::Map,0x28);check(pallet::view(&ctx)==pallet::View::Unsupported,"Reject interior with old exterior dimensions");set(pallet::Map,0);
    io[0x47]=0;check(pallet::view(&ctx)==pallet::View::Transition,"Respect fades");io[0x47]=0xe4;
    set(pallet::Width,0);check(pallet::view(&ctx)==pallet::View::Unsupported,"Do not activate during boot");set(pallet::Width,10);
    check(pallet::view(nullptr)==pallet::View::Unsupported,"No context during launcher");
    // A bottom dialogue is distinct from menus drawn elsewhere on the screen.
    for(int x=0;x<20;x++){set(0xc3a0+12*20+x,0x7a);set(0xc3a0+17*20+x,0x7a);}
    set(0xc3a0+12*20,0x79);set(0xc3a0+12*20+19,0x7b);
    set(0xc3a0+17*20,0x7d);set(0xc3a0+17*20+19,0x7e);
    for(int y=13;y<17;y++){set(0xc3a0+y*20,0x7c);set(0xc3a0+y*20+19,0x7c);}
    check(pallet::bottom_dialogue(&ctx),"Recognize complete lower text frame");
    set(0xc3a0,0x79);check(!pallet::bottom_dialogue(&ctx),"Reject Start/party menu or mixed layout");set(0xc3a0,0);
    set(pallet::Battle,1);check(!pallet::bottom_dialogue(&ctx),"Battle text cannot become an overworld overlay");set(pallet::Battle,0);
    set(0xc3a0+12*20+5,0);check(!pallet::bottom_dialogue(&ctx),"Reject partial text frame during transition");
    for(int i=0;i<360;i++)set(0xc3a0+i,0);
    const auto& valid_blocks=pallet::tileset(*pallet::scene(0)).valid_blocks;
    int invalid=0;while(invalid<256 && valid_blocks[invalid])++invalid;
    check(invalid<256,"Fixture has an invalid map block");
    set(0xc6e8+3*16+3,invalid);
    check(pallet::view(&ctx)==pallet::View::Transition,"Reject invalid live block data before mesh generation");
    set(0xc6e8+3*16+3,0);
    set(pallet::Sprite1+5,1);
    float prev=5.5f;
    for(int remaining=7;remaining>=1;remaining--) {
        set(pallet::Walk,remaining);float x=pallet::player(&ctx)[0];
        check(std::fabs(x-prev-.125f)<.0001f,"Continuous 2px walking increments");prev=x;
    }
    set(pallet::Walk,0);set(pallet::X,6);
    check(std::fabs(pallet::player(&ctx)[0]-prev-.125f)<.0001f,"No jump on step completion");
    set(pallet::X,5);set(pallet::Sprite1+5,255);set(pallet::Walk,4);
    check(pallet::player(&ctx)[0]==5.f,"Signed west movement");
    set(pallet::Walk,0);set(pallet::Sprite1+6,64);set(pallet::Sprite1+4,60);
    set(pallet::Sprite1+16,5);set(pallet::Sprite1+16+6,96);set(pallet::Sprite1+16+4,76);
    pallet::Actor npc{};
    check(pallet::actor(&ctx,1,npc)&&npc.x==7.5f&&npc.z==7.5f,"NPC remains aligned with original screen coordinates");
    set(pallet::Sprite1+16,0);check(!pallet::actor(&ctx,1,npc),"Hidden actors stay hidden");
    set(pallet::Sprite1+16,5);set(pallet::HiddenList,1);set(pallet::HiddenList+1,0);set(pallet::HiddenList+2,255);
    set(pallet::HiddenFlags,1);check(!pallet::actor(&ctx,1,npc),"Story-hidden Oak must not reappear in the wider camera");
    set(pallet::HiddenFlags,0);check(pallet::actor(&ctx,1,npc),"Story-shown actor can reappear");
    // Map handoff and spatial continuity: Route 1 south is Pallet north.
    set(pallet::Map,12);set(pallet::Height,18);set(pallet::X,10);set(pallet::Y,35);
    check(pallet::view(&ctx)==pallet::View::Overworld,"Recognize Route 1 including its southern half");
    check(pallet::actor(&ctx,0,npc)&&npc.z==35.5f,"Route actors are not clipped at Pallet's old height");
    auto south=pallet::world_player(&ctx);
    check(south[0]==10.5f&&south[1]==-.5f,"Route origin matches its real south connection");
    auto camera_before=pallet::camera_target(south[0],south[1],1);
    set(pallet::Map,0);set(pallet::Height,9);set(pallet::Y,0);
    auto north=pallet::world_player(&ctx);
    auto camera_after=pallet::camera_target(north[0],north[1],1);
    check(north[1]-south[1]==1 && camera_after[1]-camera_before[1]==1,"One step across seam moves camera one tile");
    set(pallet::Map,12); // Old dimensions during loading must never show wrong actors.
    check(pallet::view(&ctx)==pallet::View::Unsupported,"Reject partially loaded Route dimensions");
    set(pallet::Height,18);set(pallet::Y,36);
    check(pallet::view(&ctx)==pallet::View::Unsupported,"Guard Route coordinate bounds");set(pallet::Y,20);
    set(pallet::Battle,1);check(pallet::view(&ctx)==pallet::View::Unsupported,"Route wild encounters stay in original 2D");
    set(pallet::Battle,0);check(pallet::view(&ctx)==pallet::View::Overworld,"Return to Route 3D after battle");
    auto route=*pallet::scene(12);
    const auto& ts=pallet::tileset(route);
    // Place one block in a distant row to catch accidental 9-row/Paleta lookup.
    route.block_data[17*10+5]=7;
    rom[ts.blocks+7*16]=0x40;rom[ts.blocks+7*16+4]=0x50;
    check(pallet::terrain(rom.data(),route,10,34)==pallet::Terrain::Tree,"Decode trees from Route ROM blocks");
    rom[ts.blocks+7*16]=0x2c;rom[ts.blocks+7*16+4]=0x37;
    check(pallet::terrain(rom.data(),route,10,34)==pallet::Terrain::Ledge,"Decode one-way ledge boundary");
    rom[ts.blocks+7*16]=0x52;rom[ts.blocks+7*16+4]=0x52;
    check(pallet::terrain(rom.data(),route,10,34)==pallet::Terrain::Grass,"Decode encounter grass");
    rom[ts.graphics+0x52*16]=0x80;
    check(pallet::map_pixel(rom.data(),route,160,544)==1,"Read Route texture from correct atlas location");
    auto before=ram;pallet::player(&ctx);pallet::view(&ctx);pallet::actor(&ctx,0,npc);
    check(ram==before,"Presentation does not write to game memory");
    std::puts("PASS: both maps, seam/camera continuity, terrain decoding, battle fallback, movement, NPCs, read-only state");
}
