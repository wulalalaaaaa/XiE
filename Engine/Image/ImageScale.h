#pragma once

#include "Image/ImageBuffer.h"

namespace Engine {

enum class ImageResizeFilter : std::uint8_t {
    Nearest,
    Linear,
};

struct ImageResizeOptions {
    std::uint32_t maxWidth = 256;
    std::uint32_t maxHeight = 256;
    bool allowUpscale = false;
    ImageResizeFilter filter = ImageResizeFilter::Linear;
    ImageLimits limits{};
};

// Returns an owned, top-down ImageBuffer.  The source format, alpha mode and
// color-space metadata are preserved; the renderer/TextureRegistry remains a
// separate owner and conversion boundary.
ApplicationResult<ImageBuffer> ResizeImage(
    const ImageBuffer& source,
    const ImageResizeOptions& options = {});

} // namespace Engine
