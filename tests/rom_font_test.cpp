#include "rom_font.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

static void check(bool ok, const char *message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
static std::vector<uint8_t> read(const char *path) {
    std::ifstream input(path, std::ios::binary);
    check(bool(input), "input exists");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
int main(int argc, char **argv) {
    using namespace rom_font;
    if (argc > 2 && !std::ifstream(argv[2], std::ios::binary)) {
        std::fprintf(stderr, "SKIP: export a text savestate with font-export to %s\n", argv[2]);
        return 77;
    }
    std::vector<uint8_t> rom(Offset + Glyphs * 8);
    for (size_t i = Offset; i < rom.size(); ++i)
        rom[i] = uint8_t(i ^ (i >> 3));
    check(!decode(nullptr, rom.size()).valid && !decode(rom.data(), Offset).valid,
          "missing and truncated font remain invalid");
    if (argc > 1)
        rom = read(argv[1]);
    auto font = decode(rom.data(), rom.size());
    check(font.valid, "complete font decoded");
    for (int glyph = 0; glyph < 128; ++glyph)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                bool bit = (rom[Offset + glyph * 8 + y] >> (7 - x)) & 1;
                size_t i = ((glyph / 16 * 8 + y) * Width + glyph % 16 * 8 + x) * 4;
                check(font.ink(uint8_t(glyph + 128), x, y) == bit &&
                          font.rgba[i + 3] == (bit ? 255 : 0) && font.rgba[i] == 255,
                      "all 8192 atlas pixels agree with original 1 bpp bits");
            }
    std::vector<uint8_t> legacy(512 * 512 * 4);
    check(copy_to(rom.data(), rom.size(), legacy.data(), 512, 512, 0, 256),
          "legacy PC atlas accepts shared font");
    check(copy_to(rom.data(), rom.size(), legacy.data(), 512, 512, 0, 256) && decodes == 1,
          "multiple consumers decode exactly once");
    check(!copy_to(rom.data(), rom.size(), legacy.data(), 512, 512, 500, 256),
          "atlas bounds reject overflow");
    if (argc > 2) {
        auto vram = read(argv[2]);
        check(vram.size() >= 0x1000, "complete VRAM font export");
        int differences = 0;
        for (int glyph = 0; glyph < 128; ++glyph)
            for (int y = 0; y < 8; ++y) {
                auto expected = rom[Offset + glyph * 8 + y];
                for (int plane = 0; plane < 2; ++plane)
                    if (vram[0x800 + glyph * 16 + y * 2 + plane] != expected) {
                        if (differences < 8)
                            std::fprintf(stderr, "tile=%02x row=%d plane=%d ROM=%02x VRAM=%02x\n",
                                         glyph + 128, y, plane, expected,
                                         vram[0x800 + glyph * 16 + y * 2 + plane]);
                        ++differences;
                    }
            }
        check(!differences, "all 128 ROM glyphs equal both VRAM bitplanes from a text savestate");
    }
    std::puts("PASS: ROM font decoding, shared atlas and exact glyph bits");
}
