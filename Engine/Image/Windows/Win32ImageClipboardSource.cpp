#include "Image/Windows/Win32ImageClipboardSource.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "Image/ImageCodec.h"
#include "Image/Windows/WicImageCodec.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <limits>
#include <string>
#include <thread>
#include <utility>

namespace Engine {
namespace {

constexpr std::uint32_t kBitmapInfoHeaderSize = 40;
constexpr std::uint32_t kBitmapV4HeaderSize = 108;
constexpr std::uint32_t kBitmapV5HeaderSize = 124;
constexpr std::uint32_t kBiRgb = 0;
constexpr std::uint32_t kBiBitfields = 3;
constexpr std::uint32_t kBiAlphaBitfields = 6;
constexpr std::uint32_t kLcsSrgb = 0x73524742u;

ApplicationError Error(ApplicationErrorCode code, std::string operation,
    std::string message, std::optional<std::int64_t> nativeCode = {}) {
    return {code, std::move(operation), "image_clipboard", std::move(message),
        nativeCode, "win32_clipboard"};
}

template <typename T>
ApplicationResult<T> Failure(ApplicationErrorCode code,
    std::string operation, std::string message,
    std::optional<std::int64_t> nativeCode = {}) {
    return ApplicationResult<T>::Failure(Error(code, std::move(operation),
        std::move(message), nativeCode));
}

bool ReadU16(std::span<const std::byte> bytes, std::size_t offset,
    std::uint16_t& value) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(value))
        return false;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return true;
}

bool ReadU32(std::span<const std::byte> bytes, std::size_t offset,
    std::uint32_t& value) {
    if (offset > bytes.size() || bytes.size() - offset < sizeof(value))
        return false;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return true;
}

bool ReadI32(std::span<const std::byte> bytes, std::size_t offset,
    std::int32_t& value) {
    std::uint32_t raw = 0;
    if (!ReadU32(bytes, offset, raw)) return false;
    std::memcpy(&value, &raw, sizeof(value));
    return true;
}

struct MaskInfo {
    std::uint32_t mask = 0;
    unsigned shift = 0;
    std::uint32_t maximum = 0;
};

bool MakeMaskInfo(std::uint32_t mask, MaskInfo& result) {
    if (mask == 0) return false;
    const unsigned shift = std::countr_zero(mask);
    const std::uint32_t shifted = mask >> shift;
    // A DIB channel mask must describe one contiguous channel.  Rejecting
    // sparse masks avoids silently changing malformed pixel encodings.
    if ((shifted & (shifted + 1u)) != 0) return false;
    const unsigned bits = std::popcount(shifted);
    result = {mask, shift,
        bits == 32 ? std::numeric_limits<std::uint32_t>::max()
                   : static_cast<std::uint32_t>((std::uint64_t{1} << bits) - 1)};
    return true;
}

std::uint8_t ExtractChannel(std::uint32_t pixel, const MaskInfo& mask) {
    const std::uint64_t value = (pixel & mask.mask) >> mask.shift;
    return static_cast<std::uint8_t>((value * 255u + mask.maximum / 2u) /
        mask.maximum);
}

ApplicationResult<ImageBuffer> DecodeDib(
    Win32ClipboardImageFormat format, std::span<const std::byte> bytes,
    const ImageLimits& limits) {
    if (bytes.empty()) return Failure<ImageBuffer>(ApplicationErrorCode::NoData,
        "DecodeClipboardDib", "DIB payload is empty");
    if (limits.maxDimension == 0 || limits.maxEncodedBytes == 0 ||
        limits.maxBytes == 0) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidOptions,
            "DecodeClipboardDib", "image limits must be non-zero");
    }
    if (bytes.size() > limits.maxEncodedBytes) {
        return Failure<ImageBuffer>(ApplicationErrorCode::TooLarge,
            "DecodeClipboardDib", "DIB payload exceeds the configured limit");
    }
    std::uint32_t headerSize = 0;
    std::int32_t widthSigned = 0;
    std::int32_t heightSigned = 0;
    std::uint16_t planes = 0;
    std::uint16_t bitCount = 0;
    std::uint32_t compression = 0;
    std::uint32_t sizeImage = 0;
    std::uint32_t colorsUsed = 0;
    if (!ReadU32(bytes, 0, headerSize) ||
        !ReadI32(bytes, 4, widthSigned) ||
        !ReadI32(bytes, 8, heightSigned) ||
        !ReadU16(bytes, 12, planes) ||
        !ReadU16(bytes, 14, bitCount) ||
        !ReadU32(bytes, 16, compression) ||
        !ReadU32(bytes, 20, sizeImage) ||
        !ReadU32(bytes, 32, colorsUsed)) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeClipboardDib", "DIB header is truncated");
    }
    if (headerSize < kBitmapInfoHeaderSize || headerSize > bytes.size() ||
        headerSize > 4096 || planes != 1 || widthSigned <= 0 ||
        heightSigned == 0 || heightSigned == std::numeric_limits<std::int32_t>::min() ||
        (bitCount != 24 && bitCount != 32) ||
        (compression != kBiRgb && compression != kBiBitfields &&
         compression != kBiAlphaBitfields) || colorsUsed != 0) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeClipboardDib", "DIB header values are invalid or unsupported");
    }
    if (format == Win32ClipboardImageFormat::DibV5 &&
        headerSize < kBitmapV5HeaderSize) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeClipboardDib", "CF_DIBV5 does not contain a BITMAPV5HEADER");
    }
    if (format == Win32ClipboardImageFormat::Dib &&
        headerSize != kBitmapInfoHeaderSize &&
        headerSize != kBitmapV4HeaderSize &&
        headerSize != kBitmapV5HeaderSize) {
        return Failure<ImageBuffer>(ApplicationErrorCode::Unsupported,
            "DecodeClipboardDib", "DIB header variant is not supported");
    }

    const auto width = static_cast<std::uint32_t>(widthSigned);
    const auto height = static_cast<std::uint32_t>(
        heightSigned < 0 ? -static_cast<std::int64_t>(heightSigned)
                         : static_cast<std::int64_t>(heightSigned));
    if (width > limits.maxDimension || height > limits.maxDimension) {
        return Failure<ImageBuffer>(ApplicationErrorCode::TooLarge,
            "DecodeClipboardDib", "DIB dimensions exceed the configured limit");
    }
    const std::uint64_t bitsPerRow = static_cast<std::uint64_t>(width) * bitCount;
    if (bitsPerRow > std::numeric_limits<std::uint64_t>::max() - 31u) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeClipboardDib", "DIB row layout overflows or is truncated");
    }
    const std::uint64_t sourceStride = ((bitsPerRow + 31u) / 32u) * 4u;
    if (sourceStride > std::numeric_limits<std::uint64_t>::max() / height) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeClipboardDib", "DIB row layout overflows or is truncated");
    }
    const std::uint64_t sourceBytes = sourceStride * height;
    if (sourceBytes > bytes.size()) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeClipboardDib", "DIB row layout overflows or is truncated");
    }
    if (sizeImage != 0 && sizeImage < sourceBytes) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeClipboardDib", "DIB biSizeImage is smaller than its rows");
    }

    std::size_t pixelOffset = headerSize;
    MaskInfo red{};
    MaskInfo green{};
    MaskInfo blue{};
    MaskInfo alpha{};
    bool hasAlphaMask = false;
    if (compression == kBiBitfields || compression == kBiAlphaBitfields) {
        if (bitCount != 32) {
            return Failure<ImageBuffer>(ApplicationErrorCode::Unsupported,
                "DecodeClipboardDib", "bitfields are supported only for 32-bit DIBs");
        }
        std::uint32_t redMask = 0;
        std::uint32_t greenMask = 0;
        std::uint32_t blueMask = 0;
        std::uint32_t alphaMask = 0;
        if (headerSize >= kBitmapV4HeaderSize) {
            if (!ReadU32(bytes, 40, redMask) || !ReadU32(bytes, 44, greenMask) ||
                !ReadU32(bytes, 48, blueMask)) {
                return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
                    "DecodeClipboardDib", "DIB color masks are truncated");
            }
            if (headerSize >= kBitmapV5HeaderSize) {
                if (!ReadU32(bytes, 52, alphaMask)) {
                    return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
                        "DecodeClipboardDib", "DIB alpha mask is truncated");
                }
            }
        } else {
            const std::size_t maskBytes = compression == kBiAlphaBitfields
                ? 16u : 12u;
            if (bytes.size() - pixelOffset < maskBytes ||
                !ReadU32(bytes, pixelOffset + 0, redMask) ||
                !ReadU32(bytes, pixelOffset + 4, greenMask) ||
                !ReadU32(bytes, pixelOffset + 8, blueMask)) {
                return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
                    "DecodeClipboardDib", "DIB trailing color masks are truncated");
            }
            if (compression == kBiAlphaBitfields &&
                !ReadU32(bytes, pixelOffset + 12, alphaMask)) {
                return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
                    "DecodeClipboardDib", "DIB trailing alpha mask is truncated");
            }
            pixelOffset += maskBytes;
        }
        if (!MakeMaskInfo(redMask, red) || !MakeMaskInfo(greenMask, green) ||
            !MakeMaskInfo(blueMask, blue) ||
            ((red.mask & green.mask) != 0) ||
            ((red.mask & blue.mask) != 0) ||
            ((green.mask & blue.mask) != 0)) {
            return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
                "DecodeClipboardDib", "DIB color masks are invalid or overlap");
        }
        if (alphaMask != 0) {
            if (!MakeMaskInfo(alphaMask, alpha) ||
                (alpha.mask & (red.mask | green.mask | blue.mask)) != 0) {
                return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
                    "DecodeClipboardDib", "DIB alpha mask is invalid or overlaps");
            }
            hasAlphaMask = true;
        }
    }
    if (pixelOffset > bytes.size() ||
        sourceBytes > bytes.size() - pixelOffset) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeClipboardDib", "DIB pixel payload is truncated");
    }

    if (width > std::numeric_limits<std::uint32_t>::max() / 4u) {
        return Failure<ImageBuffer>(ApplicationErrorCode::TooLarge,
            "DecodeClipboardDib", "DIB output row stride is too large");
    }
    ImageDescriptor descriptor;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.rowStride = width * 4u;
    descriptor.pixelFormat = ImagePixelFormat::BGRA8;
    descriptor.alphaMode = hasAlphaMask
        ? ImageAlphaMode::Straight : ImageAlphaMode::Opaque;
    descriptor.colorSpace = ImageColorSpace::Unknown;
    descriptor.orientation = ImageOrientation::TopDown;
    if (headerSize >= kBitmapV5HeaderSize) {
        std::uint32_t colorSpace = 0;
        if (ReadU32(bytes, 56, colorSpace) && colorSpace == kLcsSrgb)
            descriptor.colorSpace = ImageColorSpace::SRGB;
    }
    auto image = ImageBuffer::Allocate(descriptor, limits);
    if (!image) return image;
    auto target = image.Value().MutableBytes();
    const auto* sourceBase = bytes.data() + pixelOffset;
    const bool topDown = heightSigned < 0;
    const std::size_t sourcePixelBytes = bitCount / 8u;
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint32_t sourceY = topDown ? y : height - 1u - y;
        const auto* sourceRow = sourceBase +
            static_cast<std::size_t>(sourceY * sourceStride);
        auto* targetRow = target.data() +
            static_cast<std::size_t>(y) * descriptor.rowStride;
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto* sourcePixel = sourceRow +
                static_cast<std::size_t>(x) * sourcePixelBytes;
            if (compression == kBiRgb) {
                targetRow[x * 4 + 0] = sourcePixel[0];
                targetRow[x * 4 + 1] = sourcePixel[1];
                targetRow[x * 4 + 2] = sourcePixel[2];
                targetRow[x * 4 + 3] = std::byte{0xff};
            } else {
                std::uint32_t pixel = 0;
                std::memcpy(&pixel, sourcePixel, sizeof(pixel));
                targetRow[x * 4 + 0] = std::byte{ExtractChannel(pixel, blue)};
                targetRow[x * 4 + 1] = std::byte{ExtractChannel(pixel, green)};
                targetRow[x * 4 + 2] = std::byte{ExtractChannel(pixel, red)};
                targetRow[x * 4 + 3] = hasAlphaMask
                    ? std::byte{ExtractChannel(pixel, alpha)}
                    : std::byte{0xff};
            }
        }
    }
    return image;
}

class NativeWin32ClipboardProvider final : public IWin32ClipboardProvider {
public:
    ApplicationResult<Win32ClipboardSnapshot> Read(
        const ImageClipboardReadOptions& options) override {
        if (options.maxAttempts == 0 || options.retryDelay.count() < 0 ||
            options.limits.maxDimension == 0 ||
            options.limits.maxEncodedBytes == 0 ||
            options.limits.maxBytes == 0) {
            return Failure<Win32ClipboardSnapshot>(
                ApplicationErrorCode::InvalidOptions, "ReadClipboardImage",
                "clipboard retry options are invalid");
        }
        bool opened = false;
        for (std::uint32_t attempt = 0; attempt < options.maxAttempts; ++attempt) {
            if (OpenClipboard(nullptr) != FALSE) {
                opened = true;
                break;
            }
            if (attempt + 1 < options.maxAttempts) {
                std::this_thread::sleep_for(options.retryDelay * (attempt + 1));
            }
        }
        if (!opened) return Failure<Win32ClipboardSnapshot>(
            ApplicationErrorCode::Busy, "ReadClipboardImage",
            "Windows clipboard remained busy after finite retries",
            static_cast<std::int64_t>(GetLastError()));
        struct CloseGuard final {
            ~CloseGuard() { CloseClipboard(); }
        } guard;

        Win32ClipboardSnapshot result;
        result.sequenceNumber = static_cast<std::uint64_t>(
            GetClipboardSequenceNumber());
        const UINT png = RegisterClipboardFormatW(L"PNG");
        struct Candidate {
            UINT nativeFormat;
            Win32ClipboardImageFormat format;
        };
        const Candidate candidates[] = {
            {png, Win32ClipboardImageFormat::Png},
            {CF_DIBV5, Win32ClipboardImageFormat::DibV5},
            {CF_DIB, Win32ClipboardImageFormat::Dib},
        };
        for (const auto& candidate : candidates) {
            if (candidate.nativeFormat == 0 ||
                IsClipboardFormatAvailable(candidate.nativeFormat) == FALSE)
                continue;
            result.hasSupportedFormat = true;
            const HANDLE handle = GetClipboardData(candidate.nativeFormat);
            if (handle == nullptr) continue;
            const SIZE_T size = GlobalSize(handle);
            if (size == 0) continue;
            if (static_cast<std::uint64_t>(size) > options.limits.maxEncodedBytes) {
                result.hasOversizedPayload = true;
                continue;
            }
            void* locked = GlobalLock(handle);
            if (locked == nullptr) continue;
            try {
                Win32ClipboardPayload payload;
                payload.format = candidate.format;
                const auto* begin = reinterpret_cast<const std::byte*>(locked);
                payload.bytes.assign(begin, begin + size);
                result.payloads.push_back(std::move(payload));
            } catch (const std::exception& exception) {
                GlobalUnlock(handle);
                return Failure<Win32ClipboardSnapshot>(
                    ApplicationErrorCode::TooLarge, "ReadClipboardImage",
                    exception.what());
            }
            GlobalUnlock(handle);
        }
        result.hasUnsupportedImageFormat =
            IsClipboardFormatAvailable(CF_BITMAP) != FALSE ||
            IsClipboardFormatAvailable(CF_ENHMETAFILE) != FALSE ||
            IsClipboardFormatAvailable(CF_METAFILEPICT) != FALSE;
        return ApplicationResult<Win32ClipboardSnapshot>::Success(
            std::move(result));
    }
};

ApplicationError SelectReadError(const std::vector<ApplicationError>& errors) {
    if (errors.empty()) return Error(ApplicationErrorCode::InvalidData,
        "ReadClipboardImage", "clipboard image payload could not be decoded");
    const auto select = [&](ApplicationErrorCode code) -> const ApplicationError* {
        for (const auto& error : errors) if (error.code == code) return &error;
        return nullptr;
    };
    for (const auto code : {ApplicationErrorCode::TooLarge,
        ApplicationErrorCode::BackendUnavailable,
        ApplicationErrorCode::DecodeFailed, ApplicationErrorCode::InvalidData,
        ApplicationErrorCode::Unsupported}) {
        if (const auto* error = select(code)) return *error;
    }
    return errors.front();
}

} // namespace

ApplicationResult<ImageBuffer> DecodeWin32BitmapPayload(
    Win32ClipboardImageFormat format, std::span<const std::byte> bytes,
    const ImageLimits& limits) {
    if (format == Win32ClipboardImageFormat::Png) {
        return Failure<ImageBuffer>(ApplicationErrorCode::Unsupported,
            "DecodeClipboardImage", "PNG is decoded by IImageCodec");
    }
    return DecodeDib(format, bytes, limits);
}

Win32ImageClipboardSource::Win32ImageClipboardSource(
    std::shared_ptr<IImageCodec> codec,
    std::shared_ptr<IWin32ClipboardProvider> provider)
    : m_Codec(std::move(codec)),
      m_Provider(provider ? std::move(provider)
                          : std::make_shared<NativeWin32ClipboardProvider>()),
      m_OwnerThread(std::this_thread::get_id()) {}

ApplicationResult<ImageClipboardCapture> Win32ImageClipboardSource::ReadImage(
    const ImageClipboardReadOptions& options) {
    if (std::this_thread::get_id() != m_OwnerThread) {
        return Failure<ImageClipboardCapture>(ApplicationErrorCode::WrongThread,
            "ReadClipboardImage", "Windows clipboard access must use the source thread");
    }
    if (options.maxAttempts == 0 || options.retryDelay.count() < 0 ||
        options.limits.maxDimension == 0 ||
        options.limits.maxEncodedBytes == 0 ||
        options.limits.maxBytes == 0) {
        return Failure<ImageClipboardCapture>(ApplicationErrorCode::InvalidOptions,
            "ReadClipboardImage", "clipboard retry options are invalid");
    }
    if (!m_Provider) return Failure<ImageClipboardCapture>(
        ApplicationErrorCode::BackendUnavailable, "ReadClipboardImage",
        "Windows clipboard provider is unavailable");
    auto snapshotResult = m_Provider->Read(options);
    if (!snapshotResult) return ApplicationResult<ImageClipboardCapture>::Failure(
        snapshotResult.Error());
    auto snapshot = std::move(snapshotResult.Value());
    if (snapshot.payloads.empty()) {
        if (snapshot.hasOversizedPayload) return Failure<ImageClipboardCapture>(
            ApplicationErrorCode::TooLarge, "ReadClipboardImage",
            "clipboard image payload exceeds the configured limit");
        if (snapshot.hasSupportedFormat) return Failure<ImageClipboardCapture>(
            ApplicationErrorCode::InvalidData, "ReadClipboardImage",
            "clipboard image payload is empty or malformed");
        if (snapshot.hasUnsupportedImageFormat) return Failure<ImageClipboardCapture>(
            ApplicationErrorCode::Unsupported, "ReadClipboardImage",
            "clipboard contains an image format without a supported importer");
        return Failure<ImageClipboardCapture>(ApplicationErrorCode::NoData,
            "ReadClipboardImage", "clipboard contains no supported image");
    }

    std::vector<ApplicationError> errors;
    for (auto& payload : snapshot.payloads) {
        ApplicationResult<ImageBuffer> decoded;
        if (payload.format == Win32ClipboardImageFormat::Png) {
            if (!m_Codec) {
                errors.push_back(Error(ApplicationErrorCode::BackendUnavailable,
                    "ReadClipboardImage", "PNG image codec is unavailable"));
                continue;
            }
            ImageEncodedPayload encoded;
            encoded.format = ImageEncodedFormat::Png;
            encoded.bytes = std::move(payload.bytes);
            decoded = m_Codec->Decode(encoded,
                ImageDecodeOptions{options.limits});
        } else {
            decoded = DecodeWin32BitmapPayload(payload.format, payload.bytes,
                options.limits);
        }
        if (decoded) {
            return ApplicationResult<ImageClipboardCapture>::Success({
                std::move(decoded.Value()), snapshot.sequenceNumber});
        }
        errors.push_back(decoded.Error());
    }
    return ApplicationResult<ImageClipboardCapture>::Failure(
        SelectReadError(errors));
}

} // namespace Engine

#else

namespace Engine {

ApplicationResult<ImageBuffer> DecodeWin32BitmapPayload(
    Win32ClipboardImageFormat, std::span<const std::byte>, const ImageLimits&) {
    return ApplicationResult<ImageBuffer>::Failure({
        ApplicationErrorCode::BackendUnavailable, "DecodeClipboardImage",
        "image_clipboard", "Win32 clipboard is unavailable on this platform",
        std::nullopt, "win32_clipboard"});
}

Win32ImageClipboardSource::Win32ImageClipboardSource(
    std::shared_ptr<IImageCodec> codec,
    std::shared_ptr<IWin32ClipboardProvider> provider)
    : m_Codec(std::move(codec)), m_Provider(std::move(provider)),
      m_OwnerThread(std::this_thread::get_id()) {}

ApplicationResult<ImageClipboardCapture> Win32ImageClipboardSource::ReadImage(
    const ImageClipboardReadOptions&) {
    return ApplicationResult<ImageClipboardCapture>::Failure({
        ApplicationErrorCode::BackendUnavailable, "ReadClipboardImage",
        "image_clipboard", "Win32 clipboard is unavailable on this platform",
        std::nullopt, "win32_clipboard"});
}

} // namespace Engine

#endif
