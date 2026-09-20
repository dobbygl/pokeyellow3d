#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// Read-only port of Yellow's home/uncompress.asm and home/pics.asm.
// All output is private storage; no emulated register, RAM or bank is changed.
namespace mon_pic {
constexpr size_t TileBytes = 7 * 7 * 16;
struct Picture {
    std::array<uint8_t, TileBytes> tiles{}; // vFrontPic order: columns, tiles, rows, planes.
    int width = 0, height = 0, mode = 0;
    bool valid = false, flipped = false;
};
struct Bits {
    const uint8_t *data;
    size_t size, position = 0;
    bool valid = true;
    unsigned take(unsigned count) {
        unsigned value = 0;
        for (unsigned i = 0; i < count; i++) {
            if (position / 8 >= size) {
                valid = false;
                return 0;
            }
            value = (value << 1) | ((data[position / 8] >> (7 - position % 8)) & 1);
            ++position;
        }
        return value;
    }
};
using Plane = std::array<uint8_t, 7 * 7 * 8>;
inline bool unpack(Bits &bits, Plane &plane, int width, int height) {
    const int pairs = width * height * 32, rows = height * 8;
    int written = 0;
    bool literal = bits.take(1) != 0;
    auto put = [&](unsigned pair) {
        int column = written / (rows * 4), shift = 6 - 2 * ((written / rows) % 4),
            row = written % rows;
        plane[column * rows + row] |= uint8_t(pair << shift);
        ++written;
    };
    while (written < pairs && bits.valid) {
        if (literal) {
            unsigned pair = bits.take(2);
            if (pair)
                put(pair);
            else
                literal = false;
        } else {
            unsigned length = 0;
            while (bits.take(1) && bits.valid)
                if (++length > 15)
                    return false;
            unsigned zeros = ((1u << (length + 1)) - 1) + bits.take(length + 1);
            if (zeros > unsigned(pairs - written))
                return false;
            while (zeros--)
                put(0);
            literal = true;
        }
    }
    return bits.valid && written == pairs;
}
inline void differential(Plane &plane, int width, int height) {
    for (int y = 0; y < height * 8; y++) {
        int previous = 0;
        for (int x = 0; x < width; x++) {
            auto &byte = plane[x * height * 8 + y];
            uint8_t decoded = 0;
            for (int bit = 7; bit >= 0; bit--) {
                previous ^= (byte >> bit) & 1;
                decoded |= uint8_t(previous << bit);
            }
            byte = decoded;
        }
    }
}
inline uint8_t reverse(uint8_t byte) {
    byte = uint8_t((byte >> 4) | (byte << 4));
    byte = uint8_t(((byte & 0xcc) >> 2) | ((byte & 0x33) << 2));
    return uint8_t(((byte & 0xaa) >> 1) | ((byte & 0x55) << 1));
}
inline Picture decompress(const uint8_t *data, size_t size, bool flipped = false) {
    Picture out;
    out.flipped = flipped;
    if (!data || size < 2)
        return out;
    out.width = data[0] >> 4;
    out.height = data[0] & 15;
    if (out.width < 1 || out.width > 7 || out.height < 1 || out.height > 7)
        return out;
    Bits bits{data + 1, size - 1};
    Plane planes[2]{};
    int first = int(bits.take(1)), second = first ^ 1;
    if (!unpack(bits, planes[first], out.width, out.height))
        return out;
    out.mode = bits.take(1) ? 1 + int(bits.take(1)) : 0;
    if (!unpack(bits, planes[second], out.width, out.height))
        return out;
    differential(planes[first], out.width, out.height);
    if (out.mode != 1)
        differential(planes[second], out.width, out.height);
    if (out.mode)
        for (size_t i = 0; i < planes[0].size(); i++)
            planes[second][i] ^= planes[first][i];
    // LoadUncompressedSpriteData bottom-aligns inside the 7x7 canvas.
    int left = (8 - out.width) / 2, top = 7 - out.height;
    for (int x = 0; x < out.width; x++)
        for (int y = 0; y < out.height * 8; y++)
            for (int plane = 0; plane < 2; plane++) {
                uint8_t byte = planes[plane][x * out.height * 8 + y];
                out.tiles[((x + left) * 56 + top * 8 + y) * 2 + plane] =
                    flipped ? reverse(byte) : byte;
            }
    out.valid = true;
    return out;
}
inline int number(const uint8_t *rom, size_t size, int species) {
    if (!rom || size != 1048576 || species < 1 || species > 190)
        return 0;
    int n = rom[0x410b1 + species - 1];
    return n >= 1 && n <= 151 ? n : 0;
}
inline int species(const uint8_t *rom, size_t size, int number) {
    if (number < 1 || number > 151)
        return 0;
    for (int id = 1; id <= 190; id++)
        if (mon_pic::number(rom, size, id) == number)
            return id;
    return 0;
}
inline Picture front(const uint8_t *rom, size_t size, int species, bool flipped = false) {
    int n = number(rom, size, species);
    if (!n)
        return {};
    // Yellow includes Mew in BaseStats (unlike Red/Blue's separate Mew header).
    size_t header = 0x383de + (n - 1) * 28;
    if (rom[header] != n)
        return {};
    unsigned pointer = rom[header + 11] | (rom[header + 12] << 8);
    if (pointer < 0x4000 || pointer >= 0x8000)
        return {};
    int bank = species < 0x1f   ? 9
               : species < 0x4a ? 10
               : species < 0x74 ? 11
               : species < 0x99 ? 12
                                : 13;
    size_t offset = bank * 0x4000 + (pointer & 0x3fff);
    auto out = decompress(rom + offset, 0x4000 - (pointer & 0x3fff), flipped);
    if (out.width != (rom[header + 10] & 15) || out.height != (rom[header + 10] >> 4))
        out.valid = false;
    return out;
}
} // namespace mon_pic
