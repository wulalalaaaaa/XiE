#pragma once

#include "Application/ApplicationResult.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Engine {

// ImageBuffer is the canonical CPU-side image container for application
// exchange.  It is deliberately separate from TextureRegistry2D: textures
// are renderer resources, while this type owns a platform-neutral pixel
// payload and its interpretation.
enum class ImagePixelFormat : std::uint8_t {
    BGRA8,
    RGBA8,
};

enum class ImageAlphaMode : std::uint8_t {
    Opaque,
    Straight,
    Premultiplied,
};

enum class ImageColorSpace : std::uint8_t {
    Unknown,
    SRGB,
};

// ImageBuffer data is always stored top-down.  Bottom-up data must be
// normalized by the importer before it becomes an ImageBuffer.
enum class ImageOrientation : std::uint8_t {
    TopDown,
    BottomUp,
};

struct ImageDescriptor {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t rowStride = 0;
    ImagePixelFormat pixelFormat = ImagePixelFormat::BGRA8;
    ImageAlphaMode alphaMode = ImageAlphaMode::Straight;
    ImageColorSpace colorSpace = ImageColorSpace::Unknown;
    ImageOrientation orientation = ImageOrientation::TopDown;

    friend constexpr bool operator==(
        const ImageDescriptor&, const ImageDescriptor&) = default;
};

struct ImageLimits {
    std::uint32_t maxDimension = 16'384;
    std::uint64_t maxBytes = 512ull * 1024ull * 1024ull;
    std::uint64_t maxEncodedBytes = 64ull * 1024ull * 1024ull;
};

struct ImageView {
    ImageDescriptor descriptor{};
    std::span<const std::byte> bytes{};
};

[[nodiscard]] const char* ToString(ImagePixelFormat format) noexcept;
[[nodiscard]] const char* ToString(ImageAlphaMode mode) noexcept;
[[nodiscard]] const char* ToString(ImageColorSpace colorSpace) noexcept;
[[nodiscard]] const char* ToString(ImageOrientation orientation) noexcept;

// Validates dimensions, format, canonical orientation, row stride and the
// complete byte count before any allocation or pointer arithmetic occurs.
ApplicationResult<std::size_t> ValidateImageLayout(
    const ImageDescriptor& descriptor,
    const ImageLimits& limits = {});

class ImageBuffer final {
public:
    ImageBuffer() = default;
    ~ImageBuffer() = default;
    ImageBuffer(const ImageBuffer&) = default;
    ImageBuffer& operator=(const ImageBuffer&) = default;
    ImageBuffer(ImageBuffer&&) noexcept = default;
    ImageBuffer& operator=(ImageBuffer&&) noexcept = default;

    static ApplicationResult<ImageBuffer> Allocate(
        ImageDescriptor descriptor,
        const ImageLimits& limits = {});
    static ApplicationResult<ImageBuffer> Copy(
        ImageDescriptor descriptor,
        std::span<const std::byte> bytes,
        const ImageLimits& limits = {});

    [[nodiscard]] const ImageDescriptor& Descriptor() const noexcept {
        return m_Descriptor;
    }
    [[nodiscard]] ImageView View() const noexcept {
        return {m_Descriptor, m_Bytes};
    }
    [[nodiscard]] std::span<const std::byte> Bytes() const noexcept {
        return m_Bytes;
    }
    // The returned span is borrowed from this owning buffer and remains valid
    // only while the buffer is not moved, assigned or destroyed.
    [[nodiscard]] std::span<std::byte> MutableBytes() noexcept {
        return m_Bytes;
    }
    [[nodiscard]] std::size_t ByteSize() const noexcept {
        return m_Bytes.size();
    }
    [[nodiscard]] bool Empty() const noexcept { return m_Bytes.empty(); }

    [[nodiscard]] std::uint32_t Width() const noexcept {
        return m_Descriptor.width;
    }
    [[nodiscard]] std::uint32_t Height() const noexcept {
        return m_Descriptor.height;
    }
    [[nodiscard]] std::uint32_t RowStride() const noexcept {
        return m_Descriptor.rowStride;
    }
    [[nodiscard]] ImagePixelFormat PixelFormat() const noexcept {
        return m_Descriptor.pixelFormat;
    }
    [[nodiscard]] ImageAlphaMode AlphaMode() const noexcept {
        return m_Descriptor.alphaMode;
    }
    [[nodiscard]] ImageColorSpace ColorSpace() const noexcept {
        return m_Descriptor.colorSpace;
    }
    [[nodiscard]] ImageOrientation Orientation() const noexcept {
        return m_Descriptor.orientation;
    }

private:
    ImageDescriptor m_Descriptor{};
    std::vector<std::byte> m_Bytes;
};

} // namespace Engine
