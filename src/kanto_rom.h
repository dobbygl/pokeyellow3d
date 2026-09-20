#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <map>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include "map_names.h"

namespace kanto {
// Supported UE Yellow layout, verified against pret's pokeyellow.sym.
constexpr size_t RomSize=0x100000, HeaderPointers=0xfc1f2, HeaderBanks=0xfc3e4;
constexpr size_t TilesetHeaders=0xc558, LedgeTable=0x1a851;
constexpr int MapCount=249, TilesetCount=25;
class Rom {
    const uint8_t* bytes_; size_t size_;
public:
    Rom(const uint8_t* bytes,size_t size):bytes_(bytes),size_(size) {
        if(!bytes || size<RomSize)throw std::runtime_error("Expected 1 MiB UE Yellow ROM");
    }
    void range(size_t at,size_t count) const {
        if(at>size_ || count>size_-at)throw std::runtime_error("ROM range exceeds image");
    }
    uint8_t byte(size_t at) const {range(at,1);return bytes_[at];}
    uint16_t word(size_t at) const {range(at,2);return bytes_[at]|(bytes_[at+1]<<8);}
    size_t address(unsigned bank,unsigned pointer) const {
        if(bank>=size_/0x4000 || pointer>=0x8000)throw std::runtime_error("Invalid ROM bank/pointer");
        return pointer<0x4000?pointer:bank*0x4000+pointer-0x4000;
    }
    std::vector<uint8_t> copy(size_t at,size_t count) const {
        range(at,count);return {bytes_+at,bytes_+at+count};
    }
};
struct Tileset {
    int id=0,bank=0,grass=255,animations=0;
    size_t blocks=0,graphics=0,collisions=0;
    std::array<bool,256> walkable{};
    std::array<bool,256> valid_blocks{};
    std::array<int,3> counters{};
};
struct Connection { char direction; int map,y_alignment,x_alignment; };
struct Warp { int x,z,destination,entrance; };
struct Sign { int x,z,text; };
struct Object { int sprite,x,z,movement,range,text; };
struct Map {
    int id=0,bank=0,tileset=0,width=0,height=0,origin_x=0,origin_z=0,component=-1,border=0;
    size_t header=0,blocks=0,objects=0;
    std::string title;
    bool interior=false;
    std::vector<uint8_t> block_data;
    std::vector<Connection> connections;
    std::vector<Warp> warps;
    std::vector<Sign> signs;
    std::vector<Object> actors;
};
inline bool dynamic_warp(const Map& m,const Warp& warp) {
    // SilphCoElevatorStoreWarpEntriesScript replaces ED with the entry floor.
    return warp.destination==255 || (m.id==236&&warp.destination==237);
}
inline std::string title(int id) {
    static const char* cities[]={"PUEBLO PALETA","CIUDAD VERDE","CIUDAD PLATEADA","CIUDAD CELESTE",
        "PUEBLO LAVANDA","CIUDAD CARMIN","CIUDAD AZULONA","CIUDAD FUCSIA","ISLA CANELA","MESETA ANIL","CIUDAD AZAFRAN"};
    if(id>=0 && id<11)return cities[id];
    if(id>=12 && id<=36)return "RUTA "+std::to_string(id-11);
    if(id==51)return "BOSQUE VERDE";
    if(id==94)return "MUELLE DE CARMIN";
    if(id>=0&&id<MapCount) {
        std::string name=MapNames[id];std::replace(name.begin(),name.end(),'_',' ');return name;
    }
    return "MAPA "+std::to_string(id);
}
inline Tileset read_tileset(const Rom& rom,int id) {
    if(id<0 || id>=TilesetCount)throw std::runtime_error("Invalid tileset ID");
    size_t p=TilesetHeaders+id*12;
    Tileset t;t.id=id;t.bank=rom.byte(p);
    t.blocks=rom.address(t.bank,rom.word(p+1));
    t.graphics=rom.address(t.bank,rom.word(p+3));
    // Collision lists live in bank 1, NOT in the graphics bank from the header.
    t.collisions=rom.address(1,rom.word(p+5));
    for(int i=0;i<3;i++)t.counters[i]=rom.byte(p+7+i);
    t.grass=rom.byte(p+10);t.animations=rom.byte(p+11);
    rom.range(t.graphics,96*16);
    for(size_t block=0;block<t.valid_blocks.size();block++) {
        size_t at=t.blocks+block*16;
        bool valid=at+16<=RomSize;
        if(valid)for(int i=0;i<16;i++)valid&=rom.byte(at+i)<96;
        t.valid_blocks[block]=valid;
    }
    for(int i=0;i<256;i++) {
        auto tile=rom.byte(t.collisions+i);
        if(tile==255)return t;
        t.walkable[tile]=true;
    }
    throw std::runtime_error("Unterminated collision list");
}
inline Map read_map(const Rom& rom,int id) {
    if(id<0 || id>=MapCount || std::string(MapNames[id]).find("UNUSED_")==0)throw std::runtime_error("Invalid or unused map ID "+std::to_string(id));
    Map m;m.id=id;m.title=title(id);m.bank=rom.byte(HeaderBanks+id);
    m.header=rom.address(m.bank,rom.word(HeaderPointers+id*2));
    size_t p=m.header;
    m.tileset=rom.byte(p);m.height=rom.byte(p+1)*2;m.width=rom.byte(p+2)*2;
    m.interior=m.tileset!=0&&m.tileset!=3&&m.tileset!=14&&m.tileset!=23;
    if(m.tileset>=TilesetCount || m.width<2 || m.height<2 || m.width>128 || m.height>160)
        throw std::runtime_error("Invalid map dimensions/tileset for map "+std::to_string(id));
    m.blocks=rom.address(m.bank,rom.word(p+3));
    m.block_data=rom.copy(m.blocks,(m.width/2)*(m.height/2));
    int flags=rom.byte(p+9);
    if(flags&0xf0)throw std::runtime_error("Invalid connection flags");
    p+=10;
    const char directions[]={'N','S','W','E'};
    for(int i=0;i<4;i++)if(flags&(8>>i)) {
        rom.range(p,11);
        int destination=rom.byte(p);
        if(destination>=MapCount || destination==11)throw std::runtime_error("Invalid connected map");
        m.connections.push_back({directions[i],destination,int8_t(rom.byte(p+7)),int8_t(rom.byte(p+8))});p+=11;
    }
    m.objects=rom.address(m.bank,rom.word(p));p=m.objects;
    m.border=rom.byte(p++);
    int n=rom.byte(p++);if(n>32)throw std::runtime_error("Excessive warp count");
    for(int i=0;i<n;i++,p+=4) {
        Warp w{rom.byte(p+1),rom.byte(p),rom.byte(p+3),rom.byte(p+2)};
        if(w.x>=m.width || w.z>=m.height)throw std::runtime_error("Warp outside map");
        m.warps.push_back(w);
    }
    n=rom.byte(p++);if(n>32)throw std::runtime_error("Excessive sign count");
    for(int i=0;i<n;i++,p+=3) {
        Sign s{rom.byte(p+1),rom.byte(p),rom.byte(p+2)};
        if(s.x>=m.width || s.z>=m.height)throw std::runtime_error("Sign outside map");
        m.signs.push_back(s);
    }
    n=rom.byte(p++);if(n>16)throw std::runtime_error("Excessive actor count");
    for(int i=0;i<n;i++) {
        Object o{rom.byte(p),int(rom.byte(p+2))-4,int(rom.byte(p+1))-4,rom.byte(p+3),rom.byte(p+4),rom.byte(p+5)};
        p+=6;rom.range(p,0);
        if(o.text&0x40) {rom.range(p,2);p+=2;}
        else if(o.text&0x80) {rom.range(p,1);p++;}
        m.actors.push_back(o);
    }
    return m;
}
struct Ledge { int direction,from,to,input; };
struct TilePair { int tileset,a,b; bool water; };
class World {
public:
    std::map<int,Map> maps;
    std::map<int,Tileset> tilesets;
    std::vector<Ledge> ledges;
    std::vector<TilePair> pairs;
    size_t connected_count=0;
    explicit World(const Rom& rom) {
        maps.emplace(0,read_map(rom,0));maps.at(0).component=0;
        std::queue<int> queue;queue.push(0);
        while(!queue.empty()) {
            int id=queue.front();queue.pop();
            const auto& a=maps.at(id);
            for(auto c:a.connections) {
                bool fresh=maps.find(c.map)==maps.end();
                if(fresh)maps.emplace(c.map,read_map(rom,c.map));
                auto& b=maps.at(c.map);
                int x=a.origin_x,z=a.origin_z;
                switch(c.direction) {
                case 'N':x-=c.x_alignment;z-=b.height;break;
                case 'S':x-=c.x_alignment;z+=a.height;break;
                case 'W':x-=b.width;z-=c.y_alignment;break;
                case 'E':x+=a.width;z-=c.y_alignment;break;
                }
                if(!fresh && (x!=b.origin_x || z!=b.origin_z))throw std::runtime_error("Contradictory connection cycle");
                b.origin_x=x;b.origin_z=z;b.component=0;
                if(fresh)queue.push(c.map);
            }
        }
        connected_count=maps.size();
        for(int id:{51,94}) {auto m=read_map(rom,id);m.component=id;maps.emplace(id,std::move(m));}
        for(const auto& entry:maps) {
            const auto& m=entry.second;
            if(!tilesets.count(m.tileset))tilesets.emplace(m.tileset,read_tileset(rom,m.tileset));
            const auto& t=tilesets.at(m.tileset);
            for(auto block:m.block_data)for(int i=0;i<16;i++) {
                int tile=rom.byte(t.blocks+block*16+i);
                if(tile>=96)throw std::runtime_error("Unexpected graphics tile in map "+std::to_string(m.id));
                rom.range(t.graphics+tile*16,16);
            }
        }
        for(int i=0;i<64;i++) {
            size_t p=LedgeTable+i*4;
            if(rom.byte(p)==255)break;
            if(i==63)throw std::runtime_error("Unterminated ledge table");
            ledges.push_back({rom.byte(p),rom.byte(p+1),rom.byte(p+2),rom.byte(p+3)});
        }
        for(size_t table:{size_t(0xada),size_t(0xafc)})for(int i=0;i<64;i++) {
            size_t p=table+i*3;
            if(rom.byte(p)==255)break;
            if(i==63)throw std::runtime_error("Unterminated tile-pair table");
            pairs.push_back({rom.byte(p),rom.byte(p+1),rom.byte(p+2),table==0xafc});
        }
    }
    const Map* find(int id) const {auto it=maps.find(id);return it==maps.end()?nullptr:&it->second;}
    const Map& ensure_map(const Rom& rom,int id) {
        if(const auto* known=find(id))return *known;
        auto m=read_map(rom,id);m.component=id;
        if(!tilesets.count(m.tileset))tilesets.emplace(m.tileset,read_tileset(rom,m.tileset));
        const auto& t=tilesets.at(m.tileset);
        for(auto b:m.block_data)if(!t.valid_blocks[b])throw std::runtime_error("Invalid block in map "+std::to_string(id));
        return maps.emplace(id,std::move(m)).first->second;
    }
    // Metadata audit; meshes are still generated only for the resident scene.
    // FF is the engine's LAST_MAP return sentinel, not a map to instantiate.
    std::map<int,int> discover_warps(const Rom& rom,int max_depth=32) {
        std::map<int,int> depths;std::queue<int> queue;
        for(const auto& entry:maps) {depths[entry.first]=0;queue.push(entry.first);}
        while(!queue.empty()) {
            int id=queue.front();queue.pop();
            const auto& m=maps.at(id);
            for(const auto& warp:m.warps) {
                int next=warp.destination;if(dynamic_warp(m,warp)||depths.count(next))continue;
                if(depths.at(id)>=max_depth)throw std::runtime_error("Warp graph exceeds depth limit");
                try {ensure_map(rom,next);} catch(const std::exception& e) {
                    throw std::runtime_error("Warp "+std::to_string(id)+" -> "+std::to_string(next)+": "+e.what());
                }
                depths[next]=depths.at(id)+1;queue.push(next);
            }
        }
        return depths;
    }
    uint8_t tile(const Rom& rom,const Map& m,int tx,int tz,const std::vector<uint8_t>* live=nullptr) const {
        if(tx<0||tz<0||tx>=m.width*2||tz>=m.height*2)throw std::runtime_error("Tile outside map");
        const auto& blocks=live?*live:m.block_data;
        int b=blocks.at((tz/4)*(m.width/2)+tx/4);
        return rom.byte(tilesets.at(m.tileset).blocks+b*16+(tz%4)*4+tx%4);
    }
};
} // namespace kanto
