#pragma once
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

// Presentation labels use the English charmap's original glyphs. Names already
// encoded by the game bypass this conversion, preserving every special glyph.
namespace hud_text {
inline std::vector<uint8_t> encode(std::string_view text) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = text[i];
        int tile = c >= 'A' && c <= 'Z'   ? 0x80 + c - 'A'
                   : c >= 'a' && c <= 'z' ? 0xa0 + c - 'a'
                   : c >= '0' && c <= '9' ? 0xf6 + c - '0'
                                          : -1;
        if (tile < 0) {
            switch (c) {
            case ' ':
                tile = 0x7f;
                break;
            case '(':
                tile = 0x9a;
                break;
            case ')':
                tile = 0x9b;
                break;
            case ':':
                tile = 0x9c;
                break;
            case ';':
                tile = 0x9d;
                break;
            case '[':
                tile = 0x9e;
                break;
            case ']':
                tile = 0x9f;
                break;
            case '\'':
                tile = 0xe0;
                break;
            case '-':
                tile = 0xe3;
                break;
            case '?':
                tile = 0xe6;
                break;
            case '!':
                tile = 0xe7;
                break;
            case '.':
                tile = 0xe8;
                break;
            case '/':
                tile = 0xf3;
                break;
            case ',':
                tile = 0xf4;
                break;
            default:
                if (c == 0xc3 && i + 1 < text.size() && uint8_t(text[i + 1]) == 0xa9) {
                    tile = 0xba;
                    ++i;
                } else
                    return {};
            }
        }
        out.push_back(uint8_t(tile));
    }
    return out;
}
inline bool supported(const uint8_t *tiles, size_t count) {
    if (!tiles)
        return false;
    for (size_t i = 0; i < count && tiles[i] != 0x50; ++i)
        if (tiles[i] < 0x7f)
            return false;
    return true;
}
inline size_t length(const uint8_t *tiles, size_t count) {
    size_t n = 0;
    while (n < count && tiles[n] != 0x50)
        ++n;
    return n;
}
} // namespace hud_text
