#include "assets/GameAssets.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "ue/Globals.h"
#include "ue/Reflection.h"

#include <Windows.h>
#include <d3d9.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>

namespace lis::assets {

namespace {

struct Region {
    std::string_view texture;
    float x0;
    float y0;
    float x1;
    float y1;
};

struct SpriteDefinition {
    SpriteId id;
    std::array<Region, 2> candidates;
};

constexpr std::array kSpriteDefinitions{
    SpriteDefinition{SpriteId::Paper,
                     {{{"SN_PauseMenu.Flash.HowToPlayPageImg", 8, 16, 1224, 944},
                       {"sn_outgamesensenmenu.Flash.HowToPlayPageImg", 8, 16, 1224, 944}}}},
    SpriteDefinition{SpriteId::NavStrip,
                     {{{"SN_PauseMenu.Flash.HowToPlayTitleBackgroundImg", 8, 16, 604, 232},
                       {"SN_SensenMenu.Flash.WhiteBubbleImg", 256, 16, 612, 204}}}},
    SpriteDefinition{SpriteId::Tape, {{{"SN_SensenMenu.Flash.WhiteBubbleImg", 264, 688, 548, 784}, {}}}},
    SpriteDefinition{SpriteId::StickyNote, {{{"SN_SensenMenu.Flash.WhiteBubbleImg", 12, 16, 240, 240}, {}}}},
    SpriteDefinition{SpriteId::Star, {{{"SN_SensenMenu.Flash.WhiteBubbleImg", 624, 8, 752, 136}, {}}}},
    SpriteDefinition{SpriteId::Logo, {{{"SN_Buttons.Flash.TooltipBkg", 8, 832, 644, 972}, {}}}},
};

constexpr std::string_view kHandwritingFont = "Font DNE_Fonts.DialogFont";
constexpr std::uint16_t kFirstGlyph = 32;
constexpr std::uint16_t kLastGlyph = 126;
constexpr std::size_t kFontCharacterSize = 0x18;
constexpr std::uintptr_t kTextureRhiOffset = 0x14;
constexpr std::uintptr_t kInterfaceOffset = 0x4;
constexpr std::chrono::seconds kRetryInterval{2};

bool IsDirect3DObject(void* candidate) noexcept {
    static const HMODULE direct3d = GetModuleHandleW(L"d3d9.dll");
    if (direct3d == nullptr || !core::IsReadable(candidate, sizeof(void*))) {
        return false;
    }
    void* vtable = *static_cast<void**>(candidate);
    MEMORY_BASIC_INFORMATION info{};
    return core::IsReadable(vtable, sizeof(void*)) && VirtualQuery(vtable, &info, sizeof(info)) != 0 &&
           info.AllocationBase == direct3d;
}

IDirect3DTexture9* AsTexture(void* candidate) noexcept {
    if (!IsDirect3DObject(candidate)) {
        return nullptr;
    }
    IDirect3DTexture9* texture = nullptr;
    if (FAILED(static_cast<IUnknown*>(candidate)->QueryInterface(__uuidof(IDirect3DTexture9),
                                                                 reinterpret_cast<void**>(&texture)))) {
        return nullptr;
    }
    texture->Release();
    return texture;
}

template <typename T>
T ReadAt(const std::byte* base, std::size_t offset) noexcept {
    T value{};
    std::memcpy(&value, base + offset, sizeof(T));
    return value;
}

bool HasSize(IDirect3DTexture9* texture, std::int32_t width, std::int32_t height) noexcept {
    D3DSURFACE_DESC description{};
    return SUCCEEDED(texture->GetLevelDesc(0, &description)) && static_cast<std::int32_t>(description.Width) == width &&
           static_cast<std::int32_t>(description.Height) == height;
}

IDirect3DTexture9* FindRenderTexture(void* resource, std::int32_t width, std::int32_t height) noexcept {
    const auto renderObject =
        core::SafeRead<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(resource) + kTextureRhiOffset);
    if (!renderObject || *renderObject < kInterfaceOffset) {
        return nullptr;
    }
    const auto candidate = core::SafeRead<void*>(*renderObject - kInterfaceOffset);
    if (!candidate) {
        return nullptr;
    }
    IDirect3DTexture9* texture = AsTexture(*candidate);
    return texture != nullptr && HasSize(texture, width, height) ? texture : nullptr;
}
}

GameAssets::GameAssets() {
    ue::SchemaBuilder schema;
    resource_ = schema.Field<void*>(schema.Class("Engine.Texture"), "Resource");
    ue::UClass* texture2d = schema.Class("Engine.Texture2D");
    sizeX_ = schema.Field<std::int32_t>(texture2d, "SizeX");
    sizeY_ = schema.Field<std::int32_t>(texture2d, "SizeY");
    ue::UClass* font = schema.Class("Engine.Font");
    fontCharacters_ = schema.Field<ue::TArray<std::byte>>(font, "Characters");
    fontTextures_ = schema.Field<ue::TArray<ue::UObject*>>(font, "Textures");
    fontKerning_ = schema.Field<std::int32_t>(font, "Kerning");
    available_ = schema.Ok();
    if (auto finished = schema.Finish(); !finished) {
        core::Log::Warning("game interface assets disabled: {}", finished.error());
    }
}

bool GameAssets::Validate(const TextureLink& link) const noexcept {
    return link.texture != nullptr && ue::IsAlive(link.object, link.index) &&
           resource_.Of(link.object) == link.resource;
}

std::optional<GameAssets::TextureLink> GameAssets::Link(ue::UObject* texture) const {
    if (texture == nullptr) {
        return std::nullopt;
    }
    void* resource = resource_.Of(texture);
    if (resource == nullptr) {
        return std::nullopt;
    }
    IDirect3DTexture9* renderTexture = FindRenderTexture(resource, sizeX_.Of(texture), sizeY_.Of(texture));
    if (renderTexture == nullptr) {
        return std::nullopt;
    }
    D3DSURFACE_DESC description{};
    if (FAILED(renderTexture->GetLevelDesc(0, &description))) {
        return std::nullopt;
    }
    return TextureLink{.object = texture,
                       .index = texture->index,
                       .resource = resource,
                       .texture = renderTexture,
                       .width = static_cast<float>(description.Width),
                       .height = static_cast<float>(description.Height)};
}

std::optional<GameAssets::TextureLink> GameAssets::LinkByPath(std::string_view path) const {
    if (path.empty()) {
        return std::nullopt;
    }
    return Link(ue::FindObject(std::format("Texture2D {}", path)));
}

void GameAssets::Refresh() {
    if (!available_) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const bool retry = now >= nextRetry_;
    for (const SpriteDefinition& definition : kSpriteDefinitions) {
        const auto slot = static_cast<std::size_t>(definition.id);
        if (Validate(links_[slot])) {
            continue;
        }
        links_[slot] = {};
        sprites_[slot] = {};
        if (!retry) {
            continue;
        }
        for (const Region& region : definition.candidates) {
            const auto link = LinkByPath(region.texture);
            if (!link) {
                continue;
            }
            links_[slot] = *link;
            sprites_[slot] = Sprite{.texture = link->texture,
                                    .u0 = region.x0 / link->width,
                                    .v0 = region.y0 / link->height,
                                    .u1 = region.x1 / link->width,
                                    .v1 = region.y1 / link->height,
                                    .width = region.x1 - region.x0,
                                    .height = region.y1 - region.y0};
            break;
        }
    }
    if (retry) {
        nextRetry_ = now + kRetryInterval;
    }
}

std::optional<FontSource> GameAssets::HandwritingFont() const {
    if (!available_) {
        return std::nullopt;
    }
    ue::UObject* font = ue::FindObject(kHandwritingFont);
    if (font == nullptr) {
        return std::nullopt;
    }
    FontSource source;
    for (ue::UObject* page : fontTextures_.Of(font).View()) {
        const auto link = Link(page);
        if (!link) {
            return std::nullopt;
        }
        source.pages.push_back(link->texture);
    }
    const ue::TArray<std::byte>& characters = fontCharacters_.Of(font);
    const std::int32_t kerning = fontKerning_.Of(font);
    for (std::uint16_t codepoint = kFirstGlyph; codepoint <= kLastGlyph; ++codepoint) {
        if (codepoint >= characters.Size()) {
            break;
        }
        const std::byte* entry = characters.data + static_cast<std::size_t>(codepoint) * kFontCharacterSize;
        Glyph glyph{.codepoint = codepoint,
                    .page = ReadAt<std::uint8_t>(entry, 0x10),
                    .x = ReadAt<std::int32_t>(entry, 0x0),
                    .y = ReadAt<std::int32_t>(entry, 0x4),
                    .width = ReadAt<std::int32_t>(entry, 0x8),
                    .height = ReadAt<std::int32_t>(entry, 0xC)};
        glyph.advance = glyph.width + kerning;
        if (glyph.page < source.pages.size() && glyph.width > 0 && glyph.height > 0) {
            source.lineHeight = std::max(source.lineHeight, static_cast<float>(glyph.height));
            source.glyphs.push_back(glyph);
        }
    }
    if (source.glyphs.empty()) {
        return std::nullopt;
    }
    return source;
}

}
