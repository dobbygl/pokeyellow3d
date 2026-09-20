#include "synthetic_context.h"
#include <cmath>
#include <utility>
#include <cstdio>

// Drives pallet::view and the actor/player readers over the hand-built
// GBContext of tests/synthetic_context.h, so the view selection is covered on
// machines without the cartridge. Every branch of view() is reached by editing
// one byte of the fixture at a time.
static void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
static bool close_to(float value, float expected) {
    return std::fabs(value - expected) < 1e-4f;
}

using V = pallet::View;

int main() {
    try {
        const auto &plan = synthetic::layout();
        synthetic::Context machine;
        GBContext *ctx = &machine.ctx;

        // --- Overworld ------------------------------------------------------
        machine.reset();
        const auto *home = machine.place_player(plan.home, 4, 4, 0);
        check(home && home->id == plan.home, "the home map is resident");
        check(pallet::view(ctx) == V::Overworld, "a clean standing state is the overworld");

        // --- Unsupported ----------------------------------------------------
        ctx->rom_size = 512 * 1024;
        check(pallet::view(ctx) == V::Unsupported, "only a 1 MiB image is supported");
        ctx->rom_size = kanto::RomSize;
        check(pallet::view(ctx) == V::Overworld, "restoring the size restores the view");

        machine.write(pallet::Map, 255);
        check(pallet::view(ctx) == V::Unsupported, "a map outside the catalog is unsupported");
        machine.write(pallet::Map, plan.home);

        machine.write(pallet::Tileset, plan.forest_tileset);
        check(pallet::view(ctx) == V::Unsupported, "a tileset byte that contradicts the header");
        machine.write(pallet::Tileset, home->tileset);

        machine.write(pallet::X, home->width);
        check(pallet::view(ctx) == V::Unsupported, "a player past the eastern edge");
        machine.write(pallet::X, 4);
        check(pallet::view(ctx) == V::Overworld, "the overworld is back");

        // --- Transition -----------------------------------------------------
        machine.lcd_on(false);
        check(pallet::view(ctx) == V::Transition, "an off LCD is a transition");
        machine.lcd_on(true);

        machine.bgp(0xff);
        check(pallet::view(ctx) == V::Transition, "a faded palette is a transition");
        machine.bgp(0xe4);

        machine.write(pallet::UpdateSprites, 0);
        check(pallet::view(ctx) == V::Transition, "sprite updates disabled is a transition");
        machine.write(pallet::UpdateSprites, 1);

        machine.write(pallet::Sprite1, 0);
        check(pallet::view(ctx) == V::Transition, "an empty player slot is a transition");
        machine.write(pallet::Sprite1, 1);

        uint8_t block = machine.live_block(*home, 1, 1);
        machine.live_block(*home, 1, 1) = synthetic::InvalidBlock;
        check(pallet::view(ctx) == V::Transition, "a live map holding an invalid block");
        machine.live_block(*home, 1, 1) = block;
        check(pallet::view(ctx) == V::Overworld, "the restored live map is valid again");

        // --- Dialogue -------------------------------------------------------
        machine.set_font(true);
        check(pallet::view(ctx) == V::Dialogue, "the font flag opens a dialogue");
        machine.set_font(false);

        machine.tile(7, 3, 0x60);
        check(pallet::view(ctx) == V::Dialogue, "a text tile anywhere opens a dialogue");
        machine.tile(7, 3, 0);
        check(pallet::view(ctx) == V::Overworld, "clearing the text tile closes the dialogue");

        check(!pallet::bottom_dialogue(ctx), "an overworld has no text box");
        machine.open_bottom_dialogue();
        check(pallet::view(ctx) == V::Dialogue, "the text box is a dialogue");
        check(pallet::bottom_dialogue(ctx), "the complete frame is composable");
        for (auto corner :
             {std::pair<int, int>{0, 12}, {19, 12}, {0, 17}, {19, 17}, {5, 12}, {0, 14}}) {
            uint8_t saved = machine.at(0xc3a0 + corner.second * 20 + corner.first);
            machine.tile(corner.first, corner.second, 0x7f);
            check(!pallet::bottom_dialogue(ctx), "an incomplete frame is not composable");
            check(pallet::view(ctx) == V::Dialogue, "a broken frame is still a dialogue");
            machine.tile(corner.first, corner.second, saved);
        }
        check(pallet::bottom_dialogue(ctx), "the frame is whole again");
        machine.tile(3, 4, 0x60);
        check(!pallet::bottom_dialogue(ctx), "text above the box rules out the bottom frame");

        // --- Battle ---------------------------------------------------------
        machine.reset();
        machine.place_player(plan.home, 4, 4, 0);
        machine.start_battle(25, 1);
        check(pallet::view(ctx) == V::Battle, "a ready battle is the battle view");
        machine.write(pallet::Map, 255);
        check(pallet::view(ctx) == V::Unsupported, "a battle on an unknown map is unsupported");
        machine.write(pallet::Map, plan.home);
        machine.bgp(0xff);
        check(pallet::view(ctx) == V::Unsupported, "a battle that is not ready is unsupported");
        machine.bgp(0xe4);
        check(pallet::view(ctx) == V::Battle, "the battle is ready again");

        // --- player() interpolation ------------------------------------------
        machine.reset();
        machine.place_player(plan.home, 3, 5, 0);
        machine.write(pallet::Sprite1 + 3, uint8_t(int8_t(-2))); // northward step
        machine.write(pallet::Sprite1 + 5, 2);                   // eastward step
        machine.write(pallet::Walk, 0);
        auto p = pallet::player(ctx);
        check(close_to(p[0], 3.5f) && close_to(p[1], 5.5f),
              "an idle player sits on the cell centre");
        machine.write(pallet::Walk, 8);
        p = pallet::player(ctx);
        check(close_to(p[0], 3.5f) && close_to(p[1], 5.5f), "a step that has not advanced yet");
        machine.write(pallet::Walk, 4);
        p = pallet::player(ctx);
        check(close_to(p[0], 4.5f) && close_to(p[1], 4.5f), "half a step interpolates both deltas");
        machine.write(pallet::Walk, 0);

        // --- actor() ----------------------------------------------------------
        pallet::Actor a{};
        check(pallet::actor(ctx, 0, a), "the player slot is an actor");
        check(a.slot == 0 && close_to(a.x, 3.5f) && close_to(a.z, 5.5f),
              "the player actor follows player()");
        machine.write(pallet::Sprite1, 0);
        check(!pallet::actor(ctx, 0, a), "an empty slot is no actor");
        machine.write(pallet::Sprite1, 1);

        // Slot 1 is on screen: its position comes from the screen offsets.
        machine.write(0xc110, 1);
        machine.write(0xc112, 0x10);
        machine.write(0xc114, 0x2c); // 16 pixels north of the player
        machine.write(0xc116, 0x50); // 16 pixels east of the player
        check(pallet::actor(ctx, 1, a), "an on-screen NPC is an actor");
        check(a.slot == 1 && a.image == 0x10, "the on-screen image index is used as is");
        check(close_to(a.x, 4.5f) && close_to(a.z, 4.5f), "screen offsets place the on-screen NPC");

        // Slot 2 is off screen: map coordinates and the sprite base rebuild it.
        machine.write(0xc120, 1);
        machine.write(0xc122, 255);
        machine.write(0xc129, 4);
        machine.write(0xc224, 6);  // z = 2
        machine.write(0xc225, 11); // x = 7
        machine.write(0xc22e, 3);  // sprite base
        check(pallet::actor(ctx, 2, a), "an off-screen NPC is still an actor");
        check(a.slot == 2 && close_to(a.x, 7.5f) && close_to(a.z, 2.5f),
              "off-screen map coordinates");
        check(a.image == 0x24, "the off-screen image is rebuilt from the sprite base");
        machine.write(0xc22e, 12);
        check(!pallet::actor(ctx, 2, a), "a sprite base outside 1..11 is refused");
        machine.write(0xc22e, 3);
        machine.write(0xc225, 4 + home->width);
        check(!pallet::actor(ctx, 2, a), "an NPC past the map edge is refused");
        machine.write(0xc225, 11);

        // A hidden-object flag removes an otherwise visible NPC.
        machine.write(pallet::HiddenList, 1);
        machine.write(pallet::HiddenList + 1, 9);
        machine.write(pallet::HiddenList + 2, 255);
        machine.write(pallet::HiddenFlags + 1, 1 << 1);
        check(!pallet::actor(ctx, 1, a), "a hidden object is not drawn");
        machine.write(pallet::HiddenFlags + 1, 0);
        check(pallet::actor(ctx, 1, a), "clearing the flag brings it back");

        // --- world_player() ----------------------------------------------------
        machine.reset();
        const auto *north = machine.place_player(plan.north, 2, 3, 0);
        check(north && north->origin_x == plan.north_origin_x &&
                  north->origin_z == plan.north_origin_z,
              "the northern scene keeps the origin the catalog derived");
        check(pallet::view(ctx) == V::Overworld, "the northern neighbour is a valid overworld");
        auto local = pallet::player(ctx);
        auto world = pallet::world_player(ctx);
        check(close_to(local[0], 2.5f) && close_to(local[1], 3.5f),
              "local coordinates stay map relative");
        check(close_to(world[0], 2.5f + plan.north_origin_x) &&
                  close_to(world[1], 3.5f + plan.north_origin_z),
              "world coordinates add the scene origin");

        std::fprintf(stderr,
                     "PASS: five view states, the six-tile text frame, walk interpolation, three "
                     "actor kinds "
                     "and the scene origin at (%d,%d)\n",
                     plan.north_origin_x, plan.north_origin_z);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
