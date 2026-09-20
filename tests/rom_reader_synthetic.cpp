#include "synthetic_rom.h"
#include <cstdio>

// Exercises kanto::Rom / kanto::World against the procedural image, so the
// reader is covered on machines that do not own the cartridge.
static void check(bool condition,const char* message) {
    if(!condition)throw std::runtime_error(message);
}
int main() {
    try {
        const auto& plan=synthetic::layout();
        auto image=synthetic::rom();
        check(image.size()==kanto::RomSize,"generator must emit exactly 1 MiB");
        uint8_t header_sum=0;
        for(size_t at=0x134;at<=0x14c;at++)header_sum=uint8_t(header_sum-image[at]-1);
        check(header_sum==image[0x14d],"cartridge header checksum");
        check(image[0x147]==0x1b&&image[0x148]==0x05,"cartridge type and 1 MiB size code");

        kanto::Rom rom(image.data(),image.size());
        kanto::World world(rom);
        check(world.connected_count==plan.connected_count,"home plus two neighbours are connected");
        check(world.maps.size()==plan.map_count,"forest and dock are always instantiated");
        for(int id:{plan.home,plan.north,plan.east,plan.forest,plan.dock})check(world.find(id),"expected map missing");
        check(!world.find(plan.interior),"the interior is not resident until a warp asks for it");

        // Connections and the origins the breadth-first walk derives from them.
        const auto& home=*world.find(plan.home);
        check(home.origin_x==0&&home.origin_z==0,"home map anchors the component");
        check(home.width==plan.home_width&&home.height==plan.home_height,"home dimensions");
        check(home.component==0&&!home.interior,"home is an outdoor component member");
        check(home.connections.size()==2,"home has a northern and an eastern exit");
        check(home.connections[0].direction=='N'&&home.connections[0].map==plan.north,"northern exit");
        check(home.connections[0].x_alignment==plan.home_north_x_alignment,"northern alignment");
        check(home.connections[1].direction=='E'&&home.connections[1].map==plan.east,"eastern exit");
        check(home.connections[1].y_alignment==plan.home_east_y_alignment,"eastern alignment");
        const auto& north=*world.find(plan.north);
        const auto& east=*world.find(plan.east);
        check(north.origin_x==plan.north_origin_x&&north.origin_z==plan.north_origin_z,"northern origin");
        check(east.origin_x==plan.east_origin_x&&east.origin_z==plan.east_origin_z,"eastern origin");
        check(north.origin_z==-north.height,"the northern map sits exactly one map height above");
        check(east.origin_x==home.width,"the eastern map starts at the home map's right edge");
        // The cycle: the eastern map is reached both from the home map and
        // from the northern one, and both paths must agree.
        check(north.connections.size()==2&&north.connections[1].map==plan.east,"cycle edge 12 -> 1");
        check(east.connections.size()==1&&east.connections[0].map==plan.home,"return edge 1 -> 0");
        check(world.find(plan.forest)->component==plan.forest,"forest is its own component");
        check(world.find(plan.dock)->component==plan.dock,"dock is its own component");

        // Objects of the home map.
        check(home.warps.size()==2,"home warp count");
        check(home.warps[0].x==plan.home_warp.x&&home.warps[0].z==plan.home_warp.z,"home warp position");
        check(home.warps[0].destination==plan.interior&&home.warps[0].entrance==plan.home_warp.entrance,"home warp target");
        check(home.warps[1].destination==plan.home_dynamic_warp.destination,"dynamic warp sentinel");
        check(kanto::dynamic_warp(home,home.warps[1]),"the sentinel warp is not a room");
        check(home.signs.size()==1&&home.signs[0].x==plan.home_sign.x&&home.signs[0].z==plan.home_sign.z,"home sign");
        check(home.signs[0].text==plan.home_sign.text,"home sign text id");
        check(home.actors.size()==2,"home actor count");
        check(home.actors[0].x==plan.home_actor.x&&home.actors[0].z==plan.home_actor.z,"first actor position");
        check(home.actors[1].x==plan.home_item_actor.x&&home.actors[1].z==plan.home_item_actor.z,
              "second actor position after the one-byte item tail");
        check(home.actors[1].text==plan.home_item_actor.text,"item actor text byte");

        // ensure_map() pulls the interior in on demand. A separate world keeps
        // it out of the set discover_warps() seeds at depth zero below.
        kanto::World lazy(rom);
        const auto& probe=lazy.ensure_map(rom,plan.interior);
        check(probe.interior&&probe.component==plan.interior,"interior component");
        check(probe.origin_x==0&&probe.origin_z==0,"interiors are not placed in the overworld");
        check(probe.tileset==plan.interior_tileset,"interior tileset");
        check(probe.width==plan.interior_width&&probe.height==plan.interior_height,"interior dimensions");
        check(probe.warps.size()==1&&probe.warps[0].destination==plan.home,"the interior warps back home");
        check(lazy.maps.size()==plan.map_count+1,"ensure_map adds exactly one map");

        // The warp graph reaches the same interior at depth 1.
        auto depths=world.discover_warps(rom);
        const auto& house=*world.find(plan.interior);
        check(depths.at(plan.interior)==plan.interior_depth,"interior is one warp away");
        check(depths.at(plan.home)==0&&depths.at(plan.dock)==0,"resident maps are at depth zero");
        check(depths.size()==plan.map_count+1,"the sentinel warp adds no map");
        check(world.tilesets.size()==4,"outdoor, interior, forest and dock tilesets");
        check(world.ledges.size()==plan.ledge_count,"ledge table entries");
        check(world.pairs.size()==plan.pair_count,"land and water tile pairs");
        check(world.pairs[plan.water_pair_index].water,"the second table is the water one");
        check(world.tilesets.at(plan.outdoor_tileset).grass==plan.grass_bottom,"grass tile from the tileset header");
        check(world.tilesets.at(plan.outdoor_tileset).walkable[plan.ground_bottom],"ground is walkable");
        check(!world.tilesets.at(plan.outdoor_tileset).walkable[plan.tree_bottom],"trees are not walkable");
        check(world.tilesets.at(plan.interior_tileset).walkable[plan.floor_bottom],"interior floor is walkable");

        // Tile sampling: top row and second row of every probed cell.
        auto tile=[&](const kanto::Map& m,synthetic::Cell c,int row){return world.tile(rom,m,c.x*2,c.z*2+row);};
        check(tile(home,plan.ground,0)==plan.ground_top&&tile(home,plan.ground,1)==plan.ground_bottom,"ground tiles");
        check(tile(home,plan.grass,1)==plan.grass_bottom,"grass tile");
        check(tile(home,plan.tree,0)==plan.tree_top&&tile(home,plan.tree,1)==plan.tree_bottom,"tree tiles");
        check(tile(home,plan.rock,0)==plan.rock_top&&tile(home,plan.rock,1)==plan.rock_bottom,"rock tiles");
        check(tile(home,plan.ledge,1)==plan.ledge_bottom,"ledge tile");
        check(tile(home,plan.water,1)==plan.water_bottom,"water tile");
        check(tile(home,plan.fence,0)==plan.fence_top&&tile(home,plan.fence,1)==plan.fence_bottom,"fence tiles");
        check(tile(home,plan.signpost,0)==plan.sign_top&&tile(home,plan.signpost,1)==plan.sign_bottom,"sign tiles");
        check(tile(home,plan.cut_tree,0)==plan.cut_top&&tile(home,plan.cut_tree,1)==plan.cut_bottom,"cut tree tiles");
        check(tile(east,plan.east_grass,1)==plan.grass_bottom,"grass on the eastern neighbour");
        check(tile(house,plan.interior_floor,1)==plan.floor_bottom,"interior floor tile");
        check(tile(house,plan.interior_wall,0)==plan.wall_top,"interior wall tile");
        check(tile(house,plan.interior_furniture,0)==plan.furniture_top,"interior furniture tile");
        bool outside=false;
        try {world.tile(rom,home,home.width*2,0);} catch(const std::exception&) {outside=true;}
        check(outside,"sampling past the eastern edge is refused");

        // Malformed images. Each mutation starts from a fresh copy.
        auto reject=[&](std::vector<uint8_t> bad,const char* why) {
            bool rejected=false;
            try {kanto::World w(kanto::Rom(bad.data(),bad.size()));} catch(const std::exception&) {rejected=true;}
            check(rejected,why);
        };
        auto bad=image;bad.resize(512);reject(bad,"a truncated image must be refused");
        bad=image;bad.push_back(0);reject(bad,"an oversized image must be refused");
        bad=image;bad[home.header+4]=0xff;reject(bad,"a block pointer outside the bank window must be refused");
        bad=image;bad[plan.outdoor_block_table]=96;reject(bad,"a block holding a tile past the sheet must be refused");
        auto at=[](size_t offset){return std::ptrdiff_t(offset);};
        bad=image;std::fill(bad.begin()+at(plan.ledge_table),bad.begin()+at(plan.ledge_table)+256,0);
        reject(bad,"an unterminated ledge table must be refused");
        // The northern map's flags are S|E, so its eastern entry is the second
        // one; moving its y alignment breaks the cycle the home map already fixed.
        bad=image;bad[world.find(plan.north)->header+10+11+7]++;
        reject(bad,"a contradictory connection cycle must be refused");

        std::fprintf(stderr,"PASS: %zu connected maps, cycle origins, warps to depth %d, tiles and six malformed images\n",
                     world.connected_count,plan.interior_depth);
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
