#include "menu_layout.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
void require(bool ok,const char* message) {if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
int main(int argc,char** argv) {
    using namespace menu_layout;
    require(classify(nullptr).kind==Kind::Full,"unknown data remains a full screen");
    std::array<uint8_t,360> map{};
    require(classify(map.data()).kind==Kind::None,"map without text");
    // A broken Start border must never hide arbitrary pixels behind the 3D.
    for(int y=0;y<14;y++)for(int x=10;x<20;x++) {
        uint8_t tile=border({10,0,10,14},x,y);map[y*20+x]=tile?tile:0x7f;
    }
    auto start=classify(map.data());require(start.kind==Kind::Partial&&start.count==1,"Start window");
    map[19]=0x7f;require(classify(map.data()).kind==Kind::Full,"missing corner is not a recognized layout");
    map[19]=0x7b;map[0]=0x80;require(classify(map.data()).kind==Kind::Full,"text outside recognized rectangles forces full screen");
    // Optional private fixtures: raw 360-byte wTileMap exported from savestates.
    // Their expected kind follows each filename on the command line (0/1/2).
    require((argc-1)%2==0,"fixture arguments: TILEMAP EXPECTED_KIND pairs");
    for(int i=1;i<argc;i+=2) {
        FILE* f=std::fopen(argv[i],"rb");require(f,"private tilemap fixture");
        require(std::fread(map.data(),1,map.size(),f)==map.size()&&std::fgetc(f)==EOF,"fixture has exactly 360 tiles");std::fclose(f);
        auto actual=classify(map.data());
        std::printf("%s: kind=%d regions=%zu\n",argv[i],int(actual.kind),actual.count);
        require(int(actual.kind)==std::atoi(argv[i+1]),"private fixture classification");
    }
    std::puts("PASS: partial layouts, unknown-border fallback and private tilemap fixtures");
}
