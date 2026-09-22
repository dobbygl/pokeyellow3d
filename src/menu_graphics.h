#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// These are graphics, not aliases for printable characters. Verified against
// pret/pokeyellow e89ead154b9968aa50eed9328ff2b38b6c194382, charmap.asm,
// LoadHpBarAndStatusTilePatterns and BillsPCMenu. Both complete source assets
// match the cartridge, and every live 2bpp tile must still match its ROM bytes.
namespace menu_graphics {
constexpr uint8_t Level = 0x6e, OccupiedBox = 0x78;
constexpr size_t LevelOffset = 0x10ae0, OccupiedBoxOffset = 0x3aa28;
inline size_t offset(uint8_t tile) {
    return tile == Level ? LevelOffset : tile == OccupiedBox ? OccupiedBoxOffset : 0;
}
inline bool matches(uint8_t tile, const uint8_t *vram, const uint8_t *rom, size_t size) {
    size_t source = offset(tile);
    return source && vram && rom && size == 1048576 &&
           std::memcmp(vram + 0x1000 + size_t(tile) * 16, rom + source, 16) == 0;
}
inline int pixel(const uint8_t *rom, uint8_t tile, int x, int y) {
    size_t source = offset(tile) + size_t(y) * 2;
    return ((rom[source] >> (7 - x)) & 1) | (((rom[source + 1] >> (7 - x)) & 1) << 1);
}
} // namespace menu_graphics
