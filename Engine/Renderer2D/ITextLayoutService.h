#pragma once

#include "Renderer2D/DrawTypes2D.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace Engine {

struct TextLayoutBitmap {
    int width = 0;
    int height = 0;
    float rasterScale = 1.0f;
    std::vector<std::uint8_t> bgraPremultiplied;
    // Logical x coordinates indexed by Unicode scalar position.
    std::vector<float> caretStops;
};

class ITextLayoutService {
public:
    virtual ~ITextLayoutService() = default;

    // Called by render pipelines before UI layout is built for a viewport.
    // Implementations may select a higher internal raster density than the
    // framebuffer DPI to preserve small-text edge quality.
    virtual void PrepareForDpiScale(float dpiScale) { (void)dpiScale; }

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
