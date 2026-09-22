#pragma once
#include <array>
#include <cstring>
#include <vector>
struct ReadOnlyMemory {
    std::array<uint8_t, 32768> wram;
    std::array<uint8_t, 16384> vram;
    std::array<uint8_t, 160> oam;
    std::array<uint8_t, 127> hram;
    std::array<uint8_t, 128> io;
    std::array<uint32_t, 160 * 144> framebuffer;
    std::vector<uint8_t> eram;
    explicit ReadOnlyMemory(GBContext *ctx) {
        std::memcpy(wram.data(), ctx->wram, wram.size());
        std::memcpy(vram.data(), ctx->vram, vram.size());
        std::memcpy(oam.data(), ctx->oam, oam.size());
        std::memcpy(hram.data(), ctx->hram, hram.size());
        std::memcpy(io.data(), ctx->io, io.size());
        std::memcpy(framebuffer.data(), gb_get_framebuffer(ctx), sizeof(framebuffer));
        if (ctx->eram && ctx->eram_size)
            eram.assign(ctx->eram, ctx->eram + ctx->eram_size);
    }
    bool unchanged(GBContext *ctx) const {
        return !std::memcmp(wram.data(), ctx->wram, wram.size()) &&
               !std::memcmp(vram.data(), ctx->vram, vram.size()) &&
               !std::memcmp(oam.data(), ctx->oam, oam.size()) &&
               !std::memcmp(hram.data(), ctx->hram, hram.size()) &&
               !std::memcmp(io.data(), ctx->io, io.size()) &&
               !std::memcmp(framebuffer.data(), gb_get_framebuffer(ctx), sizeof(framebuffer)) &&
               eram.size() == ctx->eram_size &&
               (eram.empty() || !std::memcmp(eram.data(), ctx->eram, eram.size()));
    }
};
