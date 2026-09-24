#pragma once
#include "interior_scene.h"
#include <array>

// Curated presentation rules: graphics stay in the private cartridge.
// A2 only fills the reference classifier's unknown cells, after its perimeter.
// Each row documents the exact tileset and the two left tiles; right=-1 is unused.
namespace interior {
struct ArtRule {
    int tileset, top, bottom, right;
    Kind kind;
    float height;
};
inline constexpr std::array<ArtRule, 143> ArtRules{{
    {2, 0x00, 0x00, -1, Kind::Floor, 0.00f},        // MART: vacio
    {2, 0x10, 0x10, -1, Kind::Counter, 1.05f},      // MART: mostrador
    {2, 0x28, 0x10, -1, Kind::Counter, 1.05f},      // MART: mostrador
    {2, 0x28, 0x28, -1, Kind::Wall, 2.00f},         // MART: pared
    {2, 0x3b, 0x4b, -1, Kind::Wall, 2.00f},         // MART: pared
    {2, 0x48, 0x48, -1, Kind::Machine, 1.05f},      // MART: maquina curacion
    {2, 0x5a, 0x14, -1, Kind::Wall, 2.00f},         // MART: pared
    {2, 0x5a, 0x28, -1, Kind::Wall, 2.00f},         // MART: pared
    {5, 0x05, 0x10, -1, Kind::Wall, 2.00f},         // DOJO: pared
    {5, 0x20, 0x30, -1, Kind::Furniture, 0.25f},    // DOJO: puerta suelo
    {6, 0x10, 0x10, -1, Kind::Counter, 1.05f},      // POKECENTER: mostrador
    {6, 0x48, 0x48, -1, Kind::Machine, 1.05f},      // POKECENTER: maquina curacion
    {7, 0x04, 0x14, -1, Kind::Water, 0.00f},        // GYM: agua
    {7, 0x05, 0x56, -1, Kind::Machine, 1.05f},      // GYM: barrera electrica
    {7, 0x07, 0x17, -1, Kind::Rock, 0.85f},         // GYM: roca
    {7, 0x0b, 0x1b, -1, Kind::Furniture, 0.65f},    // GYM: papelera
    {7, 0x14, 0x14, -1, Kind::Water, 0.00f},        // GYM: agua
    {7, 0x1f, 0x1f, -1, Kind::Floor, 0.00f},        // GYM: muro invisible
    {7, 0x2c, 0x2e, -1, Kind::Furniture, 0.65f},    // GYM: arbusto
    {7, 0x40, 0x50, -1, Kind::Furniture, 0.65f},    // GYM: arbol cortable
    {7, 0x44, 0x54, -1, Kind::Machine, 1.05f},      // GYM: barrera electrica
    {7, 0x45, 0x56, -1, Kind::Machine, 1.05f},      // GYM: barrera electrica
    {7, 0x5b, 0x36, -1, Kind::Machine, 1.15f},      // GYM: ordenador
    {8, 0x30, 0x1e, -1, Kind::Furniture, 1.45f},    // HOUSE: estanteria
    {8, 0x50, 0x52, -1, Kind::Furniture, 0.65f},    // HOUSE: mesa
    {8, 0x56, 0x3c, -1, Kind::Furniture, 0.65f},    // HOUSE: mesa
    {10, 0x40, 0x50, -1, Kind::Furniture, 0.85f},   // MUSEUM: expositor
    {10, 0x42, 0x52, -1, Kind::Furniture, 0.85f},   // MUSEUM: expositor
    {10, 0x44, 0x54, -1, Kind::Furniture, 0.85f},   // MUSEUM: expositor
    {12, 0x11, 0x0e, -1, Kind::Furniture, 1.05f},   // GATE: estatua
    {12, 0x24, 0x34, -1, Kind::Furniture, 0.85f},   // GATE: prismaticos
    {12, 0x25, 0x35, -1, Kind::Furniture, 0.65f},   // GATE: planta
    {12, 0x2d, 0x3d, -1, Kind::Window, 2.00f},      // GATE: ventana
    {12, 0x3a, 0x3a, -1, Kind::Window, 2.00f},      // GATE: ventana
    {12, 0x3e, 0x3e, -1, Kind::Window, 2.00f},      // GATE: ventana
    {13, 0x01, 0x11, -1, Kind::Floor, 0.00f},       // SHIP: vacio
    {13, 0x09, 0x19, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x0a, 0x2c, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x0a, 0x35, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x0a, 0x41, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x0a, 0x44, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x14, 0x14, -1, Kind::Water, 0.00f},       // SHIP: agua
    {13, 0x19, 0x0b, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x19, 0x19, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x1a, 0x2c, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x25, 0x01, -1, Kind::Floor, 0.00f},       // SHIP: vacio
    {13, 0x2c, 0x0b, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x2c, 0x3b, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x2c, 0x3c, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x2d, 0x3d, -1, Kind::Furniture, 0.65f},   // SHIP: bita
    {13, 0x2e, 0x3e, -1, Kind::Furniture, 0.85f},   // SHIP: borda
    {13, 0x41, 0x51, -1, Kind::Furniture, 0.65f},   // SHIP: mesa
    {13, 0x46, 0x56, -1, Kind::Furniture, 0.48f},   // SHIP: cama
    {13, 0x48, 0x58, -1, Kind::Furniture, 0.65f},   // SHIP: papelera
    {13, 0x56, 0x3b, -1, Kind::Furniture, 0.48f},   // SHIP: cama
    {13, 0x56, 0x56, -1, Kind::Furniture, 0.48f},   // SHIP: cama
    {15, 0x01, 0x27, -1, Kind::Furniture, 1.20f},   // CEMETERY: estatua
    {15, 0x05, 0x15, -1, Kind::Furniture, 0.65f},   // CEMETERY: lapida
    {15, 0x1d, 0x1d, -1, Kind::Counter, 1.05f},     // CEMETERY: mostrador
    {15, 0x37, 0x3d, -1, Kind::Furniture, 1.20f},   // CEMETERY: estatua
    {16, 0x13, 0x23, -1, Kind::Wall, 2.00f},        // INTERIOR: cuadro
    {16, 0x1f, 0x48, -1, Kind::Furniture, 0.65f},   // INTERIOR: mesa
    {16, 0x22, 0x5c, -1, Kind::Furniture, 0.65f},   // INTERIOR: mesa
    {16, 0x2d, 0x2d, -1, Kind::Wall, 2.00f},        // INTERIOR: tabique
    {16, 0x2d, 0x59, -1, Kind::Wall, 2.00f},        // INTERIOR: tabique
    {16, 0x2d, 0x5b, -1, Kind::Wall, 2.00f},        // INTERIOR: tabique
    {16, 0x34, 0x32, -1, Kind::Wall, 2.00f},        // INTERIOR: tabique
    {16, 0x34, 0x33, -1, Kind::Wall, 2.00f},        // INTERIOR: tabique
    {16, 0x34, 0x44, -1, Kind::Wall, 2.00f},        // INTERIOR: tabique
    {16, 0x3f, 0x4f, -1, Kind::Furniture, 0.65f},   // INTERIOR: mesa
    {16, 0x40, 0x40, -1, Kind::Furniture, 0.65f},   // INTERIOR: mesa
    {16, 0x40, 0x51, -1, Kind::Furniture, 0.65f},   // INTERIOR: mesa
    {16, 0x49, 0x40, -1, Kind::Furniture, 0.65f},   // INTERIOR: mesa
    {16, 0x4a, 0x12, -1, Kind::Furniture, 0.65f},   // INTERIOR: mesa
    {16, 0x4a, 0x40, -1, Kind::Furniture, 0.65f},   // INTERIOR: mesa
    {16, 0x4d, 0x4d, -1, Kind::Furniture, 0.65f},   // INTERIOR: mesa
    {16, 0x57, 0x57, -1, Kind::Wall, 2.00f},        // INTERIOR: tabique
    {16, 0x57, 0x58, -1, Kind::Wall, 2.00f},        // INTERIOR: tabique
    {16, 0x59, 0x59, -1, Kind::Wall, 2.00f},        // INTERIOR: tabique
    {18, 0x0e, 0x1e, -1, Kind::Machine, 1.05f},     // LOBBY: consola
    {18, 0x10, 0x10, -1, Kind::Floor, 0.00f},       // LOBBY: vacio
    {18, 0x24, 0x34, -1, Kind::Machine, 1.05f},     // LOBBY: consola
    {18, 0x27, 0x37, -1, Kind::Counter, 1.05f},     // LOBBY: mostrador
    {18, 0x37, 0x0a, -1, Kind::Wall, 2.00f},        // LOBBY: vitrina
    {18, 0x37, 0x57, -1, Kind::Counter, 1.05f},     // LOBBY: mostrador
    {18, 0x37, 0x5b, -1, Kind::Wall, 2.00f},        // LOBBY: pared
    {18, 0x42, 0x52, -1, Kind::Furniture, 1.45f},   // LOBBY: estanteria
    {18, 0x46, 0x55, -1, Kind::Counter, 1.05f},     // LOBBY: mostrador
    {18, 0x48, 0x58, -1, Kind::Wall, 2.00f},        // LOBBY: cartel
    {18, 0x4b, 0x4b, 0x4c, Kind::Counter, 1.05f},   // LOBBY: mostrador
    {18, 0x4b, 0x4b, 0x4f, Kind::Wall, 2.00f},      // LOBBY: pared
    {18, 0x4b, 0x4d, -1, Kind::Counter, 1.05f},     // LOBBY: mostrador
    {18, 0x4f, 0x4f, -1, Kind::Wall, 2.00f},        // LOBBY: pared
    {19, 0x10, 0x10, -1, Kind::Floor, 0.00f},       // MANSION: vacio
    {19, 0x1e, 0x06, -1, Kind::Furniture, 0.65f},   // MANSION: caseta
    {19, 0x24, 0x40, -1, Kind::Machine, 1.15f},     // MANSION: ordenador
    {19, 0x2a, 0x2b, -1, Kind::Furniture, 1.45f},   // MANSION: barandilla
    {19, 0x30, 0x30, -1, Kind::Furniture, 0.65f},   // MANSION: caseta
    {19, 0x34, 0x42, -1, Kind::Machine, 1.15f},     // MANSION: ordenador
    {19, 0x36, 0x36, -1, Kind::Furniture, 0.65f},   // MANSION: caseta
    {19, 0x36, 0x3c, -1, Kind::Furniture, 0.65f},   // MANSION: mesa
    {19, 0x44, 0x08, -1, Kind::Furniture, 0.65f},   // MANSION: planta
    {19, 0x46, 0x18, -1, Kind::Furniture, 0.65f},   // MANSION: planta
    {20, 0x02, 0x12, -1, Kind::Machine, 1.15f},     // LAB: ordenador
    {20, 0x04, 0x14, -1, Kind::Machine, 1.15f},     // LAB: ordenador
    {20, 0x0a, 0x1a, -1, Kind::Machine, 1.30f},     // LAB: maquina
    {20, 0x0b, 0x1b, -1, Kind::Machine, 1.30f},     // LAB: maquina
    {20, 0x10, 0x20, -1, Kind::Wall, 2.00f},        // LAB: cuadro
    {20, 0x18, 0x22, -1, Kind::Wall, 2.00f},        // LAB: cuadro
    {20, 0x22, 0x22, -1, Kind::Wall, 2.00f},        // LAB: pared
    {20, 0x23, 0x23, -1, Kind::Wall, 2.00f},        // LAB: pared
    {20, 0x28, 0x28, -1, Kind::Furniture, 1.45f},   // LAB: estanteria
    {20, 0x2c, 0x3c, -1, Kind::Furniture, 0.65f},   // LAB: planta
    {20, 0x2e, 0x3e, -1, Kind::Furniture, 0.65f},   // LAB: planta
    {20, 0x30, 0x22, -1, Kind::Wall, 2.00f},        // LAB: cuadro
    {20, 0x36, 0x36, -1, Kind::Floor, 0.00f},       // LAB: vacio
    {20, 0x40, 0x0a, -1, Kind::Machine, 1.30f},     // LAB: maquina
    {20, 0x40, 0x28, -1, Kind::Furniture, 1.45f},   // LAB: estanteria
    {20, 0x40, 0x2a, -1, Kind::Furniture, 0.70f},   // LAB: mesa
    {20, 0x40, 0x50, -1, Kind::Furniture, 0.70f},   // LAB: mesa
    {20, 0x49, 0x3a, -1, Kind::Furniture, 0.70f},   // LAB: mesa
    {20, 0x4a, 0x5a, -1, Kind::Machine, 1.30f},     // LAB: maquina
    {20, 0x4b, 0x5b, -1, Kind::Machine, 1.30f},     // LAB: maquina
    {20, 0x50, 0x53, -1, Kind::Furniture, 0.70f},   // LAB: mesa
    {20, 0x51, 0x3a, -1, Kind::Furniture, 0.70f},   // LAB: mesa
    {22, 0x01, 0x27, -1, Kind::Furniture, 1.20f},   // FACILITY: estatua
    {22, 0x02, 0x35, -1, Kind::Furniture, 0.65f},   // FACILITY: mesa
    {22, 0x05, 0x15, -1, Kind::Furniture, 0.65f},   // FACILITY: planta
    {22, 0x07, 0x17, -1, Kind::Furniture, 0.65f},   // FACILITY: planta
    {22, 0x14, 0x14, -1, Kind::Water, 0.00f},       // FACILITY: agua
    {22, 0x1d, 0x1d, 0x1e, Kind::Counter, 1.05f},   // FACILITY: mostrador
    {22, 0x1d, 0x1d, 0x35, Kind::Furniture, 0.65f}, // FACILITY: mesa
    {22, 0x1d, 0x44, -1, Kind::Furniture, 0.65f},   // FACILITY: mesa
    {22, 0x28, 0x38, -1, Kind::Furniture, 0.48f},   // FACILITY: cama
    {22, 0x33, 0x33, -1, Kind::Floor, 0.00f},       // FACILITY: vacio
    {22, 0x35, 0x35, -1, Kind::Furniture, 0.65f},   // FACILITY: mesa
    {22, 0x35, 0x45, -1, Kind::Furniture, 0.65f},   // FACILITY: mesa
    {22, 0x37, 0x3d, -1, Kind::Furniture, 1.20f},   // FACILITY: estatua
    {22, 0x38, 0x19, -1, Kind::Furniture, 0.48f},   // FACILITY: cama
    {22, 0x40, 0x50, -1, Kind::Furniture, 0.45f},   // FACILITY: barandilla
    {22, 0x41, 0x41, -1, Kind::Furniture, 0.45f},   // FACILITY: barandilla
    {22, 0x41, 0x51, -1, Kind::Furniture, 0.45f},   // FACILITY: barandilla
    {22, 0x4a, 0x4c, -1, Kind::Machine, 1.30f},     // FACILITY: maquina
}};

// Compile-time buckets keep a lookup within its own tileset, not all 143 rows.
inline constexpr auto ArtOffsets = [] {
    std::array<size_t, 26> offsets{};
    size_t row = 0;
    for (int ts = 0; ts <= 25; ++ts) {
        while (row < ArtRules.size() && ArtRules[row].tileset < ts)
            ++row;
        offsets[size_t(ts)] = row;
    }
    return offsets;
}();
inline constexpr bool valid_art_rules() {
    for (size_t i = 0; i < ArtRules.size(); ++i) {
        const auto &r = ArtRules[i];
        if (r.tileset < 0 || r.tileset >= 25 || r.top < 0 || r.top >= 96 || r.bottom < 0 ||
            r.bottom >= 96 || r.right < -1 || r.right >= 96 || r.height < 0 || r.height > 2 ||
            r.kind == Kind::Warp || r.kind == Kind::Count ||
            (i && ArtRules[i - 1].tileset > r.tileset))
            return false;
        for (size_t j = 0; j < i; ++j) {
            const auto &p = ArtRules[j];
            if (r.tileset == p.tileset && r.top == p.top && r.bottom == p.bottom &&
                (r.right == p.right || r.right == -1 || p.right == -1))
                return false;
        }
    }
    return true;
}
static_assert(valid_art_rules(), "A2 rules must be bounded, ordered and unambiguous");

inline const ArtRule *art_rule(int tileset, int top, int bottom, int right) {
    if (tileset < 0 || tileset >= 25)
        return nullptr;
    for (size_t i = ArtOffsets[size_t(tileset)]; i < ArtOffsets[size_t(tileset) + 1]; ++i) {
        const auto &r = ArtRules[i];
        if (r.top == top && r.bottom == bottom && (r.right == -1 || r.right == right))
            return &r;
    }
    return nullptr;
}

// Separate API: OFF and engine/UI consumers always retain classify_tiles().
inline Cell classify_art_tiles(const pallet::Scene &scene, int x, int z, int top, int bottom,
                               int right) {
    auto reference = classify_tiles(scene, x, z, top, bottom);
    // FACILITY table backs share an old broad rule with CEMETERY bookcases.
    // Keep both halves of these tables at the same height, without touching graves.
    if (scene.tileset == 22 && top == 0x0d && bottom == 0x1d && reference.kind == Kind::Furniture)
        return {Kind::Furniture, .65f, true};
    if (reference.curated)
        return reference;
    if (const auto *rule = art_rule(scene.tileset, top, bottom, right))
        return {rule->kind, rule->height, true};
    return reference; // Unknown future art keeps its flat fallback and audit warning.
}

inline Cell classify_art(const uint8_t *rom, const pallet::Scene &scene, int x, int z,
                         const std::vector<uint8_t> *live = nullptr) {
    return classify_art_tiles(scene, x, z, pallet::map_tile(rom, scene, x * 2, z * 2, live),
                              pallet::map_tile(rom, scene, x * 2, z * 2 + 1, live),
                              pallet::map_tile(rom, scene, x * 2 + 1, z * 2, live));
}
} // namespace interior
