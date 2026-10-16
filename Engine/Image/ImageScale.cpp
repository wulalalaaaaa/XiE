#include "Image/ImageScale.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

namespace Engine {
namespace {

ApplicationError Error(ApplicationErrorCode code, std::string operation,
    std::string message) {
    return {code, std::move(operation), "image_scale", std::move(message),
        std::nullopt, "xie_image"};
}

template <typename T>
ApplicationResult<T> Failure(ApplicationErrorCode code,
    std::string operation, std::string message) {
    return ApplicationResult<T>::Failure(
        Error(code, std::move(operation), std::move(message)));
}

std::uint8_t ByteAt(std::span<const std::byte> bytes,
    std::uint32_t stride, std::uint32_t x, std::uint32_t y,
    std::uint32_t channel) {
    const auto offset = static_cast<std::size_t>(y) * stride +
        static_cast<std::size_t>(x) * 4u + channel;
    return std::to_integer<std::uint8_t>(bytes[offset]);
}

} // namespace

ApplicationResult<ImageBuffer> ResizeImage(
    const ImageBuffer& source, const ImageResizeOptions& options) {
    if (options.maxWidth == 0 || options.maxHeight == 0) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidOptions,
            "ResizeImage", "resize bounds must be non-zero");
    }
    const auto sourceLayout = ValidateImageLayout(
        source.Descriptor(), options.limits);
    if (!sourceLayout) return ApplicationResult<ImageBuffer>::Failure(
        sourceLayout.Error());
    if (source.PixelFormat() != ImagePixelFormat::BGRA8 &&
        source.PixelFormat() != ImagePixelFormat::RGBA8) {
        return Failure<ImageBuffer>(ApplicationErrorCode::Unsupported,
            "ResizeImage", "only 8-bit four-channel images can be resized");
    }

    const double widthScale = static_cast<double>(options.maxWidth) /
        static_cast<double>(source.Width());
    const double heightScale = static_cast<double>(options.maxHeight) /
        static_cast<double>(source.Height());
    double scale = std::min(widthScale, heightScale);
    if (!options.allowUpscale) scale = std::min(1.0, scale);
    if (!(scale > 0.0) || !std::isfinite(scale)) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidOptions,
            "ResizeImage", "resize scale is not finite");
    }

    const auto targetWidth = static_cast<std::uint32_t>(std::clamp(
        std::llround(static_cast<double>(source.Width()) * scale), 1ll,
        static_cast<long long>(options.maxWidth)));
    const auto targetHeight = static_cast<std::uint32_t>(std::clamp(
        std::llround(static_cast<double>(source.Height()) * scale), 1ll,
        static_cast<long long>(options.maxHeight)));

    ImageDescriptor descriptor = source.Descriptor();
    descriptor.width = targetWidth;
    descriptor.height = targetHeight;
    if (targetWidth > std::numeric_limits<std::uint32_t>::max() / 4u) {
        return Failure<ImageBuffer>(ApplicationErrorCode::TooLarge,
            "ResizeImage", "resized row stride overflows 32-bit layout");
    }
    descriptor.rowStride = targetWidth * 4u;
    descriptor.orientation = ImageOrientation::TopDown;
    auto result = ImageBuffer::Allocate(descriptor, options.limits);
    if (!result) return result;
    if (targetWidth == source.Width() && targetHeight == source.Height()) {
        return ImageBuffer::Copy(descriptor, source.Bytes(), options.limits);
    }

    const auto sourceBytes = source.Bytes();
    auto targetBytes = result.Value().MutableBytes();
    for (std::uint32_t y = 0; y < targetHeight; ++y) {
        const double sourceY =
            (static_cast<double>(y) + 0.5) * source.Height() /
                targetHeight - 0.5;
        const auto y0 = static_cast<std::uint32_t>(std::clamp(
            static_cast<long long>(std::floor(std::max(0.0, sourceY))),
            0ll, static_cast<long long>(source.Height() - 1)));
        const auto y1 = static_cast<std::uint32_t>(std::clamp(
            static_cast<long long>(std::ceil(std::max(0.0, sourceY))),
            0ll, static_cast<long long>(source.Height() - 1)));
        const double fy = std::clamp(sourceY - std::floor(sourceY), 0.0, 1.0);
        for (std::uint32_t x = 0; x < targetWidth; ++x) {
            const double sourceX =
                (static_cast<double>(x) + 0.5) * source.Width() /
                    targetWidth - 0.5;
            const auto x0 = static_cast<std::uint32_t>(std::clamp(
                static_cast<long long>(std::floor(std::max(0.0, sourceX))),
                0ll, static_cast<long long>(source.Width() - 1)));
            const auto x1 = static_cast<std::uint32_t>(std::clamp(
                static_cast<long long>(std::ceil(std::max(0.0, sourceX))),
                0ll, static_cast<long long>(source.Width() - 1)));
            const double fx = std::clamp(
                sourceX - std::floor(sourceX), 0.0, 1.0);
            auto* pixel = targetBytes.data() +
                static_cast<std::size_t>(y) * descriptor.rowStride +
                static_cast<std::size_t>(x) * 4u;
            for (std::uint32_t channel = 0; channel < 4; ++channel) {
                if (options.filter == ImageResizeFilter::Nearest) {
                    const auto nx = static_cast<std::uint32_t>(std::clamp(
                        static_cast<long long>(std::floor(
                            static_cast<double>(x) * source.Width() /
                                targetWidth)),
                        0ll, static_cast<long long>(source.Width() - 1)));
                    const auto ny = static_cast<std::uint32_t>(std::clamp(
                        static_cast<long long>(std::floor(
                            static_cast<double>(y) * source.Height() /
                                targetHeight)),
                        0ll, static_cast<long long>(source.Height() - 1)));
                    pixel[channel] = std::byte{ByteAt(
                        sourceBytes, source.RowStride(), nx, ny, channel)};
                    continue;
                }
                const double top = (1.0 - fx) * ByteAt(
                    sourceBytes, source.RowStride(), x0, y0, channel) +
                    fx * ByteAt(sourceBytes, source.RowStride(), x1, y0, channel);
                const double bottom = (1.0 - fx) * ByteAt(
                    sourceBytes, source.RowStride(), x0, y1, channel) +
                    fx * ByteAt(sourceBytes, source.RowStride(), x1, y1, channel);
                const auto value = static_cast<unsigned char>(std::clamp(
                    std::llround((1.0 - fy) * top + fy * bottom), 0ll, 255ll));
                pixel[channel] = std::byte{value};
            }
        }
    }
    return result;
}

} // namespace Engine
