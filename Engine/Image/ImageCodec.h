#pragma once

#include "Image/ImageBuffer.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Engine {

struct ImageResizeOptions;

enum class ImageEncodedFormat : std::uint8_t {
    Unknown,
    Png,
    Jpeg,
    Bmp,
    Gif,
};

[[nodiscard]] const char* ToString(ImageEncodedFormat format) noexcept;
[[nodiscard]] ImageEncodedFormat DetectEncodedFormat(
    std::span<const std::byte> bytes) noexcept;

struct ImageEncodedPayload {
    ImageEncodedFormat format = ImageEncodedFormat::Unknown;
    std::vector<std::byte> bytes;
};

struct ImageMetadata {
    ImageEncodedFormat encodedFormat = ImageEncodedFormat::Unknown;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frameCount = 0;
    ImageAlphaMode alphaMode = ImageAlphaMode::Straight;
    ImageColorSpace colorSpace = ImageColorSpace::Unknown;
    ImageOrientation orientation = ImageOrientation::TopDown;
    std::uint64_t encodedBytes = 0;
    bool animated = false;
};

struct ImageProbeOptions {
    ImageLimits limits{};
};

struct ImageDecodeOptions {
    ImageLimits limits{};
    bool firstFrameOnly = true;
};

struct ImageEncodeOptions {
    ImageLimits limits{};
};

class IImageCodec {
public:
    virtual ~IImageCodec() = default;

    virtual ApplicationResult<ImageBuffer> Decode(
        const ImageEncodedPayload& payload,
        const ImageDecodeOptions& options = {}) = 0;
    // Optional backend fast path for a bounded thumbnail.  The default keeps
    // older codecs source-compatible; ImageFileService falls back to Decode
    // plus the platform-neutral ResizeImage contract when unsupported.
    virtual ApplicationResult<ImageBuffer> DecodeThumbnail(
        const ImageEncodedPayload& payload,
        const ImageResizeOptions& resize,
        const ImageDecodeOptions& options = {}) {
        (void)payload;
        (void)resize;
        (void)options;
        return ApplicationResult<ImageBuffer>::Failure({
            ApplicationErrorCode::Unsupported, "DecodeThumbnail", "image_codec",
            "image codec thumbnail fast path is unavailable", std::nullopt,
            BackendName() == nullptr ? std::string{} : BackendName()});
    }
    // A backend may expose inexpensive container/frame metadata without
    // allocating the decoded pixel buffer.  Older/custom codecs can retain
    // the default Unsupported result and still implement Decode().
    virtual ApplicationResult<ImageMetadata> Probe(
        const ImageEncodedPayload& payload,
        const ImageProbeOptions& options = {}) {
        (void)payload;
        (void)options;
        return ApplicationResult<ImageMetadata>::Failure({
            ApplicationErrorCode::Unsupported, "ProbeImage", "image_codec",
            "image codec metadata probing is unavailable", std::nullopt,
            BackendName() == nullptr ? std::string{} : BackendName()});
    }
    virtual ApplicationResult<ImageEncodedPayload> Encode(
        const ImageBuffer& image,
        ImageEncodedFormat format = ImageEncodedFormat::Png,
        const ImageEncodeOptions& options = {}) = 0;

    [[nodiscard]] virtual bool Supports(
        ImageEncodedFormat format) const noexcept = 0;
    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

} // namespace Engine
