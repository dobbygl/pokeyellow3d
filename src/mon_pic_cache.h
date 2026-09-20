#pragma once
#include "mon_pic.h"
#include <algorithm>

namespace mon_pic {
// 32 fixed slots (<27 KiB of portrait data). No per-page allocations or growth.
struct Cache {
    static constexpr size_t Capacity=32;
    struct Entry {int species=0;bool flipped=false;uint64_t used=0;Picture picture;};
    std::array<Entry,Capacity> entries{};
    const uint8_t* source=nullptr;size_t source_size=0,decodes=0,hits=0;
    uint64_t clock=0;
    Picture invalid;
    const Picture& get(const uint8_t* rom,size_t size,int species,bool flipped=false) {
        if(source!=rom||source_size!=size){*this={};source=rom;source_size=size;}
        if(!number(rom,size,species))return invalid;
        ++clock;
        for(auto& e:entries)if(e.species==species&&e.flipped==flipped) {e.used=clock;++hits;return e.picture;}
        auto& e=*std::min_element(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.used<b.used;});
        e={species,flipped,clock,front(rom,size,species,flipped)};++decodes;return e.picture;
    }
    size_t resident() const {return std::count_if(entries.begin(),entries.end(),[](const Entry& e){return e.species!=0;});}
};
// Original column-major, interlaced tiles -> transparent RGBA billboard.
// White enclosed by an outline (eyes, highlights) remains opaque.
inline std::array<uint8_t,56*56*4> rgba(const Picture& picture,const std::array<std::array<uint8_t,3>,4>& colors,bool silhouette=false) {
    std::array<uint8_t,56*56*4> out{};if(!picture.valid)return out;
    std::array<uint8_t,56*56> indices{},outside{};std::array<int,56*56> queue{};size_t begin=0,end=0;
    for(int y=0;y<56;y++)for(int x=0;x<56;x++) {
        int column=picture.flipped?6-x/8:x/8,at=(column*56+y)*2,bit=7-x%8;
        int value=((picture.tiles[at]>>bit)&1)|(((picture.tiles[at+1]>>bit)&1)<<1),p=y*56+x;
        indices[p]=uint8_t(value);
        for(int c=0;c<3;c++)out[p*4+c]=silhouette?uint8_t(c?36:22):colors[value][c];
        out[p*4+3]=255;
    }
    auto add=[&](int p){if(!indices[p]&&!outside[p]){outside[p]=1;queue[end++]=p;}};
    for(int i=0;i<56;i++){add(i);add(55*56+i);add(i*56);add(i*56+55);}
    while(begin<end) {
        int p=queue[begin++],x=p%56,y=p/56;out[p*4+3]=0;
        if(x)add(p-1);if(x<55)add(p+1);if(y)add(p-56);if(y<55)add(p+56);
    }
    return out;
}
} // namespace mon_pic
