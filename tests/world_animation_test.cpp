#include "npc_animation.h"
#include "world_effects.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static void check(bool ok, const char *why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}
static void npc() {
    std::vector<uint8_t> rom(1048576), ram(32768), vram(16384, 0xee);
    GBContext ctx{};
    ctx.rom = rom.data();
    ctx.rom_size = rom.size();
    ctx.wram = ram.data();
    ctx.vram = vram.data();
    auto *a = ram.data() + 0x110, *b = ram.data() + 0x210;
    a[0] = 1;
    a[1] = 3;
    a[5] = 1;
    a[8] = 3;
    a[9] = 12;
    b[0] = 8;
    auto m = npc_animation::sample(&ctx, 1);
    check(m.walking && m.frame == 15 && m.dx == -.5f && m.dz == 0,
          "walking uses live frame and destination-relative fractional step");
    check(npc_animation::sample(&ctx, 1).dx == m.dx, "frozen offscreen step stays frozen");
    a[5] = 255;
    check(npc_animation::sample(&ctx, 1).dx == .5f, "signed westward step");
    a[1] = 4;
    check(npc_animation::sample(&ctx, 1).dx == 1, "double-speed scripted step");
    b[0] = 9;
    check(!npc_animation::sample(&ctx, 1).walking, "invalid remaining count stays static");
    a[1] = 2;
    check(npc_animation::sample(&ctx, 1).frame == 12 && npc_animation::sample(&ctx, 1).dx == 0,
          "stationary actor ignores stale walking counter");
    check(!npc_animation::sample(&ctx, 15).walking && !npc_animation::sample(nullptr, 1).walking,
          "companion and unsupported contexts retain existing behavior");
    // Procedural sheet and facing entries. Each tile has a unique asymmetric
    // colour pattern so tile order, both flip axes and walking bank are tested.
    const size_t sheet = 0x9000;
    auto *entry = rom.data() + npc_animation::Sheets;
    entry[0] = 0;
    entry[1] = 0x50;
    entry[2] = 192;
    entry[3] = 2;
    for (int tile = 0; tile < 24; ++tile)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                int value = (tile + x + y * 2 + (tile >= 12 ? 1 : 0)) & 3;
                rom[sheet + size_t(tile) * 16 + y * 2] |= uint8_t((value & 1) << (7 - x));
                rom[sheet + size_t(tile) * 16 + y * 2 + 1] |= uint8_t((value >> 1) << (7 - x));
            }
    for (int frame = 0; frame < 16; ++frame) {
        size_t table = npc_animation::Facings + size_t(frame) * 2;
        size_t layout = 0x4100 + size_t(frame) * 17;
        rom[table] = uint8_t(layout);
        rom[table + 1] = uint8_t(layout >> 8);
        rom[layout] = 4;
        for (int part = 0; part < 4; ++part) {
            size_t p = layout + 1 + size_t(part) * 4;
            rom[p] = uint8_t((part / 2) * 8);
            rom[p + 1] = uint8_t((part % 2) * 8);
            rom[p + 2] = uint8_t(part + ((frame & 1) ? 128 : 0));
            rom[p + 3] = uint8_t(((frame & 2) ? 0x20 : 0) | ((frame & 4) ? 0x40 : 0));
        }
        npc_animation::Image image{};
        check(npc_animation::decode(&ctx, 1, frame, image), "valid ROM sheet decoded");
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                int expected = 0;
                if (x >= 8 && x < 24 && y >= 8 && y < 24) {
                    int sx = (x - 8) % 8, sy = (y - 8) % 8;
                    int tile = (x - 8) / 8 + (y - 8) / 8 * 2 + ((frame & 1) ? 12 : 0);
                    if (frame & 2)
                        sx = 7 - sx;
                    if (frame & 4)
                        sy = 7 - sy;
                    expected = (tile + sx + sy * 2 + (tile >= 12 ? 1 : 0)) & 3;
                }
                check(image[size_t(y) * 32 + x] == expected, "ROM pixels and flips match oracle");
            }
    }
    auto before_ram = ram, before_rom = rom, before_vram = vram;
    npc_animation::Image result{}, standing{};
    check(npc_animation::decode(&ctx, 1, 0, standing), "standing frame");
    entry[2] = 64;
    check(npc_animation::decode(&ctx, 1, 15, result) && result == standing,
          "four-tile objects never animate");
    entry[2] = 192;
    check(ram == before_ram && rom == before_rom && vram == before_vram,
          "decoding leaves every supplied memory region unchanged");
    entry[3] = 255;
    result.fill(7);
    check(!npc_animation::decode(&ctx, 1, 0, result) && result[0] == 7,
          "invalid source keeps caller fallback intact");
}
static void effects() {
    using namespace world_effects;
    State s;
    uint32_t cycles = 1000;
    s.update(12, cycles, false, 1, 1, Surface::Grass);
    for (int i = 0; i < 60; ++i)
        s.update(12, cycles += 70224, false, 1, 1, Surface::Grass);
    check(s.wind() > 0 && s.count() == 0 && s.emitted == 0,
          "wind advances while a stationary player emits nothing");
    s.update(12, cycles += 70224, false, 1.5f, 1, Surface::Grass);
    check(s.count() == 3, "actual walking creates grass fragments");
    float wind = s.wind(), age = s.particles[0].age;
    for (int i = 0; i < 60; ++i)
        s.update(12, cycles += 70224, true, 1.5f, 1, Surface::Grass);
    check(s.wind() == wind && s.particles[0].age == age && s.count() == 3,
          "pause freezes wind, particles and emission");
    for (int i = 0; i < 60; ++i)
        s.update(12, cycles += 70224, false, 1.5f, 1, Surface::Grass);
    check(s.count() == 0 && s.emitted == 3, "stationary trail expires without new emissions");
    s.update(0, cycles += 70224, false, 4, 4, Surface::Surf);
    check(s.wind() == 0 && s.count() == 0 && s.emitted == 0, "map change resets all transients");
    State replay = s;
    for (int i = 0; i < 1000; ++i) {
        cycles += 100;
        float x = 4 + float(i % 2) * .5f;
        s.update(0, cycles, false, x, 4, Surface::Surf);
        replay.update(0, cycles, false, x, 4, Surface::Surf);
    }
    check(s.count() == s.particles.size() && s.emitted > s.particles.size(),
          "sustained emissions use a fixed bounded pool");
    for (size_t i = 0; i < s.particles.size(); ++i) {
        auto p = s.particles[i], q = replay.particles[i];
        check(p.x == q.x && p.z == q.z && p.vx == q.vx && p.vz == q.vz && p.age == q.age &&
                  p.surface == Surface::Surf,
              "same movement and cycles produce identical surf particles");
    }
    s.reset();
    check(s.count() == 0 && s.wind() == 0 && s.emitted == 0, "explicit load reset");
    s.update(0, UINT32_MAX - 1000, false, 4, 4, Surface::None);
    s.update(0, 69223, false, 4, 4, Surface::None);
    check(s.wind() > 0 && s.wind() < .02f, "normal 32-bit guest cycle wrap remains continuous");
    s.update(0, 0, false, 4, 4, Surface::None);
    check(s.wind() == 0, "backwards clock restore resets transients");
}
int main() {
    npc();
    effects();
    std::puts("PASS: live NPC frames and ROM sheets; deterministic bounded wind/particles, "
              "pause/load/map/wrap");
}
