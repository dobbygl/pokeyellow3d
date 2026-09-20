#pragma once
#include <array>
#include <cstring>
struct ReadOnlyMemory {
    std::array<uint8_t,32768> wram;
    std::array<uint8_t,16384> vram;
    std::array<uint32_t,160*144> framebuffer;
    explicit ReadOnlyMemory(GBContext* ctx) {
        std::memcpy(wram.data(),ctx->wram,wram.size());
        std::memcpy(vram.data(),ctx->vram,vram.size());
        std::memcpy(framebuffer.data(),gb_get_framebuffer(ctx),sizeof(framebuffer));
    }
    bool unchanged(GBContext* ctx) const {
        return !std::memcmp(wram.data(),ctx->wram,wram.size())&&
               !std::memcmp(vram.data(),ctx->vram,vram.size())&&
               !std::memcmp(framebuffer.data(),gb_get_framebuffer(ctx),sizeof(framebuffer));
    }
};
