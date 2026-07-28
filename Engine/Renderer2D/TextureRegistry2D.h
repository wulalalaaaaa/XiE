#pragma once

#include "DrawTypes2D.h"

#include <cstdint>
#include <span>
#include <vector>

namespace Engine {

struct TexturePixels2D {
    TextureInfo info{};
    std::vector<std::uint8_t> rgba;
};

class TextureRegistry2D final : public ITextureInfoProvider2D {
public:
    TextureRegistry2D();

    TextureHandle CreateTextureRGBA8(
        int width,
        int height,
        std::span<const std::uint8_t> pixels,
        AlphaMode alphaMode);
    TextureHandle CreateTextureState(
        int width,
        int height,
        AlphaMode alphaMode,
        TextureState state);
    bool UpdateTextureRGBA8(
        TextureHandle handle,
        int width,
        int height,
        std::span<const std::uint8_t> pixels,
        AlphaMode alphaMode);
    bool UpdateTextureState(
        TextureHandle handle,
        int width,
        int height,
        AlphaMode alphaMode,
        TextureState state);
    void Destroy(TextureHandle handle);

    [[nodiscard]] const TexturePixels2D* Get(TextureHandle handle) const;
    [[nodiscard]] TextureInfo GetInfo(TextureHandle handle) const override;
    [[nodiscard]] TextureHandle MissingTexture() const;
    [[nodiscard]] std::uint64_t Revision() const override { return m_Revision; }

private:
    struct Slot {
        TexturePixels2D texture;
        std::uint32_t generation = 1;
        bool occupied = false;
    };

    TextureHandle Allocate(const TexturePixels2D& texture);

private:
    std::vector<Slot> m_Slots;
    TextureHandle m_MissingTexture{};
    std::uint64_t m_Revision = 0;
};

} // namespace Engine
