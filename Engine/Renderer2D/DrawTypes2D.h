#pragma once

#include "Foundation/Handles/GenerationalHandle.h"
#include "Foundation/Math/Types2D.h"

#include <cstdint>

namespace Engine {

enum class BlendMode {
    Opaque,
    Alpha,
    Additive
};

struct TextureHandleTag;
struct FontHandleTag;
struct TextLayoutHandleTag;
using TextureHandle = GenerationalHandle<TextureHandleTag>;
using FontHandle = GenerationalHandle<FontHandleTag>;
using TextLayoutHandle = GenerationalHandle<TextLayoutHandleTag>;

enum class TextureState {
    Loading,
    Ready,
    Missing,
    Failed
};

struct TextureInfo {
    int width = 0;
    int height = 0;
    AlphaMode alphaMode = AlphaMode::Premultiplied;
    TextureState state = TextureState::Loading;
    std::uint64_t revision = 0;
};

class ITextureInfoProvider2D {
public:
    virtual ~ITextureInfoProvider2D() = default;
    [[nodiscard]] virtual TextureInfo GetInfo(TextureHandle handle) const = 0;
    [[nodiscard]] virtual std::uint64_t Revision() const { return 0; }
};

struct Render2DView {
    float framebufferWidth = 0.0f;
    float framebufferHeight = 0.0f;
    float dpiScale = 1.0f;
};

struct Render2DStats {
    std::uint32_t commandCount = 0;
    std::uint32_t drawCallCount = 0;
    std::uint32_t quadCount = 0;
    std::uint32_t textureBindCount = 0;
    std::uint32_t clipChangeCount = 0;
};

} // namespace Engine
