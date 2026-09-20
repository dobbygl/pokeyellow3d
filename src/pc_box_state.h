#pragma once
#include "pc_state.h"
#include "pc_storage.h"
namespace pc_box_state {
struct Selection {int index=-1,row=-1;bool party=false;};
inline bool glyph(const GBContext* ctx,const uint32_t* lcd,int tile,int tx,int ty,uint32_t background) {
    if(tile<0||tile>255||tx<0||tx>=20||ty<0||ty>=18)return false;
    int at=(ctx->io[0x40]&16)?tile*16:0x1000+int(int8_t(tile))*16;
    for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
        bool ink=(ctx->vram[at+y*2]|ctx->vram[at+y*2+1])&(1<<(7-x));
        if((lcd[(ty*8+y)*160+tx*8+x]!=background)!=ink)return false;
    }
    return true;
}
inline bool visible(const GBContext* ctx,const uint32_t* lcd,const pc_storage::Snapshot& data,Selection s) {
    const auto& box=s.party?data.party:data.boxes[data.active];
    if(!lcd||s.index<0||s.index>=box.count||s.row<0||s.row>3)return false;
    uint32_t background=lcd[25*160+41];int row=4+2*s.row;
    if(!glyph(ctx,lcd,0xed,5,row,background)&&!glyph(ctx,lcd,0xec,5,row,background))return false;
    const auto& mon=box.mons[s.index];
    for(int i=0;i<10&&mon.name[i]!=0x50;i++)if(!glyph(ctx,lcd,mon.name[i],6+i,row,background))return false;
    return true;
}
inline Selection selection(const GBContext* ctx,const uint32_t* lcd,const pc_storage::Snapshot& data) {
    if(!ctx||!ctx->wram||!ctx->rom||ctx->rom_size!=1048576||!data.valid||data.active<0||data.active>=12||
       !ctx->vram||!ctx->io||!(ctx->io[0x40]&0x80)||ctx->io[0x47]!=0xe4)return {};
    if(battle::live_return(ctx,0x2170b,0x2ae0)) {
        int pointer=battle::read(ctx,0xcf8a)|(battle::read(ctx,0xcf8b)<<8);
        if(pointer!=0xd162&&pointer!=0xda7f)return {};
        int row=battle::read(ctx,0xcc26);Selection s{row+battle::read(ctx,0xcc36),row,pointer==0xd162};
        return visible(ctx,lcd,data,s)?s:Selection{};
    }
    bool deposit=battle::live_return(ctx,0x215ad,0x5789);
    bool withdraw=battle::live_return(ctx,0x2163b,0x5789);
    bool release=battle::live_return(ctx,0x216a9,0x35ef);
    if(deposit||withdraw||release)for(int row=0;row<4;row++) {
        Selection s{battle::read(ctx,0xcf91),row,deposit};
        if(visible(ctx,lcd,data,s))return s;
    }
    return {};
}
} // namespace pc_box_state
