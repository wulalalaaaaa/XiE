#pragma once

#include "Image/ImageCodec.h"

namespace Engine {

// Windows Imaging Component is an optional implementation of the portable
// IImageCodec contract.  WIC/COM types intentionally remain private to the
// .cpp file.
class WicImageCodec final : public IImageCodec {
public:
    ApplicationResult<ImageBuffer> Decode(
        const ImageEncodedPayload& payload,
        const ImageDecodeOptions& options = {}) override;
    ApplicationResult<ImageBuffer> DecodeThumbnail(
        const ImageEncodedPayload& payload,
        const ImageResizeOptions& resize,
        const ImageDecodeOptions& options = {}) override;
    ApplicationResult<ImageMetadata> Probe(
        const ImageEncodedPayload& payload,
        const ImageProbeOptions& options = {}) override;
    ApplicationResult<ImageEncodedPayload> Encode(
        const ImageBuffer& image,
        ImageEncodedFormat format = ImageEncodedFormat::Png,
        const ImageEncodeOptions& options = {}) override;
    [[nodiscard]] bool Supports(
        ImageEncodedFormat format) const noexcept override;
    [[nodiscard]] const char* BackendName() const noexcept override {
        return "wic";
    }
};

} // namespace Engine
