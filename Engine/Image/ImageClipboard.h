#pragma once

#include "Image/ImageBuffer.h"

#include <chrono>
#include <cstdint>

namespace Engine {

struct ImageClipboardCapture {
    ImageBuffer image;
    std::uint64_t sequenceNumber = 0;
};

struct ImageClipboardReadOptions {
    std::uint32_t maxAttempts = 4;
    std::chrono::milliseconds retryDelay{8};
    ImageLimits limits{};
};

class IImageClipboardSource {
public:
    virtual ~IImageClipboardSource() = default;

    // Synchronous read.  A platform implementation may require its creating
    // thread (for example, the Windows clipboard owner thread); it returns
    // WrongThread instead of silently touching the platform from another
    // thread.
    virtual ApplicationResult<ImageClipboardCapture> ReadImage(
        const ImageClipboardReadOptions& options = {}) = 0;
    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

} // namespace Engine
