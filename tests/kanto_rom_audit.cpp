#include "kanto_rom.h"
#include "assets_manifest_pokeyellow.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <chrono>
#include <cstring>

static void check(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
int main(int argc,char** argv) {
    if(argc!=2){std::fprintf(stderr,"Usage: kanto_rom_audit ROM\n");return 2;}
    try {
        std::ifstream file(argv[1],std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
        kanto::Rom rom(bytes.data(),bytes.size());
        auto start=std::chrono::steady_clock::now();kanto::World world(rom);
        check(world.connected_count==36&&world.maps.size()==38,"Expected 36 connected and 2 separate scenes");
        int overworld=0,plateau=0;size_t quads=0;
        std::puts("id,title,tileset,width,height,component,origin_x,origin_z,blocks_offset,connections,warps,signs,actors,floor_quads");
        for(const auto& entry:world.maps) {
            const auto& m=entry.second;
            if(m.component==0){overworld+=m.tileset==0;plateau+=m.tileset==23;}
            quads+=m.width*m.height*4;
            std::printf("%d,%s,%d,%d,%d,%d,%d,%d,%zu,%zu,%zu,%zu,%zu,%d\n",m.id,m.title.c_str(),m.tileset,m.width,m.height,m.component,m.origin_x,m.origin_z,m.blocks,m.connections.size(),m.warps.size(),m.signs.size(),m.actors.size(),m.width*m.height*4);
            for(const auto& c:m.connections) {
                const auto& n=*world.find(c.map);bool inverse=false;
                for(const auto& back:n.connections)if(back.map==m.id)inverse=true;
                check(inverse,"Missing inverse connection");
            }
        }
        check(overworld==34&&plateau==2,"Wrong tileset distribution");
        check(world.find(0)->blocks==99069 && world.find(12)->blocks==114940,"Reference map offsets mismatch");
        check(world.find(1)->width==40 && world.find(1)->height==36,"Wrong Viridian dimensions");
        check(world.find(12)->origin_z==-36 && world.find(1)->origin_x==-10 && world.find(1)->origin_z==-72,"Connection displacement mismatch");
        check(world.tilesets.at(0).walkable[0x2c] && !world.tilesets.at(0).walkable[0x50],"Collision bank mismatch");
        // The runtime only loads manifest ranges. Its sparse ROM must decode identically.
        std::vector<uint8_t> sparse(kanto::RomSize,255);
        for(auto e:POKEYELLOW_ASSETS_MANIFEST)std::memcpy(sparse.data()+e.rom_offset,bytes.data()+e.rom_offset,e.size);
        kanto::World resident(kanto::Rom(sparse.data(),sparse.size()));
        for(const auto& entry:world.maps) {
            const auto& a=entry.second;const auto& b=*resident.find(a.id);
            check(a.block_data==b.block_data&&a.origin_x==b.origin_x&&a.origin_z==b.origin_z,"Missing manifest map data");
            const auto& t=world.tilesets.at(a.tileset);const auto& u=resident.tilesets.at(a.tileset);
            check(t.walkable==u.walkable,"Missing manifest collisions");
            for(int i=0;i<96*16;i++)check(bytes[t.graphics+i]==sparse[u.graphics+i],"Missing manifest graphics");
        }
        // Corrupt-size, out-of-bank pointer, map bounds, terminators and cycle regressions.
        auto reject=[&](std::vector<uint8_t> bad){bool rejected=false;try{kanto::World w(kanto::Rom(bad.data(),bad.size()));}catch(const std::exception&){rejected=true;}check(rejected,"Malformed ROM accepted");};
        auto bad=bytes;bad.resize(512);reject(bad);
        bad=bytes;bad[kanto::HeaderBanks]=64;reject(bad);
        bad=bytes;bad[kanto::HeaderPointers+1]=0xff;reject(bad);
        bad=bytes;bad[world.find(0)->header+1]=0;reject(bad);
        bad=bytes;bad[world.find(12)->header+10+8]++;reject(bad);
        bad=bytes;std::fill(bad.begin()+0x4ac2,bad.begin()+0x4ac2+256,0);reject(bad);
        double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        std::fprintf(stderr,"PASS: 36 connected (34 Overworld/2 Plateau), 2 separate, cycles, manifest, malformed inputs; %zu floor quads; audit %.2f ms\n",quads,ms);
    } catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
