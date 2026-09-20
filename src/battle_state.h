#pragma once
#include "gbrt.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

// Canonical UE Yellow. Addresses checked against pokeyellow_internal.h and
// pret/pokeyellow's symbols, engine/battle/{core,init_battle,animations}.asm.
// These helpers only read machine state; presentation owns all interpolation.
namespace battle {
constexpr int TileMap=0xc3a0, IsInBattle=0xd056, BattleType=0xd059, Link=0xd12a;
constexpr int Animation=0xd07b, AnimDelay=0xd085, AnimCounter=0xd086;
struct Rect {int x,y,base;};
// CopyUncompressedPicToHL writes seven columns of seven consecutive tiles.
// LoadMonBackPic scales the 28px back image to 56px before loading vBackPic.
constexpr Rect Enemy{12,0,0},Player{1,5,0x31};
constexpr int PortraitSize=56;
using Image=std::array<uint8_t,PortraitSize*PortraitSize*4>;
inline uint64_t fingerprint(const Image& image) {
    uint64_t value=14695981039346656037ull;
    for(uint8_t byte:image){value^=byte;value*=1099511628211ull;}
    return value;
}
inline uint8_t read(const GBContext* ctx,int a){return ctx->wram[a-0xc000];}
inline int word(const GBContext* ctx,int a){return read(ctx,a)*256+read(ctx,a+1);}
inline uint8_t tile(const GBContext* ctx,int x,int y){return read(ctx,TileMap+y*20+x);}
inline bool normal(const GBContext* ctx) {
    return ctx&&ctx->wram&&ctx->vram&&ctx->io&&ctx->rom&&ctx->rom_size==1048576&&
        read(ctx,IsInBattle)>=1&&read(ctx,IsInBattle)<=2&&!read(ctx,BattleType)&&!read(ctx,Link);
}
inline bool rectangle(const GBContext* ctx,Rect r,bool displayed=false) {
    for(int y=0;y<7;y++)for(int x=0;x<7;x++) {
        int id=r.base+x*7+y;
        if(tile(ctx,r.x+x,r.y+y)!=id)return false;
        if(displayed) {
            // Battle UI is normally the LCD window at (WX=7,WY=0), not BG0.
            bool window=(ctx->io[0x40]&0x20)&&ctx->io[0x4a]==0&&ctx->io[0x4b]==7;
            int map=(ctx->io[0x40]&(window?0x40:8))?0x1c00:0x1800;
            if(ctx->vram[map+(r.y+y)*32+r.x+x]!=id)return false;
        }
    }
    return true;
}
inline bool name_matches(const GBContext* ctx,int address,int x,int y) {
    // A HUD name must be present, not the trainer-introduction picture or a
    // full-screen party/item list reusing the same species bytes.
    int length=0;while(length<10&&read(ctx,address+length)!=0x50)++length;
    // CenterMonName shifts names shorter than five characters right by 1–2.
    x+=length<=2?2:length<=4?1:0;
    int count=0;
    for(;count<3;count++) {
        int c=read(ctx,address+count);if(c==0x50)break;
        if(c<0x80||tile(ctx,x+count,y)!=c)return false;
    }
    return count>0;
}
inline bool animation_running(const GBContext* ctx);
inline bool trainer_intro(const GBContext* ctx) {
    // InitBattle sets EnemyMonPartyPos to FF while the trainer owns vFrontPic.
    // LoadEnemyMonData replaces it before sending the first Pokemon. This is
    // stronger than FirstMonsNotOutYet, which also covers Spearow's entrance.
    return normal(ctx)&&read(ctx,IsInBattle)==2&&read(ctx,0xd030)>0&&
        read(ctx,0xcfe7)==255&&(ctx->io[0x40]&0x80)&&ctx->io[0x47]==0xe4&&
        !ctx->io[0x42]&&(ctx->io[0x43]==0||ctx->io[0x43]==2)&&rectangle(ctx,Enemy,true);
}
inline bool ready(const GBContext* ctx) {
    if(trainer_intro(ctx))return true;
    if(!normal(ctx)||!(ctx->io[0x40]&0x80)||read(ctx,0xd11c)||
       !read(ctx,0xcfe4)||!read(ctx,0xd013)||!word(ctx,0xcff3)||!word(ctx,0xd022))return false;
    // Animations deliberately erase portraits, scroll the LCD and flash the
    // palette. Their verified lifetime lets the compositor retain the arena
    // and either play a supported effect or show the complete original LCD.
    if(animation_running(ctx))return true;
    return ctx->io[0x47]==0xe4&&
        ctx->io[0x42]==0&&(ctx->io[0x43]==0||ctx->io[0x43]==2)&&!read(ctx,0xd11c)&&
        read(ctx,0xcfe4)&&read(ctx,0xd013)&&word(ctx,0xcff3)&&word(ctx,0xd022)&&
        rectangle(ctx,Enemy)&&name_matches(ctx,0xcfd9,1,0)&&name_matches(ctx,0xd008,10,7);
}
inline bool live_return(const GBContext* ctx,size_t call,int target) {
    if(ctx->rom[call]!=0xcd||ctx->rom[call+1]!=(target&255)||ctx->rom[call+2]!=(target>>8))return false;
    if(ctx->sp<0xd000||ctx->sp>=0xe000)return false;
    int address=int(call%0x4000)+0x4003;
    for(int a=ctx->sp;a<0xdfff;a+=2)
        if(read(ctx,a)==(address&255)&&read(ctx,a+1)==(address>>8))return true;
    return false;
}
inline bool capture_running(const GBContext* ctx) {
    return normal(ctx)&&read(ctx,IsInBattle)==1&&live_return(ctx,0x79fe7,0x4124);
}
inline bool animation_running(const GBContext* ctx) {
    if(!normal(ctx)||!read(ctx,Animation))return false;
    // IDs/counters are unions and persist. Verified live CALL returns cover
    // PlayMoveAnimation, direct MoveAnimation calls, its damage feedback, and
    // each stage of TossBallAnimation, even during a nested audio bank call.
    return live_return(ctx,0x3f09b,0x3eb4)||live_return(ctx,0x3f0a3,0x3e84)||live_return(ctx,0x78dbc,0x4124)||
        live_return(ctx,0x78dc6,0x4df6)||capture_running(ctx);
}
enum class Effect { Original, Physical, Projectile, Status, Self };
struct Move {int id=0,type=0,effect=0,power=0;Effect presentation=Effect::Original;};
inline Move move(const GBContext* ctx,int id) {
    if(id<1||id>165)return {};
    size_t p=0x38000+6*(id-1);if(ctx->rom[p]!=id)return {};
    Move m{id,ctx->rom[p+3],ctx->rom[p+1],ctx->rom[p+2]};
    int e=m.effect;
    if(!m.power) {
        if((e>=0x0a&&e<=0x0f)||(e>=0x32&&e<=0x37)||e==0x2e||e==0x2f||e==0x38||e==0x40||e==0x41)
            m.presentation=Effect::Self;
        else if((e>=0x12&&e<=0x17)||(e>=0x3a&&e<=0x3f)||e==0x20||e==0x31||e==0x42||e==0x43||e==0x54||e==0x56)
            m.presentation=Effect::Status;
    } else {
        // Multi-turn, transforming, trapping, OHKO and other special protocols
        // keep the complete original animation. Simple damage and side effects
        // use Gen I's ROM type split, without altering damage calculation.
        switch(e) {
        case 0:case 2:case 3:case 4:case 5:case 6:case 0x10:case 0x11:
        case 0x1f:case 0x21:case 0x22:case 0x24:case 0x25:case 0x30:
        case 0x44:case 0x45:case 0x46:case 0x47:case 0x4c:
            m.presentation=m.type<20?Effect::Physical:Effect::Projectile;break;
        }
    }
    return m;
}
inline std::string text(const GBContext* ctx,int address) {
    std::string value;
    for(int i=0;i<10;i++) {
        int c=read(ctx,address+i);if(c==0x50)break;
        if(c>=0x80&&c<=0x99)value+=char('A'+c-0x80);
        else if(c>=0xa0&&c<=0xb9)value+=char('a'+c-0xa0);
        else if(c>=0xf6)value+=char('0'+c-0xf6);
        else if(c==0x7f)value+=' ';else if(c==0xe3)value+='-';
        else if(c==0xef)value+="M";else if(c==0xf5)value+="F";
        else if(c==0xe8)value+='.';else if(c==0xe0)value+='\'';else value+='?';
    }
    return value;
}
struct Mon {int species,hp,max_hp,level,status,hp_color;std::string name;};
inline Mon mon(const GBContext* ctx,bool enemy) {
    int b=enemy?0xcfe4:0xd013;
    return {read(ctx,b),word(ctx,b+1),word(ctx,b+15),read(ctx,b+14),read(ctx,b+4),
        read(ctx,enemy?0xcf1d:0xcf1c),text(ctx,enemy?0xcfd9:0xd008)};
}
inline const char* status(int s) {
    if(s&7)return "SLP";if(s&8)return "PSN";if(s&16)return "BRN";
    if(s&32)return "FRZ";if(s&64)return "PAR";return "";
}
inline int dex(const GBContext* ctx,int species) {
    if(species<1||species>190)return 0;
    int number=ctx->rom[0x410b1+species-1];return number<=151?number:0;
}
inline std::array<std::array<uint8_t,3>,4> palette(const GBContext* ctx,int species) {
    std::array<std::array<uint8_t,3>,4> out{{{255,255,255},{170,170,170},{85,85,85},{0,0,0}}};
    int n=dex(ctx,species);if(!n)return out;
    int id=ctx->rom[0x72921+n];if(id>=40)return out;
    // CGBBasePalettes, little endian RGB555, eight bytes per palette.
    for(int i=0;i<4;i++) {
        size_t p=0x72af9+id*8+i*2;int c=ctx->rom[p]|(ctx->rom[p+1]<<8);
        for(int k=0;k<3;k++)out[i][k]=uint8_t(((c>>(k*5))&31)*255/31);
    }
    return out;
}
inline Image portrait(const GBContext* ctx,Rect r,int species) {
    Image out{};std::array<uint8_t,56*56> indices{};auto colors=palette(ctx,species);
    for(int y=0;y<56;y++)for(int x=0;x<56;x++) {
        int id=tile(ctx,r.x+x/8,r.y+y/8);
        int at=(ctx->io[0x40]&16)?id*16:0x1000+int(int8_t(id))*16;
        int row=at+(y%8)*2,bit=7-x%8;
        int v=((ctx->vram[row]>>bit)&1)|(((ctx->vram[row+1]>>bit)&1)<<1);
        int i=y*56+x;indices[i]=v;
        for(int c=0;c<3;c++)out[i*4+c]=colors[v][c];out[i*4+3]=255;
    }
    // Only remove background white connected to the outside. White eyes and
    // highlights enclosed by the outline remain opaque.
    if(r.base==Player.base) {
        // Back portraits are cropped at the bottom of the LCD picture. Close
        // that crop between its first and last ink pixel before the flood fill,
        // otherwise the white torso is mistaken for outside background.
        int left=56,right=-1;
        for(int x=0;x<56;x++)if(indices[55*56+x]){left=std::min(left,x);right=x;}
        for(int x=left;x<=right;x++)if(!indices[55*56+x])indices[55*56+x]=4;
    }
    std::array<int,56*56> queue{};int head=0,tail=0;
    auto push=[&](int i){if(!indices[i]&&out[i*4+3]){out[i*4+3]=0;queue[tail++]=i;}};
    for(int i=0;i<56;i++){push(i);push(55*56+i);push(i*56);push(i*56+55);}
    while(head<tail) {
        int i=queue[head++],x=i%56,y=i/56;
        if(x)push(i-1);if(x<55)push(i+1);if(y)push(i-56);if(y<55)push(i+56);
    }
    return out;
}
inline int experience_at(const GBContext* ctx,int species,int level) {
    int n=dex(ctx,species);if(!n)return -1;
    int growth=ctx->rom[0x383de + (n-1)*28+19];if(growth>5)return -1;
    size_t p=0x58e73+growth*4;int a=ctx->rom[p]>>4,b=ctx->rom[p]&15;
    if(!b)return -1;
    int c=ctx->rom[p+1];c=(c&128)?-(c&127):c;
    return std::max(0,a*level*level*level/b+c*level*level+ctx->rom[p+2]*level-ctx->rom[p+3]);
}
inline float experience(const GBContext* ctx) {
    int slot=read(ctx,0xcc2f);if(slot>=read(ctx,0xd162)||slot>=6)return 0;
    int p=0xd16a+slot*44,species=read(ctx,p),level=read(ctx,p+33);
    if(level>=100)return 1;
    int value=(read(ctx,p+14)<<16)|(read(ctx,p+15)<<8)|read(ctx,p+16);
    int a=experience_at(ctx,species,level),b=experience_at(ctx,species,level+1);
    return a>=0&&b>a?std::clamp(float(value-a)/(b-a),0.f,1.f):0;
}
} // namespace battle
