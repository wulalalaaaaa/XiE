#include "Image/ImageBuffer.h"

#include <exception>
#include <limits>
#include <new>
#include <string>

namespace Engine {
namespace {

ApplicationError Error(
    ApplicationErrorCode code, std::string operation, std::string message) {
    return {code, std::move(operation), "image", std::move(message),
        std::nullopt, "xie_image"};
}

template <typename T>
ApplicationResult<T> Failure(ApplicationErrorCode code,
    std::string operation, std::string message) {
    return ApplicationResult<T>::Failure(
        Error(code, std::move(operation), std::move(message)));
}

ApplicationResult<std::uint64_t> BytesPerPixel(ImagePixelFormat format) {
    switch (format) {
    case ImagePixelFormat::BGRA8:
    case ImagePixelFormat::RGBA8:
        return ApplicationResult<std::uint64_t>::Success(4);
    }
    return Failure<std::uint64_t>(ApplicationErrorCode::Unsupported,
        "ValidateImageLayout", "pixel format is not supported");
}

} // namespace

const char* ToString(ImagePixelFormat format) noexcept {
    switch (format) {
    case ImagePixelFormat::BGRA8: return "bgra8";
    case ImagePixelFormat::RGBA8: return "rgba8";
    }
    return "unknown";
}

const char* ToString(ImageAlphaMode mode) noexcept {
    switch (mode) {
    case ImageAlphaMode::Opaque: return "opaque";
    case ImageAlphaMode::Straight: return "straight";
    case ImageAlphaMode::Premultiplied: return "premultiplied";
    }
    return "unknown";
}

const char* ToString(ImageColorSpace colorSpace) noexcept {
    switch (colorSpace) {
    case ImageColorSpace::Unknown: return "unknown";
    case ImageColorSpace::SRGB: return "srgb";
    }
    return "unknown";
}

const char* ToString(ImageOrientation orientation) noexcept {
    switch (orientation) {
    case ImageOrientation::TopDown: return "top_down";
    case ImageOrientation::BottomUp: return "bottom_up";
    }
    return "unknown";
}

ApplicationResult<std::size_t> ValidateImageLayout(
    const ImageDescriptor& descriptor, const ImageLimits& limits) {
    if (limits.maxDimension == 0 || limits.maxBytes == 0) {
        return Failure<std::size_t>(ApplicationErrorCode::InvalidOptions,
            "ValidateImageLayout", "image limits must be non-zero");
    }
    if (descriptor.width == 0 || descriptor.height == 0) {
        return Failure<std::size_t>(ApplicationErrorCode::InvalidData,
            "ValidateImageLayout", "image dimensions must be non-zero");
    }
    if (descriptor.width > limits.maxDimension ||
        descriptor.height > limits.maxDimension) {
        return Failure<std::size_t>(ApplicationErrorCode::TooLarge,
            "ValidateImageLayout", "image dimensions exceed the configured limit");
    }
    if (descriptor.orientation != ImageOrientation::TopDown) {
        return Failure<std::size_t>(ApplicationErrorCode::Unsupported,
            "ValidateImageLayout", "ImageBuffer requires top-down orientation");
    }

    const auto bytesPerPixel = BytesPerPixel(descriptor.pixelFormat);
    if (!bytesPerPixel) return bytesPerPixel;
    const auto minimumStride =
        static_cast<std::uint64_t>(descriptor.width) * bytesPerPixel.Value();
    if (descriptor.rowStride < minimumStride) {
        return Failure<std::size_t>(ApplicationErrorCode::InvalidData,
            "ValidateImageLayout", "row stride is smaller than one image row");
    }
    const auto stride = static_cast<std::uint64_t>(descriptor.rowStride);
    const auto height = static_cast<std::uint64_t>(descriptor.height);
    if (stride > std::numeric_limits<std::uint64_t>::max() / height) {
        return Failure<std::size_t>(ApplicationErrorCode::TooLarge,
            "ValidateImageLayout", "image byte count overflows 64-bit size");
    }
    const auto total = stride * height;
    if (total > limits.maxBytes) {
        return Failure<std::size_t>(ApplicationErrorCode::TooLarge,
            "ValidateImageLayout", "image byte count exceeds the configured limit");
    }
    if (total > std::numeric_limits<std::size_t>::max()) {
        return Failure<std::size_t>(ApplicationErrorCode::TooLarge,
            "ValidateImageLayout", "image byte count does not fit this process");
    }
    if (total > static_cast<std::uint64_t>(
            std::numeric_limits<std::ptrdiff_t>::max())) {
        return Failure<std::size_t>(ApplicationErrorCode::TooLarge,
            "ValidateImageLayout", "image byte count exceeds iterator limits");
    }
    return ApplicationResult<std::size_t>::Success(
        static_cast<std::size_t>(total));
}

ApplicationResult<ImageBuffer> ImageBuffer::Allocate(
    ImageDescriptor descriptor, const ImageLimits& limits) {
    const auto size = ValidateImageLayout(descriptor, limits);
    if (!size) return ApplicationResult<ImageBuffer>::Failure(size.Error());
    try {
        ImageBuffer result;
        result.m_Descriptor = descriptor;
        result.m_Bytes.resize(size.Value());
        return ApplicationResult<ImageBuffer>::Success(std::move(result));
    } catch (const std::exception& exception) {
        return ApplicationResult<ImageBuffer>::Failure({
            ApplicationErrorCode::TooLarge, "AllocateImageBuffer", "image",
            exception.what(), std::nullopt, "xie_image"});
    }
}

ApplicationResult<ImageBuffer> ImageBuffer::Copy(
    ImageDescriptor descriptor, std::span<const std::byte> bytes,
    const ImageLimits& limits) {
    const auto size = ValidateImageLayout(descriptor, limits);
    if (!size) return ApplicationResult<ImageBuffer>::Failure(size.Error());
    if (bytes.size() < size.Value()) {
        return ApplicationResult<ImageBuffer>::Failure({
            ApplicationErrorCode::InvalidData, "CopyImageBuffer", "image",
            "pixel payload is shorter than the declared layout", std::nullopt,
            "xie_image"});
    }
    try {
        ImageBuffer result;
        result.m_Descriptor = descriptor;
        result.m_Bytes.assign(bytes.begin(), bytes.begin() +
            static_cast<std::ptrdiff_t>(size.Value()));
        return ApplicationResult<ImageBuffer>::Success(std::move(result));
    } catch (const std::exception& exception) {
        return ApplicationResult<ImageBuffer>::Failure({
            ApplicationErrorCode::TooLarge, "CopyImageBuffer", "image",
            exception.what(), std::nullopt, "xie_image"});
    }
}

} // namespace Engine
