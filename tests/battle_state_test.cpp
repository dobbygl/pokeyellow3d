#include "pallet_state.h"
#include "assets_manifest_pokeyellow.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

static void check(bool condition,const char* message) {
    if(!condition){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
int main(int argc,char** argv) {
    if(argc!=2)return 77;
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<uint8_t> rom(std::istreambuf_iterator<char>(input),{});
    check(rom.size()==1048576,"local canonical ROM");
    std::vector<uint8_t> ram(32768),vram(16384),io(128),sparse(1048576,255);
    GBContext ctx{};ctx.wram=ram.data();ctx.vram=vram.data();ctx.io=io.data();ctx.rom=rom.data();ctx.rom_size=rom.size();
    auto set=[&](int a,int value){ram[a-0xc000]=uint8_t(value);};
    auto draw_rect=[&](battle::Rect r) {
        for(int y=0;y<7;y++)for(int x=0;x<7;x++) {
            int id=r.base+x*7+y;set(battle::TileMap+(r.y+y)*20+r.x+x,id);
            vram[0x1800+(r.y+y)*32+r.x+x]=uint8_t(id);
        }
    };
    auto name=[&](int address,int x,int y,const std::string& text) {
        for(size_t i=0;i<text.size();i++){int c=text[i]-'A'+0x80;set(address+i,c);set(battle::TileMap+y*20+x+i,c);}
        set(address+text.size(),0x50);
    };
    io[0x40]=0x81;io[0x47]=0xe4;io[0x43]=2; // battle slide finishes with SCX=2
    set(battle::IsInBattle,1);set(0xcfe4,165);set(0xd013,84);
    set(0xcff4,13);set(0xcfe6,13);set(0xcff2,2);
    set(0xd023,21);set(0xd015,21);set(0xd021,6);
    draw_rect(battle::Enemy);draw_rect(battle::Player);
    name(0xcfd9,1,0,"RATTATA");name(0xd008,10,7,"PIKACHU");
    check(battle::ready(&ctx)&&pallet::view(&ctx)==pallet::View::Battle,"recognize stable battle with two loaded portraits and HUDs");
    for(int type=1;type<=4;type++){set(battle::BattleType,type);check(!battle::ready(&ctx),"tutorial, Safari and special battles stay original");}
    set(battle::BattleType,0);set(battle::Link,1);check(!battle::ready(&ctx),"link battle unsupported");set(battle::Link,0);
    set(0xd11c,1);check(!battle::ready(&ctx),"never label the trainer's back picture as a Pokemon");set(0xd11c,0);
    set(battle::TileMap+12,0x7f);check(!battle::ready(&ctx),"missing enemy picture falls back");draw_rect(battle::Enemy);
    io[0x47]=0xff;check(!battle::ready(&ctx),"palette transition falls back");io[0x47]=0xe4;
    vram[0x1800+12]=0x7f;check(!battle::rectangle(&ctx,battle::Enemy,true),"unfinished VRAM transfer cannot update portrait");draw_rect(battle::Enemy);
    std::copy_n(vram.data()+0x1800,1024,vram.data()+0x1c00);vram[0x1800+12]=0x7f;
    io[0x40]=0xe3;io[0x4b]=7;
    check(battle::rectangle(&ctx,battle::Enemy,true),"battle LCD window uses 9c00 even while BG points at 9800");
    io[0x40]=0x81;draw_rect(battle::Enemy);
    auto player=battle::mon(&ctx,false),enemy=battle::mon(&ctx,true);
    check(player.name=="PIKACHU"&&player.level==6&&player.hp==21&&player.max_hp==21,"player HUD addresses");
    check(enemy.name=="RATTATA"&&enemy.level==2&&enemy.hp==13&&enemy.max_hp==13,"enemy HUD addresses");
    check(battle::dex(&ctx,84)==25&&battle::dex(&ctx,165)==19,"internal species are converted to national dex");
    check(battle::experience_at(&ctx,84,6)==216&&battle::experience_at(&ctx,84,7)==343,"Pikachu growth curve reads ROM coefficients");
    set(0xd162,1);set(0xd16a,84);set(0xd18b,6);set(0xd179,1);set(0xd17a,24); // 280 experience
    check(std::abs(battle::experience(&ctx)-64.f/127)<.0001f,"party experience fraction");
    // Draw a closed black square with a white center in the enemy rectangle.
    auto point=[&](int x,int y,int value,int base=0) {
        int id=base+(x/8)*7+y/8,at=0x1000+id*16+(y%8)*2,mask=1<<(7-x%8);
        vram[at]=(vram[at]&~mask)|((value&1)?mask:0);
        vram[at+1]=(vram[at+1]&~mask)|((value&2)?mask:0);
    };
    for(int i=8;i<=16;i++){point(i,8,3);point(i,16,3);point(8,i,3);point(16,i,3);}
    point(24,32,1);
    auto before_ram=ram,before_vram=vram,before_io=io;
    auto image=battle::portrait(&ctx,battle::Enemy,84);
    check(image[3]==0&&image[(12*56+12)*4+3]==255,"outside white transparent, enclosed white opaque");
    check(image[(32*56+24)*4+3]==255&&image[(24*56+32)*4+3]==0,"column-major tiles decode to correct XY");
    auto colors=battle::palette(&ctx,84);
    check(colors[1][0]==255&&colors[1][1]==255,"Pikachu's yellow palette from ROM");
    for(const auto& e:POKEYELLOW_ASSETS_MANIFEST)std::copy_n(rom.data()+e.rom_offset,e.size,sparse.data()+e.rom_offset);
    ctx.rom=sparse.data();
    check(battle::palette(&ctx,84)==colors&&battle::experience_at(&ctx,84,7)==343,"sparse runtime has palette and growth data");
    check(battle::portrait(&ctx,battle::Enemy,84)==image,"runtime portrait equals full ROM portrait");
    check(ram==before_ram&&vram==before_vram&&io==before_io,"state, portrait, palette and EXP readers never write machine memory");
    for(int y=8;y<56;y++){point(8,y,3,battle::Player.base);point(16,y,3,battle::Player.base);}
    for(int x=8;x<=16;x++)point(x,8,3,battle::Player.base);
    auto back=battle::portrait(&ctx,battle::Player,84);
    check(back[(50*56+12)*4+3]==255&&back[(55*56+12)*4+3]==255&&back[3]==0,
        "cropped back portrait preserves white torso through the bottom edge");
    set(battle::Animation,84);ctx.sp=0xdff0;
    check(!battle::animation_running(&ctx),"stale animation ID alone is not an animation");
    set(0xdff0,0x9e);set(0xdff1,0x70);
    check(battle::animation_running(&ctx),"live animation return address survives a sound bank switch");
    ctx.sp=0xdff2;check(!battle::animation_running(&ctx),"popped stack bytes cannot keep an animation active");
    int categories[5]{};
    for(int id=1;id<=165;id++) {
        auto move=battle::move(&ctx,id);
        check(move.id==id,"all 165 move records are resident and valid");
        ++categories[int(move.presentation)];
        // Every move must retain ownership through the wrapper's cleanup,
        // including erased VRAM rectangles and a fully flashed palette.
        ctx.sp=0xdff0;set(0xdff0,0xa6);set(0xdff1,0x70);
        set(battle::Animation,id);io[0x47]=0xff;set(battle::TileMap+12,0x7f);
        check(battle::animation_running(&ctx)&&battle::ready(&ctx),"all move IDs retain the compositor during animation cleanup");
        ctx.sp=0xdff2;
        check(!battle::animation_running(&ctx)&&!battle::ready(&ctx),"completed moves cannot retain the animation compositor");
    }
    io[0x47]=0xe4;draw_rect(battle::Enemy);
    std::printf("Move coverage: original=%d physical=%d projectile=%d status=%d self=%d total=165\n",
        categories[0],categories[1],categories[2],categories[3],categories[4]);
    check(battle::move(&ctx,33).presentation==battle::Effect::Physical,"Tackle uses physical effect");
    check(battle::move(&ctx,84).presentation==battle::Effect::Projectile&&battle::move(&ctx,84).type==23,"Thundershock uses Electric projectile");
    check(battle::move(&ctx,45).presentation==battle::Effect::Status,"Growl targets opponent");
    check(battle::move(&ctx,97).presentation==battle::Effect::Self,"Agility targets user");
    check(battle::move(&ctx,144).presentation==battle::Effect::Original&&battle::move(&ctx,19).presentation==battle::Effect::Original,"Transform and Fly preserve original animation protocol");
    check(!battle::move(&ctx,0).id&&!battle::move(&ctx,166).id,"animation-only IDs do not read past move table");
    ctx.sp=0xdff0;set(0xdff0,0xea);set(0xdff1,0x5f);set(battle::Animation,0xc2);
    check(battle::capture_running(&ctx)&&battle::animation_running(&ctx),"live toss sequence detected through sound-bank calls");
    io[0x47]=0xff;set(battle::TileMap+12,0x7f);
    check(battle::ready(&ctx),"active animation owns presentation despite erased portrait or flashed palette");
    ctx.sp=0xdff2;check(!battle::ready(&ctx),"stale capture bytes cannot override scene guards");
    io[0x47]=0xe4;draw_rect(battle::Enemy);set(battle::IsInBattle,2);set(0xd030,25);set(0xcfe7,255);set(0xd11c,1);
    check(battle::trainer_intro(&ctx)&&battle::ready(&ctx),"trainer owns front portrait before first enemy is loaded");
    set(0xcfe7,0);check(!battle::trainer_intro(&ctx),"first enemy is never mistaken for the trainer even while player has not sent out");
    std::puts("PASS: battle guards, HUDs, portraits, palette, EXP, sparse ROM and animation lifetime");
}
