#pragma once
#include "battle_state.h"
#include "menu_text.h"
#include "mon_pic_cache.h"

// Read-only presentation of StatusScreen/StatusScreen2 and the party menu.
// Layouts, CALL sites and graphic profiles are verified against pret/pokeyellow
// e89ead154b9968aa50eed9328ff2b38b6c194382 and the generated WRAM definitions.
namespace pokemon_menu {
enum class Kind { None, Party, Actions, Stats, Moves };
enum class Cell { Text, Graphic, Decoration, Portrait, Health };
struct Bar {
    int x = 0, y = 0, pixels = 0, color = 0;
};
struct Snapshot {
    Kind kind = Kind::None;
    bool valid = false;
    int species = 0, selected = 0, count = 0;
    int experience = 0;
    float experience_fraction = 0;
    std::array<uint8_t, 360> text{};
    std::array<Cell, 360> cells{};
    menu_layout::Layout layout;
    std::array<Bar, 6> bars{};
    size_t bar_count = 0;
};
inline bool ready(const GBContext *ctx) {
    if (!ctx || !ctx->wram || !ctx->vram || !ctx->io || !ctx->rom || ctx->rom_size != 1048576 ||
        (ctx->io[0x40] & 0x90) != 0x80 || ctx->io[0x47] != 0xe4)
        return false;
    // The full-screen window hides the overworld's still-scrolled background.
    bool window = (ctx->io[0x40] & 0x20) && !ctx->io[0x4a] && ctx->io[0x4b] == 7;
    return window || (!ctx->io[0x42] && !ctx->io[0x43]);
}
inline bool party_return(const GBContext *ctx) {
    // HandlePartyMenuInput is in the fixed bank. battle::live_return is for
    // banked callers, so its +4000 return-address normalization does not apply.
    if (ctx->rom[0x1230] != 0xcd || ctx->rom[0x1231] != 0xaf || ctx->rom[0x1232] != 0x3a ||
        ctx->sp < 0xd000 || ctx->sp >= 0xe000)
        return false;
    for (int a = ctx->sp; a < 0xdfff; a += 2)
        if (battle::read(ctx, a) == 0x33 && battle::read(ctx, a + 1) == 0x12)
            return true;
    return false;
}
inline Kind context(const GBContext *ctx) {
    if (!ready(ctx))
        return Kind::None;
    if (battle::live_return(ctx, 0x1161d, 0x3852))
        return Kind::Stats;
    if (battle::live_return(ctx, 0x11814, 0x3852))
        return Kind::Moves;
    // StartMenu_Pokemon and PartyMenuOrRockOrRun share the three-option
    // action geometry; their original strings retain their different order.
    if (battle::live_return(ctx, 0x11c95, 0x3aab) ||
        (battle::normal(ctx) && battle::live_return(ctx, 0x3d236, 0x3aab)))
        return Kind::Actions;
    return party_return(ctx) ? Kind::Party : Kind::None;
}
struct Graphic {
    size_t address = 0;
    bool one_bit = false;
};
inline Graphic graphic(Kind kind, uint8_t tile) {
    bool summary = kind == Kind::Stats || kind == Kind::Moves;
    if (summary) {
        if (tile >= 0x6d && tile <= 0x6f)
            return {0x10c00 + size_t(tile - 0x6d) * 8, true};
        if (tile == 0x78)
            return {0x10c18, true};
        if (tile == 0x76 || tile == 0x77)
            return {0x10c30 + size_t(tile - 0x76) * 8, true};
        if (tile == 0x72)
            return {0x11682, true};
    }
    if (tile >= 0x62 && tile <= 0x74)
        return {0x10a20 + size_t(tile - 0x62) * 16, false};
    return {};
}
inline bool graphic_matches(const GBContext *ctx, Kind kind, uint8_t tile) {
    auto source = graphic(kind, tile);
    if (!source.address)
        return false;
    for (int row = 0; row < 8; ++row)
        for (int plane = 0; plane < 2; ++plane) {
            size_t offset = size_t(row) * (source.one_bit ? 1 : 2) + (source.one_bit ? 0 : plane);
            if (ctx->vram[0x1000 + int(tile) * 16 + row * 2 + plane] !=
                ctx->rom[source.address + offset])
                return false;
        }
    return true;
}
inline bool border(const uint8_t *tiles, menu_layout::Rect r,
                   menu_layout::Rect cover = {0, 0, 0, 0}) {
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) {
            if (menu_layout::contains(cover, x, y))
                continue;
            auto expected = menu_layout::border(r, x, y);
            if (expected && tiles[y * 20 + x] != expected)
                return false;
        }
    return true;
}
inline bool health(const GBContext *ctx, Snapshot &out, int x, int y, uint8_t end, int color) {
    const auto *tiles = ctx->wram + 0x3a0;
    if (tiles[y * 20 + x - 2] != 0x71 || tiles[y * 20 + x - 1] != 0x62 ||
        tiles[y * 20 + x + 6] != end || color > 2 || out.bar_count == out.bars.size())
        return false;
    int pixels = 0;
    bool partial = false;
    for (int i = 0; i < 6; ++i) {
        int tile = tiles[y * 20 + x + i];
        if (tile < 0x63 || tile > 0x6b || (partial && tile != 0x63))
            return false;
        partial |= tile < 0x6b;
        pixels += tile - 0x63;
        out.cells[y * 20 + x + i] = Cell::Health;
    }
    out.cells[y * 20 + x + 6] = Cell::Health;
    out.bars[out.bar_count++] = {x, y, pixels, color};
    return true;
}
inline bool experience(const GBContext *ctx, Snapshot &out) {
    int slot = battle::read(ctx, 0xcf91), location = battle::read(ctx, 0xcc49), record = 0;
    switch (location) {
    case 0:
        if (slot >= battle::read(ctx, 0xd162) || slot >= 6)
            return false;
        record = 0xd16a + slot * 44;
        break;
    case 1:
        if (slot >= battle::read(ctx, 0xd89b) || slot >= 6)
            return false;
        record = 0xd8a3 + slot * 44;
        break;
    case 2:
        if (slot >= battle::read(ctx, 0xda7f) || slot >= 20)
            return false;
        record = 0xda95 + slot * 33;
        break;
    case 3:
        record = 0xda5e;
        break;
    default:
        return false;
    }
    if (battle::read(ctx, record) != out.species)
        return false;
    int level = battle::read(ctx, 0xcfb8);
    if (!level || level > 100)
        return false;
    // CalcExpToLevelUp overwrites wLoadedMonExp on page two. The persistent
    // source record holds total experience on both pages.
    out.experience = (battle::read(ctx, record + 14) << 16) |
                     (battle::read(ctx, record + 15) << 8) | battle::read(ctx, record + 16);
    int low = battle::experience_at(ctx, out.species, level);
    int high = battle::experience_at(ctx, out.species, std::min(level + 1, 100));
    if (low < 0 || high < low || out.experience < low)
        return false;
    out.experience_fraction = level == 100 ? 1.f
                              : high > low
                                  ? std::clamp(float(out.experience - low) / (high - low), 0.f, 1.f)
                                  : 0.f;
    return true;
}
inline bool graphic_position(const Snapshot &out, int x, int y, uint8_t tile) {
    if (out.kind == Kind::Stats || out.kind == Kind::Moves) {
        if (x == 1 && y == 7 && tile == 0x74)
            return true;
        if (out.kind == Kind::Stats)
            return (x == 14 && y == 2 && tile == 0x6e) || (x == 11 && y == 3 && tile == 0x71) ||
                   (x == 12 && y == 3 && tile == 0x62) || (x == 10 && y == 13 && tile == 0x73) ||
                   (x == 11 && y == 13 && tile == 0x74);
        return (x == 14 && y == 6 && tile == 0x70) || (x == 16 && y == 6 && tile == 0x6e) ||
               ((x == 11 || x == 12) && y >= 10 && y <= 16 && !(y & 1) && tile == 0x72);
    }
    if (y >= out.count * 2)
        return false;
    return (!(y & 1) && x == 13 && tile == 0x6e) ||
           ((y & 1) &&
            ((x == 4 && tile == 0x71) || (x == 5 && tile == 0x62) ||
             (x >= 6 && x < 12 && tile >= 0x63 && tile <= 0x6b) || (x == 12 && tile == 0x6c)));
}
inline bool icons_valid(const GBContext *ctx, int count) {
    if (!ctx->oam || !(ctx->io[0x40] & 2) || (ctx->io[0x40] & 4))
        return false;
    for (int i = 0; i < 40; ++i) {
        const auto *sprite = ctx->oam + i * 4;
        if (i >= count * 4) {
            if (sprite[0] && sprite[0] < 160 && sprite[1] && sprite[1] < 168)
                return false;
            continue;
        }
        int row = i / 4, part = i % 4;
        int y = sprite[0] - 16, x = sprite[1] - 8;
        if (x != 8 + (part % 2) * 8 || y < row * 16 + (part / 2) * 8 ||
            y > row * 16 + (part / 2) * 8 + 1 || (sprite[3] & ~0x70))
            return false;
        // MonPartySpritePointers: 30 source/length/bank/VRAM records used by
        // LoadMonPartySpriteGfx. Both animation frames come from these assets.
        bool matched = false;
        for (int entry = 0; entry < 30; ++entry) {
            const auto *p = ctx->rom + 0x7184d + entry * 6;
            int destination = p[4] | (p[5] << 8), address = 0x8000 + sprite[2] * 16;
            if (address < destination || address >= destination + p[2] * 16)
                continue;
            int pointer = p[0] | (p[1] << 8);
            size_t source = size_t(p[3]) * 0x4000 + (pointer & 0x3fff) + address - destination;
            if (pointer < 0x4000 || pointer >= 0x8000 || source + 16 > ctx->rom_size)
                return false;
            matched = std::memcmp(ctx->rom + source, ctx->vram + sprite[2] * 16, 16) == 0;
            break;
        }
        if (!matched)
            return false;
    }
    return true;
}
inline Snapshot prepare(const GBContext *ctx, mon_pic::Cache &portraits) {
    Snapshot out;
    out.kind = context(ctx);
    if (out.kind == Kind::None)
        return out;
    const auto *tiles = ctx->wram + 0x3a0;
    std::copy_n(tiles, 360, out.text.begin());
    out.layout.kind = menu_layout::Kind::Partial;
    out.layout.regions[out.layout.count++] = {0, 0, 20, 18};
    bool summary = out.kind == Kind::Stats || out.kind == Kind::Moves;
    if (summary) {
        out.species = battle::read(ctx, 0xcf97);
        const auto &picture = portraits.get(ctx->rom, ctx->rom_size, out.species, true);
        if (!picture.valid ||
            std::memcmp(picture.tiles.data(), ctx->vram + 0x1000, mon_pic::TileBytes) ||
            !experience(ctx, out))
            return out;
        for (int y = 0; y < 7; ++y)
            for (int x = 1; x < 8; ++x) {
                if (tiles[y * 20 + x] != (7 - x) * 7 + y)
                    return out;
                out.cells[y * 20 + x] = Cell::Portrait;
            }
        for (int y = 1; y < 7; ++y) {
            if (out.kind == Kind::Stats && y == 3)
                continue;
            if (tiles[y * 20 + 19] != 0x78)
                return out;
            out.cells[y * 20 + 19] = Cell::Decoration;
        }
        for (int x = 8; x < 20; ++x) {
            if (tiles[7 * 20 + x] != (x == 8 ? 0x6f : x == 19 ? 0x77 : 0x76))
                return out;
            out.cells[7 * 20 + x] = Cell::Decoration;
        }
        menu_layout::Rect lower{0, 8, out.kind == Kind::Stats ? 10 : 20, 10};
        if (!border(tiles, lower))
            return out;
        out.layout.regions[out.layout.count++] = lower;
        if (out.kind == Kind::Moves)
            for (int x = 9; x < 19; ++x)
                if (tiles[2 * 20 + x] != 0x7f)
                    return out; // The extra XP bar must never cover original text.
        if (out.kind == Kind::Stats) {
            if (!health(ctx, out, 13, 3, 0x6d, battle::read(ctx, 0xcf24)))
                return out;
            for (int y = 9; y < 17; ++y) {
                if (tiles[y * 20 + 19] != 0x78)
                    return out;
                out.cells[y * 20 + 19] = Cell::Decoration;
            }
            for (int x = 12; x < 20; ++x) {
                if (tiles[17 * 20 + x] != (x == 12 ? 0x6f : x == 19 ? 0x77 : 0x76))
                    return out;
                out.cells[17 * 20 + x] = Cell::Decoration;
            }
        }
    } else {
        out.count = battle::read(ctx, 0xd162);
        bool actions = out.kind == Kind::Actions;
        out.selected = battle::read(ctx, actions ? 0xcc2b : 0xcc26);
        if (!out.count || out.count > 6 || out.selected >= out.count ||
            !icons_valid(ctx, out.count))
            return out;
        // HandleMenuInput updates selection before repainting its arrow. Keep
        // the side portrait on the visibly selected row during that frame.
        for (int row = 0; row < out.count; ++row)
            if (tiles[(row * 2 + 1) * 20] == (actions ? 0xec : 0xed)) {
                if (row != out.selected && (actions || row != battle::read(ctx, 0xcc2a)))
                    return out;
                out.selected = row;
                break;
            }
        menu_layout::Rect action{0, 0, 0, 0};
        if (actions) {
            // DisplayFieldMoveMonMenu adds a blank row above the first field
            // move. The no-field-move window has only its ordinary top edge.
            int max = battle::read(ctx, 0xcc28), extra = max > 2 ? 1 : 0;
            int x = battle::read(ctx, 0xcc25) - 1, y = battle::read(ctx, 0xcc24) - 1 - extra;
            if (max < 2 || max > 6 || x < 1 || x > 11 || y < 2 || y > 11 ||
                y != 15 - 2 * max - extra)
                return out;
            action = {x, y, 20 - x, 18 - y};
            if (!border(tiles, action))
                return out;
        }
        if (!border(tiles, menu_layout::Bottom, action))
            return out;
        out.layout.regions[out.layout.count++] = menu_layout::Bottom;
        if (actions)
            out.layout.regions[out.layout.count++] = action;
        out.species = battle::read(ctx, 0xd16a + out.selected * 44);
        if (!portraits.get(ctx->rom, ctx->rom_size, out.species).valid)
            return out;
        for (int row = 0; row < out.count; ++row) {
            int y = row * 2 + 1;
            // Actions can cover the right-hand part of lower party rows.
            if (!menu_layout::contains(action, 12, y) && tiles[y * 20 + 4] == 0x71 &&
                !health(ctx, out, 6, y, 0x6c, battle::read(ctx, 0xcf1e + row)))
                return out;
        }
    }
    const auto &font = rom_font::get(ctx->rom, ctx->rom_size);
    for (int i = 0; i < 360; ++i) {
        uint8_t tile = tiles[i];
        if (out.cells[i] == Cell::Portrait) {
            out.text[i] = 0x7f;
            continue;
        }
        if (tile >= 0x80) {
            if (!menu_text::font_matches(font, ctx->vram, tile))
                return out;
        } else if (tile < 0x79) {
            if (!graphic_matches(ctx, out.kind, tile) ||
                (out.cells[i] == Cell::Text && !graphic_position(out, i % 20, i / 20, tile)))
                return out;
            if (out.cells[i] == Cell::Text)
                out.cells[i] = Cell::Graphic;
            out.text[i] = 0x7f;
        }
    }
    out.valid = true;
    return out;
}
} // namespace pokemon_menu
