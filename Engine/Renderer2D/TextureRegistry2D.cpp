#include "TextureRegistry2D.h"

#include <algorithm>

namespace Engine {

TextureRegistry2D::TextureRegistry2D() {
    constexpr std::uint8_t pixels[] = {
        255, 0, 255, 255, 32, 32, 32, 255,
        32, 32, 32, 255, 255, 0, 255, 255
    };
    m_MissingTexture = CreateTextureRGBA8(2, 2, pixels, AlphaMode::Straight);
}

TextureHandle TextureRegistry2D::CreateTextureRGBA8(
    int width,
    int height,
    std::span<const std::uint8_t> pixels,
    AlphaMode alphaMode
) {
    if (width <= 0 || height <= 0 || pixels.size() < static_cast<std::size_t>(width * height * 4)) {
        return m_MissingTexture;
    }

    TexturePixels2D texture{};
    texture.info.width = width;
    texture.info.height = height;
    texture.info.alphaMode = alphaMode;
    texture.info.state = TextureState::Ready;
    texture.rgba.assign(pixels.begin(), pixels.begin() + static_cast<std::ptrdiff_t>(width * height * 4));
    return Allocate(texture);
}

TextureHandle TextureRegistry2D::CreateTextureState(
    int width, int height, AlphaMode alphaMode, TextureState state) {
    if (state == TextureState::Ready || width < 0 || height < 0) return {};
    TexturePixels2D texture{};
    texture.info.width = width;
    texture.info.height = height;
    texture.info.alphaMode = alphaMode;
    texture.info.state = state;
    return Allocate(texture);
}

bool TextureRegistry2D::UpdateTextureRGBA8(
    TextureHandle handle,
    int width,
    int height,
    std::span<const std::uint8_t> pixels,
    AlphaMode alphaMode
) {
    if (!handle.IsValid() || handle == m_MissingTexture || handle.index > m_Slots.size() ||
        width <= 0 || height <= 0 ||
        pixels.size() < static_cast<std::size_t>(width * height * 4)) return false;
    Slot& slot = m_Slots[handle.index - 1];
    if (!slot.occupied || slot.generation != handle.generation) return false;
    slot.texture.info.width = width;
    slot.texture.info.height = height;
    slot.texture.info.alphaMode = alphaMode;
    slot.texture.info.state = TextureState::Ready;
    slot.texture.rgba.assign(
        pixels.begin(), pixels.begin() + static_cast<std::ptrdiff_t>(width * height * 4));
    slot.texture.info.revision = ++m_Revision;
    return true;
}

bool TextureRegistry2D::UpdateTextureState(
    TextureHandle handle,
    int width,
    int height,
    AlphaMode alphaMode,
    TextureState state
) {
    if (!handle.IsValid() || handle == m_MissingTexture || handle.index > m_Slots.size() ||
        state == TextureState::Ready || width < 0 || height < 0) return false;
    Slot& slot = m_Slots[handle.index - 1];
    if (!slot.occupied || slot.generation != handle.generation) return false;
    slot.texture.info.width = width;
    slot.texture.info.height = height;
    slot.texture.info.alphaMode = alphaMode;
    slot.texture.info.state = state;
    slot.texture.rgba.clear();
    slot.texture.info.revision = ++m_Revision;
    return true;
}

void TextureRegistry2D::Destroy(TextureHandle handle) {
    if (!handle.IsValid() || handle.index > m_Slots.size()) {
        return;
    }

    Slot& slot = m_Slots[handle.index - 1];
    if (!slot.occupied || slot.generation != handle.generation) {
        return;
    }

    slot.texture = {};
    slot.occupied = false;
    ++slot.generation;
    if (slot.generation == 0) {
        slot.generation = 1;
    }
    ++m_Revision;
}

const TexturePixels2D* TextureRegistry2D::Get(TextureHandle handle) const {
    if (!handle.IsValid() || handle.index > m_Slots.size()) {
        return Get(m_MissingTexture);
    }

    const Slot& slot = m_Slots[handle.index - 1];
    if (!slot.occupied || slot.generation != handle.generation) {
        if (handle == m_MissingTexture) {
            return nullptr;
        }
        return Get(m_MissingTexture);
    }
    return &slot.texture;
}

TextureInfo TextureRegistry2D::GetInfo(TextureHandle handle) const {
    if (!handle.IsValid() || handle.index > m_Slots.size()) {
        TextureInfo info{};
        info.state = TextureState::Missing;
        return info;
    }
    const Slot& slot = m_Slots[handle.index - 1];
    if (!slot.occupied || slot.generation != handle.generation) {
        TextureInfo info{};
        info.state = TextureState::Missing;
        return info;
    }
    return slot.texture.info;
}

TextureHandle TextureRegistry2D::MissingTexture() const {
    return m_MissingTexture;
}

TextureHandle TextureRegistry2D::Allocate(const TexturePixels2D& texture) {
    auto it = std::find_if(m_Slots.begin(), m_Slots.end(), [](const Slot& slot) {
        return !slot.occupied;
    });

    if (it == m_Slots.end()) {
        m_Slots.push_back({});
        it = m_Slots.end() - 1;
    }

    it->texture = texture;
    it->occupied = true;
    if (it->generation == 0) {
        it->generation = 1;
    }
    it->texture.info.revision = ++m_Revision;

    return TextureHandle{
        static_cast<std::uint32_t>(std::distance(m_Slots.begin(), it) + 1),
        it->generation
    };
}

} // namespace Engine
