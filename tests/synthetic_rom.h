#pragma once
#include "kanto_rom.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Procedural 1 MiB cartridge image for the tests that must run without the
// real ROM. Every byte is generated here; nothing is copied from a dump.
//
// The world is deliberately tiny: Pallet-sized home map with a northern and an
// eastern neighbour that close a cycle, one interior reachable by warp, plus
// the two scenes kanto::World always instantiates (51 and 94).
namespace synthetic {
struct Cell { int x=0,z=0; };
struct WarpPoint { int x=0,z=0,destination=0,entrance=0; };
struct SignPoint { int x=0,z=0,text=0; };
struct ActorPoint { int sprite=0,x=0,z=0,movement=0,range=0,text=0; };

namespace detail {
// Bank 4 holds every map header, object list and block list, so a single bank
// byte per map addresses all three. Banks 8 and 9 hold the two tileset block
// tables and tile sheets. Collision lists are reached through
// kanto::Rom::address(1,...), which is the identity below 0x8000, so they live
// in the first bank.
constexpr size_t MapBank=4,MapBase=MapBank*0x4000;
constexpr size_t OutdoorBank=8,OutdoorBlocks=OutdoorBank*0x4000,OutdoorGraphics=OutdoorBlocks+0x1000;
constexpr size_t IndoorBank=9,IndoorBlocks=IndoorBank*0x4000,IndoorGraphics=IndoorBlocks+0x1000;
constexpr size_t OutdoorCollisions=0x2000,IndoorCollisions=0x2100;
constexpr size_t LandPairs=0xada,WaterPairs=0xafc;

// Outdoor tiles. Each block below is uniform: the four movement cells of a
// block share the same top/bottom pair, so terrain() reads the same values
// wherever it samples inside the block.
constexpr uint8_t GroundTop=0x01,GroundBottom=0x02;
constexpr uint8_t GrassTop=0x51,GrassBottom=0x52;
constexpr uint8_t TreeTop=0x40,TreeBottom=0x50;
constexpr uint8_t RockTop=0x2a,RockBottom=0x3a;
constexpr uint8_t LedgeTop=0x1c,LedgeBottom=0x2c;
constexpr uint8_t WaterTop=0x04,WaterBottom=0x14;
constexpr uint8_t FenceTop=0x0e,FenceBottom=0x55;
constexpr uint8_t SignTop=0x46,SignBottom=0x56;
constexpr uint8_t CutTop=0x2d,CutBottom=0x3d;
// Interior tiles for tileset 1 (the classic house family).
constexpr uint8_t FloorTop=0x01,FloorBottom=0x0c;
constexpr uint8_t WallTop=0x10,WallBottom=0x11;
constexpr uint8_t TelevisionTop=0x06,TelevisionBottom=0x07;

struct ConnSpec { char direction='N'; int map=0,y_alignment=0,x_alignment=0; };
struct MapSpec {
    int id=0,tileset=0,blocks_w=1,blocks_h=1,border=0;
    std::vector<uint8_t> blocks;
    std::vector<ConnSpec> connections;
    std::vector<WarpPoint> warps;
    std::vector<SignPoint> signs;
    std::vector<ActorPoint> actors;
};

inline std::vector<uint8_t> build() {
    std::vector<uint8_t> image(kanto::RomSize,0);
    auto put=[&](size_t at,int v){image[at]=uint8_t(v);};
    auto put_word=[&](size_t at,int v){image[at]=uint8_t(v&0xff);image[at+1]=uint8_t((v>>8)&0xff);};

    // --- cartridge header ------------------------------------------------
    // The boot logo at 0x104 stays blank: no consumer in this repository
    // inspects it and there is nothing procedural to put there.
    const char title[]="SYNTHETIC KANTO";
    for(int i=0;i<15;i++)put(0x134+i,title[i]);
    put(0x143,0x80);  // CGB compatible
    put(0x144,'S');put(0x145,'Y');
    put(0x146,0x00);  // no SGB support
    put(0x147,0x1b);  // MBC5 + RAM + battery, as the UE cartridge
    put(0x148,0x05);  // 1 MiB
    put(0x149,0x03);  // 32 KiB SRAM
    put(0x14a,0x01);put(0x14b,0x33);put(0x14c,0x00);

    // --- tileset graphics and block tables --------------------------------
    auto sheet=[&](size_t at,int salt) {
        for(int tile=0;tile<96;tile++)for(int row=0;row<8;row++) {
            put(at+size_t(tile)*16+size_t(row)*2,uint8_t(tile*11+row*29+salt*7));
            put(at+size_t(tile)*16+size_t(row)*2+1,uint8_t(tile*37+row*13+salt*53+0x5a));
        }
    };
    sheet(OutdoorGraphics,0);sheet(IndoorGraphics,1);
    auto uniform=[](uint8_t top,uint8_t bottom) {
        std::array<uint8_t,16> b{};
        for(int c=0;c<4;c++){b[size_t(c)]=top;b[size_t(4+c)]=bottom;b[size_t(8+c)]=top;b[size_t(12+c)]=bottom;}
        return b;
    };
    // Every one of the 256 blocks is filled with tiles below 96 so that
    // Tileset::valid_blocks is true everywhere and ensure_map() never trips.
    auto block_table=[&](size_t at,const std::vector<std::array<uint8_t,16>>& defined) {
        for(int block=0;block<256;block++)for(int i=0;i<16;i++)
            put(at+size_t(block)*16+size_t(i),block<int(defined.size())?defined[size_t(block)][size_t(i)]
                                                                       :uint8_t((block*7+i*3)%96));
    };
    block_table(OutdoorBlocks,{uniform(GroundTop,GroundBottom),uniform(GrassTop,GrassBottom),
        uniform(TreeTop,TreeBottom),uniform(RockTop,RockBottom),uniform(LedgeTop,LedgeBottom),
        uniform(WaterTop,WaterBottom),uniform(FenceTop,FenceBottom),uniform(SignTop,SignBottom),
        uniform(CutTop,CutBottom)});
    block_table(IndoorBlocks,{uniform(FloorTop,FloorBottom),uniform(WallTop,WallBottom),
        uniform(TelevisionTop,TelevisionBottom)});
    for(size_t i=0;i<4;i++)put(OutdoorCollisions+i,std::array<int,4>{GroundBottom,GrassBottom,LedgeBottom,0xff}[i]);
    for(size_t i=0;i<4;i++)put(IndoorCollisions+i,std::array<int,4>{FloorBottom,0x0d,0x1c,0xff}[i]);

    // --- tileset headers ---------------------------------------------------
    auto tileset_header=[&](int id,size_t bank,size_t blocks,size_t graphics,size_t collisions,int grass) {
        size_t p=kanto::TilesetHeaders+size_t(id)*12;
        put(p,int(bank));
        put_word(p+1,int(0x4000+(blocks-bank*0x4000)));
        put_word(p+3,int(0x4000+(graphics-bank*0x4000)));
        put_word(p+5,int(collisions));  // bank 1 addressing is the identity here
        put(p+7,0xff);put(p+8,0xff);put(p+9,0xff);  // no counter tiles
        put(p+10,grass);put(p+11,0);
    };
    // Tileset 1 is the interior family; every other id shares the outdoor
    // sheet so read_tileset() succeeds for all 25 ids, including 3 and 14 used
    // by the two scenes the World constructor always instantiates.
    for(int id=0;id<kanto::TilesetCount;id++)
        if(id==1)tileset_header(id,IndoorBank,IndoorBlocks,IndoorGraphics,IndoorCollisions,0xff);
        else tileset_header(id,OutdoorBank,OutdoorBlocks,OutdoorGraphics,OutdoorCollisions,GrassBottom);

    // --- ledge and tile-pair tables ---------------------------------------
    size_t p=kanto::LedgeTable;
    for(auto entry:{std::array<int,4>{2,GroundBottom,LedgeBottom,0x80},
                    std::array<int,4>{1,GroundBottom,LedgeBottom,0x20}}) {
        for(int i=0;i<4;i++)put(p+size_t(i),entry[size_t(i)]);
        p+=4;
    }
    put(p,0xff);
    p=LandPairs;
    for(auto entry:{std::array<int,3>{0,GroundBottom,WaterBottom},std::array<int,3>{1,FloorBottom,WallBottom}}) {
        for(int i=0;i<3;i++)put(p+size_t(i),entry[size_t(i)]);
        p+=3;
    }
    put(p,0xff);
    put(WaterPairs,0);put(WaterPairs+1,WaterBottom);put(WaterPairs+2,GroundBottom);put(WaterPairs+3,0xff);

    // --- maps --------------------------------------------------------------
    size_t cursor=MapBase;
    auto alloc=[&](size_t n){size_t at=cursor;cursor+=n;return at;};
    auto write_map=[&](const MapSpec& m) {
        size_t blocks_at=alloc(m.blocks.size());
        for(size_t i=0;i<m.blocks.size();i++)put(blocks_at+i,m.blocks[i]);
        size_t objects=alloc(4+m.warps.size()*4+m.signs.size()*3+m.actors.size()*8);
        size_t q=objects;
        put(q++,m.border);
        put(q++,int(m.warps.size()));
        for(auto w:m.warps){put(q,w.z);put(q+1,w.x);put(q+2,w.entrance);put(q+3,w.destination);q+=4;}
        put(q++,int(m.signs.size()));
        for(auto s:m.signs){put(q,s.z);put(q+1,s.x);put(q+2,s.text);q+=3;}
        put(q++,int(m.actors.size()));
        for(auto a:m.actors) {
            put(q,a.sprite);put(q+1,a.z+4);put(q+2,a.x+4);
            put(q+3,a.movement);put(q+4,a.range);put(q+5,a.text);q+=6;
            // Trainers carry a two-byte tail and item objects a one-byte tail.
            if(a.text&0x40){put(q,0);put(q+1,0);q+=2;}
            else if(a.text&0x80){put(q,0);q++;}
        }
        size_t header=alloc(10+m.connections.size()*11+2);
        put(header,m.tileset);put(header+1,m.blocks_h);put(header+2,m.blocks_w);
        put_word(header+3,int(0x4000+(blocks_at-MapBase)));
        put_word(header+5,0);put_word(header+7,0);  // text and script pointers, never read
        int flags=0;
        for(auto c:m.connections)flags|=c.direction=='N'?8:c.direction=='S'?4:c.direction=='W'?2:1;
        put(header+9,flags);
        size_t e=header+10;
        for(char d:{'N','S','W','E'})for(auto c:m.connections)if(c.direction==d) {
            put(e,c.map);put_word(e+1,0);put_word(e+3,0);   // strip pointers, never read
            put(e+5,m.blocks_w*2);put(e+6,m.blocks_w*2);    // strip length and width
            put(e+7,c.y_alignment);put(e+8,c.x_alignment);put_word(e+9,0);
            e+=11;
        }
        put_word(e,int(0x4000+(objects-MapBase)));
        if(m.id>=0) {
            put(kanto::HeaderBanks+size_t(m.id),int(MapBank));
            put_word(kanto::HeaderPointers+size_t(m.id)*2,int(0x4000+(header-MapBase)));
        }
        return header;
    };
    // Unused ids share one minimal but valid header (2x2 cells, no exits), so
    // read_map() only ever fails on the UNUSED_ name check, never on garbage.
    MapSpec stub;stub.id=-1;stub.blocks={0};
    size_t shared=write_map(stub);
    for(int id=0;id<kanto::MapCount;id++) {
        put(kanto::HeaderBanks+size_t(id),int(MapBank));
        put_word(kanto::HeaderPointers+size_t(id)*2,int(0x4000+(shared-MapBase)));
    }
    MapSpec home;home.id=0;home.tileset=0;home.blocks_w=5;home.blocks_h=5;
    home.blocks={0,0,0,0,0, 0,1,0,2,0, 0,3,0,4,0, 0,5,0,6,0, 0,7,0,8,0};
    home.connections={{'N',12,0,-2},{'E',1,6,0}};
    home.warps={{4,6,37,0},{8,2,255,0}};
    home.signs={{2,8,3}};
    home.actors={{1,3,5,2,0,0x01},{2,7,7,1,0,0x84}};
    write_map(home);
    MapSpec north;north.id=12;north.tileset=0;north.blocks_w=4;north.blocks_h=4;
    north.blocks=std::vector<uint8_t>(16,0);
    north.connections={{'S',0,0,2},{'E',1,-2,0}};
    write_map(north);
    MapSpec east;east.id=1;east.tileset=0;east.blocks_w=6;east.blocks_h=6;
    east.blocks=std::vector<uint8_t>(36,0);east.blocks[7]=1;  // a grass block at cells (2..3,2..3)
    east.connections={{'W',0,-6,0}};
    write_map(east);
    MapSpec forest;forest.id=51;forest.tileset=3;forest.blocks_w=2;forest.blocks_h=2;
    forest.blocks=std::vector<uint8_t>(4,0);
    write_map(forest);
    MapSpec dock;dock.id=94;dock.tileset=14;dock.blocks_w=2;dock.blocks_h=2;
    dock.blocks=std::vector<uint8_t>(4,0);
    write_map(dock);
    MapSpec house;house.id=37;house.tileset=1;house.blocks_w=4;house.blocks_h=4;
    house.blocks={1,1,1,1, 1,0,0,1, 1,0,2,1, 1,0,0,1};
    house.warps={{3,7,0,0}};
    write_map(house);

    // --- checksums ---------------------------------------------------------
    uint8_t header_sum=0;
    for(size_t at=0x134;at<=0x14c;at++)header_sum=uint8_t(header_sum-image[at]-1);
    image[0x14d]=header_sum;
    uint16_t global=0;
    for(size_t at=0;at<image.size();at++)if(at!=0x14e&&at!=0x14f)global=uint16_t(global+image[at]);
    image[0x14e]=uint8_t(global>>8);image[0x14f]=uint8_t(global&0xff);
    return image;
}
} // namespace detail

// Everything a test needs to assert concrete values against the image.
struct Layout {
    // Map ids.
    int home=0,north=12,east=1,forest=51,dock=94,interior=37;
    // Tileset ids.
    int outdoor_tileset=0,interior_tileset=1,forest_tileset=3,dock_tileset=14;
    // Sizes in movement cells, i.e. kanto::Map::width / height.
    int home_width=10,home_height=10,north_width=8,north_height=8;
    int east_width=12,east_height=12,forest_width=4,forest_height=4;
    int dock_width=4,dock_height=4,interior_width=8,interior_height=8;
    // Alignment bytes stored in the connection entries.
    int home_north_x_alignment=-2,home_east_y_alignment=6;
    int north_south_x_alignment=2,north_east_y_alignment=-2,east_west_y_alignment=-6;
    // Origins the World BFS must compute; the home map anchors at (0,0).
    int north_origin_x=2,north_origin_z=-8,east_origin_x=10,east_origin_z=-6;
    size_t connected_count=3;  // 0, 12 and 1
    size_t map_count=5;        // plus 51 and 94, always instantiated
    size_t ledge_count=2,pair_count=3,water_pair_index=2;
    // Outdoor probes on the home map, in movement cells.
    Cell ground{0,0},grass{2,2},tree{6,2},rock{2,4},ledge{6,4};
    Cell water{2,6},fence{6,6},signpost{2,8},cut_tree{6,8};
    Cell east_grass{2,2};  // on the eastern neighbour
    // Interior probes.
    Cell interior_floor{2,2},interior_wall{3,0},interior_furniture{4,4},interior_warp{3,7};
    float interior_furniture_height=.85f,interior_wall_height=2.f;
    // Objects.
    WarpPoint home_warp{4,6,37,0};          // leads to the interior
    WarpPoint home_dynamic_warp{8,2,255,0}; // LAST_MAP sentinel, never followed
    WarpPoint interior_exit{3,7,0,0};
    SignPoint home_sign{2,8,3};
    ActorPoint home_actor{1,3,5,2,0,0x01},home_item_actor{2,7,7,1,0,0x84};
    int interior_depth=1;
    // Raw tiles, so tile() can be checked without re-deriving the block tables.
    uint8_t ground_top=detail::GroundTop,ground_bottom=detail::GroundBottom;
    uint8_t grass_top=detail::GrassTop,grass_bottom=detail::GrassBottom;
    uint8_t tree_top=detail::TreeTop,tree_bottom=detail::TreeBottom;
    uint8_t rock_top=detail::RockTop,rock_bottom=detail::RockBottom;
    uint8_t ledge_top=detail::LedgeTop,ledge_bottom=detail::LedgeBottom;
    uint8_t water_top=detail::WaterTop,water_bottom=detail::WaterBottom;
    uint8_t fence_top=detail::FenceTop,fence_bottom=detail::FenceBottom;
    uint8_t sign_top=detail::SignTop,sign_bottom=detail::SignBottom;
    uint8_t cut_top=detail::CutTop,cut_bottom=detail::CutBottom;
    uint8_t floor_top=detail::FloorTop,floor_bottom=detail::FloorBottom;
    uint8_t wall_top=detail::WallTop,wall_bottom=detail::WallBottom;
    uint8_t furniture_top=detail::TelevisionTop,furniture_bottom=detail::TelevisionBottom;
    // Image offsets the rejection tests mutate.
    size_t outdoor_block_table=detail::OutdoorBlocks,interior_block_table=detail::IndoorBlocks;
    size_t outdoor_graphics=detail::OutdoorGraphics,interior_graphics=detail::IndoorGraphics;
    size_t ledge_table=kanto::LedgeTable;
};
inline const Layout& layout() {static const Layout value;return value;}
inline std::vector<uint8_t> rom() {static const std::vector<uint8_t> image=detail::build();return image;}
} // namespace synthetic
