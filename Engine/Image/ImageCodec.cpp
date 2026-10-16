#include "Image/ImageCodec.h"

#include <cstring>

namespace Engine {

const char* ToString(ImageEncodedFormat format) noexcept {
    switch (format) {
    case ImageEncodedFormat::Unknown: return "unknown";
    case ImageEncodedFormat::Png: return "png";
    case ImageEncodedFormat::Jpeg: return "jpeg";
    case ImageEncodedFormat::Bmp: return "bmp";
    case ImageEncodedFormat::Gif: return "gif";
    }
    return "unknown";
}

ImageEncodedFormat DetectEncodedFormat(
    std::span<const std::byte> bytes) noexcept {
    static constexpr unsigned char png[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (bytes.size() >= sizeof(png) &&
        std::memcmp(bytes.data(), png, sizeof(png)) == 0)
        return ImageEncodedFormat::Png;
    if (bytes.size() >= 3 &&
        std::to_integer<unsigned char>(bytes[0]) == 0xff &&
        std::to_integer<unsigned char>(bytes[1]) == 0xd8 &&
        std::to_integer<unsigned char>(bytes[2]) == 0xff)
        return ImageEncodedFormat::Jpeg;
    if (bytes.size() >= 2 &&
        std::to_integer<unsigned char>(bytes[0]) == 0x42 &&
        std::to_integer<unsigned char>(bytes[1]) == 0x4d)
        return ImageEncodedFormat::Bmp;
    if (bytes.size() >= 6 &&
        (std::memcmp(bytes.data(), "GIF87a", 6) == 0 ||
         std::memcmp(bytes.data(), "GIF89a", 6) == 0))
        return ImageEncodedFormat::Gif;
    return ImageEncodedFormat::Unknown;
}

} // namespace Engine
