#include "mon_pic.h"
#include "mon_pic_cache.h"
#include "assets_manifest_pokeyellow.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

static void check(bool value,const char* message) {
    if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
static std::vector<uint8_t> read(const char* path) {
    std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};
}
int main(int argc,char** argv) {
    if(argc<2)return 77;
    auto rom=read(argv[1]);check(rom.size()==1048576,"canonical local ROM size");
    std::vector<uint8_t> sparse(rom.size(),255);
    for(const auto& e:POKEYELLOW_ASSETS_MANIFEST)std::copy_n(rom.data()+e.rom_offset,e.size,sparse.data()+e.rom_offset);
    int sizes[8]{},modes[3]{};
    for(int n=1;n<=151;n++) {
        int id=mon_pic::species(rom.data(),rom.size(),n);
        auto pic=mon_pic::front(rom.data(),rom.size(),id);
        auto resident=mon_pic::front(sparse.data(),sparse.size(),id);
        check(pic.valid&&resident.valid&&pic.tiles==resident.tiles,"all species decode equally from canonical and sparse runtime ROM");
        ++sizes[pic.width];++modes[pic.mode];
    }
    check(sizes[5]&&sizes[6]&&sizes[7],"covers 5x5, 6x6 and 7x7 portraits");
    mon_pic::Cache cache;
    for(int pass=0;pass<4;pass++)for(int n=1;n<=151;n++) {
        int id=mon_pic::species(rom.data(),rom.size(),pass%2?152-n:n);
        auto expected=mon_pic::front(rom.data(),rom.size(),id,pass%2);
        check(cache.get(rom.data(),rom.size(),id,pass%2).tiles==expected.tiles,"cached portrait and orientation equal direct decode");
        auto decodes=cache.decodes;
        cache.get(rom.data(),rom.size(),id,pass%2);
        check(cache.decodes==decodes&&cache.resident()<=32,"cache hit does not decode or exceed memory cap");
    }
    check(cache.resident()==32,"repeated complete traversals retain exactly 32 cache entries");
    cache.get(sparse.data(),sparse.size(),84);
    check(cache.resident()==1,"changing ROM invalidates cached pictures");
    cache={};
    auto get=[&](int n){return cache.get(rom.data(),rom.size(),mon_pic::species(rom.data(),rom.size(),n));};
    for(int n=1;n<=32;n++)get(n);
    get(1);get(33);auto decodes=cache.decodes;get(1);
    check(cache.decodes==decodes,"recently used entry survives eviction");
    get(2);check(cache.decodes==decodes+1,"oldest unused entry is evicted first");
    check(!mon_pic::front(nullptr,0,84).valid&&!mon_pic::front(rom.data(),rom.size(),0).valid&&
        !mon_pic::front(rom.data(),rom.size(),191).valid,"invalid ROM and species fail closed");
    auto corrupt=rom;int id=mon_pic::species(rom.data(),rom.size(),1);
    corrupt[0x383de + 11]=0;corrupt[0x383de + 12]=0;
    check(!mon_pic::front(corrupt.data(),corrupt.size(),id).valid,"bad bank-relative pointer rejected");
    for(unsigned size=0;size<256;size++) {
        std::vector<uint8_t> invalid(size,255);
        check(!mon_pic::decompress(invalid.data(),invalid.size()).valid,"invalid dimensions rejected");
        if(size)invalid[0]=0x77;
        check(!mon_pic::decompress(invalid.data(),invalid.size()).valid,"truncated literal data rejected");
    }
    std::printf("151 species decoded: 5x5=%d 6x6=%d 7x7=%d; modes=%d/%d/%d; sparse ROM matches\n",
        sizes[5],sizes[6],sizes[7],modes[0],modes[1],modes[2]);
    if(argc<3)return 0;
    auto evidence=read(argv[2]);
    if(evidence.empty()){std::fprintf(stderr,"SKIP: generate private original-engine VRAM evidence with tests/dex_portraits_qa.sh\n");return 77;}
    constexpr size_t Record=5+mon_pic::TileBytes;
    check(evidence.size()==5+151*Record&&std::equal(evidence.begin(),evidence.begin()+5,"MPVR1"),"complete 151-species original-engine evidence");
    for(int n=1;n<=151;n++) {
        const uint8_t* record=evidence.data()+5+(n-1)*Record;
        check(record[0]==n&&mon_pic::number(rom.data(),rom.size(),record[1])==n&&record[4]==1,"ordered evidence species and mirrored orientation");
        auto picture=mon_pic::front(rom.data(),rom.size(),record[1],true);
        check(picture.width==record[2]&&picture.height==record[3],"original portrait dimensions");
        check(std::equal(picture.tiles.begin(),picture.tiles.end(),record+5),"byte-exact equality to original vFrontPic");
        auto normal=mon_pic::front(rom.data(),rom.size(),record[1]);
        for(size_t i=0;i<mon_pic::TileBytes;i++)check(normal.tiles[i]==mon_pic::reverse(record[5+i]),"normal orientation agrees with original mirrored bytes");
    }
    std::puts("PASS: 151 original-engine VRAM portraits, 118384 bytes identical");
}
