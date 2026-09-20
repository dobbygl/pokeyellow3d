#pragma once
#include "pallet_state.h"
#include "synthetic_rom.h"
#include <array>
#include <cstdint>
#include <vector>

// A GBContext assembled by hand around tests/synthetic_rom.h. The procedural
// image is never executed: it is only published as ctx.rom so the read-only
// state helpers (pallet::view, pallet::actor, battle::*) have a cartridge to
// decode. No recompiled game function and no gb_context_create is involved, so
// ctx.ppu stays null; GBContext has no framebuffer member of its own and the
// LCD buffer below belongs to this fixture alone.
namespace synthetic {

// A block id that no map in the image uses. Its first tile is pushed past the
// 96-tile sheet so kanto::Tileset::valid_blocks rejects it, which is the only
// way to make pallet::valid_live_map fail: every block the generator writes is
// deliberately valid.
constexpr uint8_t InvalidBlock = 200;

// Procedural 56x56 portrait, as 2bpp colour indices. A body outline open at
// the bottom edge, an enclosed white pocket, and two shaded patches. The open
// bottom is what battle::portrait's back-picture crop has to close, so the
// same tiles decode differently for the front and the back rectangle.
inline int portrait_index(int x, int y) {
    if (x < 8 || x > 47 || y < 8) return 0;                    // outside the body
    if (x == 8 || x == 47 || y == 8) return 3;                 // outline, bottom left open
    if (x >= 16 && x <= 23 && y >= 16 && y <= 23)              // enclosed pocket
        return (x == 16 || x == 23 || y == 16 || y == 23) ? 3 : 0;
    if (x >= 30 && x <= 39 && y >= 28 && y <= 37) return 2;    // dark patch
    if (x >= 12 && x <= 43 && y >= 42 && y <= 50) return 1;    // light patch
    return 0;                                                  // white body fill
}

struct Context {
    std::vector<uint8_t> image;
    std::array<uint8_t, 0x2000> wram{};   // C000-DFFF
    std::array<uint8_t, 0x2000> vram{};   // 8000-9FFF
    std::array<uint8_t, 0x2000> eram{};   // A000-BFFF
    std::array<uint8_t, 0xa0> oam{};
    std::array<uint8_t, 0x80> hram{};     // FF80-FFFE, one spare byte
    std::array<uint8_t, 0x80> io{};       // FF00-FF7F
    std::array<uint32_t, 160 * 144> framebuffer{};
    GBContext ctx{};

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Context() : image(rom()) {
        // Poison one unused block in both block tables; see InvalidBlock.
        image[detail::OutdoorBlocks + size_t(InvalidBlock) * 16] = 96;
        image[detail::IndoorBlocks + size_t(InvalidBlock) * 16] = 96;
        ctx.rom = image.data();
        ctx.rom_size = kanto::RomSize;
        ctx.wram = wram.data();
        ctx.vram = vram.data();
        ctx.eram = eram.data();
        ctx.eram_size = eram.size();
        ctx.oam = oam.data();
        ctx.hram = hram.data();
        ctx.io = io.data();
        reset();
    }

    // --- raw accessors -----------------------------------------------------
    uint8_t& at(int address) { return wram[size_t(address - 0xc000)]; }
    void write(int address, int value) { at(address) = uint8_t(value); }
    // Battle HP and stat words are big endian, as battle::word reads them.
    void write_hp(int address, int value) {
        at(address) = uint8_t((value >> 8) & 0xff);
        at(address + 1) = uint8_t(value & 0xff);
    }
    void tile(int x, int y, int value) { at(0xc3a0 + y * 20 + x) = uint8_t(value); }

    // --- LCD ---------------------------------------------------------------
    // Bit 4 keeps tile data at 8000 so battle::portrait indexes VRAM directly;
    // bit 5 stays clear so the battle UI is read from the BG map at 9800.
    void lcd_on(bool on = true) { io[0x40] = uint8_t(on ? (io[0x40] | 0x80) : (io[0x40] & ~0x80)); }
    void bgp(int value) { io[0x47] = uint8_t(value); }
    void set_font(bool loaded) { write(pallet::Font, loaded ? 1 : 0); }

    void reset() {
        wram.fill(0);
        vram.fill(0);
        eram.fill(0);
        oam.fill(0);
        hram.fill(0);
        io.fill(0);
        framebuffer.fill(0xffe0f8d0);
        ctx.sp = 0xdf00;
        io[0x40] = 0x91;  // LCD on, tile data at 8000, BG enabled, window off
        io[0x42] = 0;     // SCY
        io[0x43] = 0;     // SCX
        io[0x4a] = 0;     // WY
        io[0x4b] = 7;     // WX
        bgp(0xe4);
    }

    // --- overworld ---------------------------------------------------------
    // wOverworldMap keeps a three-block border on every side.
    uint8_t& live_block(const pallet::Scene& s, int x, int z) {
        return at(0xc6e8 + (z + 3) * (s.width / 2 + 6) + x + 3);
    }
    void write_live_map(const pallet::Scene& s) {
        const int bw = s.width / 2, bh = s.height / 2;
        for (int z = 0; z < bh + 6; z++)
            for (int x = 0; x < bw + 6; x++) at(0xc6e8 + z * (bw + 6) + x) = uint8_t(s.border);
        for (int z = 0; z < bh; z++)
            for (int x = 0; x < bw; x++) live_block(s, x, z) = s.block_data[size_t(z * bw + x)];
    }
    // Header bytes, sprite slot and live map of a standing player.
    const pallet::Scene* place_player(int map, int x, int y, int facing) {
        pallet::load_catalog(ctx.rom, ctx.rom_size);
        const auto* s = pallet::ensure_scene(map);
        write(pallet::Map, map);
        write(pallet::X, x);
        write(pallet::Y, y);
        write(pallet::Walk, 0);
        write(pallet::Font, 0);
        write(pallet::UpdateSprites, 1);
        write(pallet::Sprite1, 1);      // slot in use
        write(pallet::Sprite1 + 2, 0);  // on-screen image index
        write(pallet::Sprite1 + 4, 0x3c);
        write(pallet::Sprite1 + 6, 0x40);
        write(0xc109, facing);
        write(pallet::HiddenList, 255);  // empty hidden-object list
        if (s) {
            write(pallet::Tileset, s->tileset);
            write(pallet::Width, s->width / 2);
            write(pallet::Height, s->height / 2);
            write_live_map(*s);
        }
        return s;
    }

    // The exact six-row full-width text box pallet::bottom_dialogue accepts.
    void open_bottom_dialogue() {
        for (int y = 0; y < 12; y++)
            for (int x = 0; x < 20; x++) tile(x, y, 0);
        tile(0, 12, 0x79);
        tile(19, 12, 0x7b);
        tile(0, 17, 0x7d);
        tile(19, 17, 0x7e);
        for (int x = 1; x < 19; x++) {
            tile(x, 12, 0x7a);
            tile(x, 17, 0x7a);
        }
        for (int y = 13; y < 17; y++) {
            tile(0, y, 0x7c);
            tile(19, y, 0x7c);
            for (int x = 1; x < 19; x++) tile(x, y, 0x80 + (x * 3 + y * 7) % 26);
        }
    }

    // --- battle ------------------------------------------------------------
    void write_portrait_tile(int id, int column, int row) {
        for (int ty = 0; ty < 8; ty++) {
            uint8_t low = 0, high = 0;
            for (int tx = 0; tx < 8; tx++) {
                int v = portrait_index(column * 8 + tx, row * 8 + ty);
                if (v & 1) low = uint8_t(low | (0x80 >> tx));
                if (v & 2) high = uint8_t(high | (0x80 >> tx));
            }
            vram[size_t(id) * 16 + size_t(ty) * 2] = low;
            vram[size_t(id) * 16 + size_t(ty) * 2 + 1] = high;
        }
    }
    // Seven columns of seven consecutive tile ids, plus the matching BG map
    // entries battle::rectangle(displayed) demands and the tile bitmaps.
    void write_rectangle(battle::Rect r, bool displayed = true) {
        for (int y = 0; y < 7; y++)
            for (int x = 0; x < 7; x++) {
                int id = r.base + x * 7 + y;
                tile(r.x + x, r.y + y, id);
                if (displayed) vram[size_t(0x1800 + (r.y + y) * 32 + r.x + x)] = uint8_t(id);
                write_portrait_tile(id, x, y);
            }
    }
    // Uppercase name in WRAM plus the HUD glyphs at the column CenterMonName
    // shifts short names to.
    void write_name(int address, const char* text, int x, int y) {
        int length = 0;
        while (text[length]) ++length;
        for (int i = 0; i < length; i++) write(address + i, 0x80 + (text[i] - 'A'));
        write(address + length, 0x50);
        int shift = length <= 2 ? 2 : length <= 4 ? 1 : 0;
        for (int i = 0; i < length; i++) tile(x + shift + i, y, 0x80 + (text[i] - 'A'));
    }
    void clear_names() {
        for (int x = 0; x < 10; x++) tile(x, 0, 0);
        for (int x = 8; x < 20; x++) tile(x, 7, 0);
    }
    // Everything battle::ready checks outside the LCD registers: a wild battle
    // with both HUD names, both HP words and both portrait rectangles.
    void start_battle(int enemy_species, int player_species,
                      const char* enemy_name = "PIKACH", const char* player_name = "BULBAS") {
        write(battle::IsInBattle, 1);
        write(battle::BattleType, 0);
        write(battle::Link, 0);
        write(battle::Animation, 0);
        write(0xd11c, 0);   // no full-screen list over the arena
        write(0xd030, 0);   // no trainer engaged
        write(0xcfe7, 0);   // enemy party position, FF only during the intro
        write(0xcfe4, enemy_species);
        write_hp(0xcfe5, 18);
        write_hp(0xcff3, 24);
        write(0xcfe8, 0);   // enemy status
        write(0xcff2, 7);   // enemy level
        write(0xcf1d, 1);   // enemy HP bar colour
        write(0xd013, player_species);
        write_hp(0xd014, 26);
        write_hp(0xd022, 31);
        write(0xd017, 0);   // player status
        write(0xd021, 9);   // player level
        write(0xcf1c, 1);   // player HP bar colour
        write_rectangle(battle::Enemy);
        write_rectangle(battle::Player);
        write_name(0xcfd9, enemy_name, 1, 0);
        write_name(0xd008, player_name, 10, 7);
    }
    // Writes the CALL battle::live_return looks for and its return address on
    // the emulated stack, exactly where the scan from SP starts.
    void arm_live_return(size_t call, int target) {
        image[call] = 0xcd;
        image[call + 1] = uint8_t(target & 0xff);
        image[call + 2] = uint8_t((target >> 8) & 0xff);
        int address = int(call % 0x4000) + 0x4003;
        write(ctx.sp, address & 0xff);
        write(ctx.sp + 1, (address >> 8) & 0xff);
    }
    void disarm_live_return(size_t call) {
        image[call] = image[call + 1] = image[call + 2] = 0;
        write(ctx.sp, 0);
        write(ctx.sp + 1, 0);
    }
};
}  // namespace synthetic
