#pragma once

#include "ue/Schema.h"
#include "ue/Types.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

struct IDirect3DTexture9;

namespace lis::assets {

enum class SpriteId : std::uint8_t { Paper, NavStrip, Tape, StickyNote, Star, Logo, Count };

struct Sprite {
    IDirect3DTexture9* texture = nullptr;
    float u0 = 0.0F;
    float v0 = 0.0F;
    float u1 = 1.0F;
    float v1 = 1.0F;
    float width = 0.0F;
    float height = 0.0F;

    [[nodiscard]] bool IsValid() const noexcept { return texture != nullptr; }
};

struct Glyph {
    std::uint16_t codepoint = 0;
    std::uint8_t page = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::int32_t advance = 0;
};

struct FontSource {
    std::vector<IDirect3DTexture9*> pages;
    std::vector<Glyph> glyphs;
    float lineHeight = 0.0F;
};

class GameAssets {
public:
    GameAssets();

    void Refresh();
    [[nodiscard]] const Sprite& Get(SpriteId id) const noexcept { return sprites_[static_cast<std::size_t>(id)]; }
    [[nodiscard]] std::optional<FontSource> HandwritingFont() const;

private:
    struct TextureLink {
        ue::UObject* object = nullptr;
        std::int32_t index = -1;
        void* resource = nullptr;
        IDirect3DTexture9* texture = nullptr;
        float width = 0.0F;
        float height = 0.0F;
    };

    [[nodiscard]] bool Validate(const TextureLink& link) const noexcept;
    [[nodiscard]] std::optional<TextureLink> Link(ue::UObject* texture) const;
    [[nodiscard]] std::optional<TextureLink> LinkByPath(std::string_view path) const;

    ue::Member<void*> resource_;
    ue::Member<std::int32_t> sizeX_;
    ue::Member<std::int32_t> sizeY_;
    ue::Member<ue::TArray<std::byte>> fontCharacters_;
    ue::Member<ue::TArray<ue::UObject*>> fontTextures_;
    ue::Member<std::int32_t> fontKerning_;
    bool available_ = false;
    std::chrono::steady_clock::time_point nextRetry_{};

    std::array<TextureLink, static_cast<std::size_t>(SpriteId::Count)> links_{};
    std::array<Sprite, static_cast<std::size_t>(SpriteId::Count)> sprites_{};
};

}
