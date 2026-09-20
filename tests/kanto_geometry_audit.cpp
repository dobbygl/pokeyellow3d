#include "world_scene.h"
#include <fstream>
#include <iterator>
#include <set>

int main(int argc,char** argv) {
    if(argc!=2)return 2;
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<uint8_t> rom{std::istreambuf_iterator<char>(input),{}};
    if(!pallet::load_catalog(rom.data(),rom.size()))return 3;
    int total=0,missing=0;
    for(const auto& entry:pallet::catalog->maps) {
        const auto& scene=entry.second;const auto& houses=pallet::houses(scene);
        for(auto h:houses) {
            if(h.w<=0||h.d<=0||h.x<0||h.z<0||h.x+h.w>scene.width||h.z+h.d>scene.height) {
                std::fprintf(stderr,"FAIL: invalid building in map %d\n",scene.id);return 4;
            }
            std::printf("building,%d,%.1f,%.1f,%.1f,%.1f,%.1f\n",scene.id,h.x,h.z,h.w,h.d,h.eave);++total;
        }
        std::map<std::pair<int,int>,int> flat_solids;
        std::array<int,14> classified{};
        for(int z=0;z<scene.height;z++)for(int x=0;x<scene.width;x++) {
            auto type=pallet::terrain(rom.data(),scene,x,z);
            ++classified[size_t(type)];
            int top=pallet::map_tile(rom.data(),scene,x*2,z*2),bottom=pallet::map_tile(rom.data(),scene,x*2,z*2+1);
            if(type==pallet::Terrain::Ground && !pallet::tileset(scene).walkable[bottom] &&
               !pallet::cleared(rom.data(),scene,x,z))++flat_solids[{top,bottom}];
        }
        for(auto entry:flat_solids)
            std::printf("original-flat-solid,%d,%02x,%02x,%d\n",scene.id,entry.first.first,entry.first.second,entry.second);
        for(size_t i=0;i<classified.size();i++)if(classified[i])
            std::printf("terrain,%d,%zu,%d\n",scene.id,i,classified[i]);
        for(auto w:scene.warps) {
            int tile=pallet::map_tile(rom.data(),scene,w.x*2,w.z*2+1);
            // Only the recognized front-door graphic is a guaranteed building.
            // Cave holes, side/back gates and map-return warps are distinct cases.
            if((scene.tileset!=0&&scene.tileset!=23)||tile!=0x1b)continue;
            bool covered=pallet::terrain(rom.data(),scene,w.x,w.z)==pallet::Terrain::Portal;
            for(auto h:houses)covered|=w.x>=h.x&&w.x<h.x+h.w&&w.z==h.z+h.d-1;
            if(!covered){std::fprintf(stderr,"MISSING building: map=%d door=%d,%d\n",scene.id,w.x,w.z);++missing;}
        }
    }
    std::fprintf(stderr,"Geometry audit: %d buildings; %d uncovered front doors\n",total,missing);
    return missing?1:0;
}
