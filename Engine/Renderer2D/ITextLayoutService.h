#pragma once

#include "Renderer2D/DrawTypes2D.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Engine {

struct TextLayoutBitmap {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgraPremultiplied;
};

class ITextLayoutService {
public:
    virtual ~ITextLayoutService() = default;

    virtual TextLayoutHandle CreateLayout(
        FontHandle font,
        std::u32string_view text,
        float maxWidth,
        float fontSize) = 0;
    virtual const TextLayoutBitmap* GetBitmap(TextLayoutHandle handle) const = 0;
    virtual std::uint32_t RasterizeCount() const = 0;
    virtual std::uint64_t Revision() const { return RasterizeCount(); }
};

} // namespace Engine
