#include "Image/Windows/WicImageCodec.h"

#include "Image/ImageScale.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <objidl.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace Engine {
namespace {

using Microsoft::WRL::ComPtr;

ApplicationError Error(ApplicationErrorCode code, std::string operation,
    std::string message, HRESULT nativeCode = S_OK) {
    std::optional<std::int64_t> native;
    if (nativeCode != S_OK) native = static_cast<std::int64_t>(nativeCode);
    return {code, std::move(operation), "image_codec", std::move(message),
        native, "wic"};
}

template <typename T>
ApplicationResult<T> Failure(ApplicationErrorCode code,
    std::string operation, std::string message, HRESULT nativeCode = S_OK) {
    return ApplicationResult<T>::Failure(
        Error(code, std::move(operation), std::move(message), nativeCode));
}

class ComApartmentScope final {
public:
    ComApartmentScope() {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        m_Usable = SUCCEEDED(result) || result == RPC_E_CHANGED_MODE;
        // RPC_E_CHANGED_MODE means the caller already selected a different
        // apartment.  COM calls are still usable; this scope must not undo
        // the caller's initialization.
        m_ShouldUninitialize = result == S_OK || result == S_FALSE;
        m_Result = result;
    }

    ~ComApartmentScope() {
        if (m_ShouldUninitialize) CoUninitialize();
    }

    [[nodiscard]] bool Usable() const noexcept { return m_Usable; }
    [[nodiscard]] HRESULT Result() const noexcept { return m_Result; }

private:
    HRESULT m_Result = E_FAIL;
    bool m_Usable = false;
    bool m_ShouldUninitialize = false;
};

ApplicationResult<void> CheckEncodedPayload(
    const ImageEncodedPayload& payload, const ImageLimits& limits) {
    if (payload.bytes.empty()) {
        return Failure<void>(ApplicationErrorCode::NoData, "DecodeImage",
            "encoded image payload is empty");
    }
    if (limits.maxDimension == 0 || limits.maxBytes == 0 ||
        limits.maxEncodedBytes == 0) {
        return Failure<void>(ApplicationErrorCode::InvalidOptions,
            "DecodeImage", "encoded image limit must be non-zero");
    }
    if (payload.bytes.size() > limits.maxEncodedBytes) {
        return Failure<void>(ApplicationErrorCode::TooLarge, "DecodeImage",
            "encoded image payload exceeds the configured limit");
    }
    if (payload.bytes.size() > std::numeric_limits<DWORD>::max()) {
        return Failure<void>(ApplicationErrorCode::TooLarge, "DecodeImage",
            "encoded image payload is too large for WIC memory streams");
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<ImageBuffer> DecodeEncoded(
    const ImageEncodedPayload& payload, const ImageDecodeOptions& options) {
    const auto valid = CheckEncodedPayload(payload, options.limits);
    if (!valid) return ApplicationResult<ImageBuffer>::Failure(valid.Error());

    ComApartmentScope apartment;
    if (!apartment.Usable()) {
        return Failure<ImageBuffer>(ApplicationErrorCode::BackendUnavailable,
            "DecodeImage", "WIC requires a usable COM apartment",
            apartment.Result());
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) {
        return Failure<ImageBuffer>(ApplicationErrorCode::BackendUnavailable,
            "DecodeImage", "WIC factory creation failed", hr);
    }

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(stream.GetAddressOf());
    if (FAILED(hr)) {
        return Failure<ImageBuffer>(ApplicationErrorCode::DecodeFailed,
            "DecodeImage", "WIC stream creation failed", hr);
    }
    hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(payload.bytes.data())),
        static_cast<DWORD>(payload.bytes.size()));
    if (FAILED(hr)) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeImage", "WIC rejected the encoded image payload", hr);
    }

    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateDecoderFromStream(stream.Get(), nullptr,
        WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    UINT frameCount = 0;
    if (SUCCEEDED(hr)) hr = decoder->GetFrameCount(&frameCount);
    if (SUCCEEDED(hr) && frameCount > 1 && !options.firstFrameOnly) {
        return Failure<ImageBuffer>(ApplicationErrorCode::Unsupported,
            "DecodeImage", "multi-frame images require explicit animation support");
    }
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(
        converter.GetAddressOf());
    if (SUCCEEDED(hr)) hr = converter->Initialize(frame.Get(),
        GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return Failure<ImageBuffer>(ApplicationErrorCode::DecodeFailed,
            "DecodeImage", "WIC could not decode the encoded image", hr);
    }

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr)) {
        return Failure<ImageBuffer>(ApplicationErrorCode::DecodeFailed,
            "DecodeImage", "WIC did not provide image dimensions", hr);
    }
    if (width == 0 || height == 0) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeImage", "decoded image dimensions are empty");
    }
    if (width > options.limits.maxDimension ||
        height > options.limits.maxDimension) {
        return Failure<ImageBuffer>(ApplicationErrorCode::TooLarge,
            "DecodeImage", "decoded image dimensions exceed the configured limit");
    }

    const std::uint64_t stride64 = static_cast<std::uint64_t>(width) * 4;
    if (stride64 > std::numeric_limits<std::uint32_t>::max()) {
        return Failure<ImageBuffer>(ApplicationErrorCode::TooLarge,
            "DecodeImage", "decoded row stride is too large");
    }
    ImageDescriptor descriptor;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.rowStride = static_cast<std::uint32_t>(stride64);
    descriptor.pixelFormat = ImagePixelFormat::BGRA8;
    descriptor.colorSpace = ImageColorSpace::Unknown;
    descriptor.orientation = ImageOrientation::TopDown;

    auto image = ImageBuffer::Allocate(descriptor, options.limits);
    if (!image) return image;
    if (image.Value().ByteSize() > std::numeric_limits<UINT>::max()) {
        return Failure<ImageBuffer>(ApplicationErrorCode::TooLarge,
            "DecodeImage", "decoded image is too large for WIC CopyPixels");
    }
    hr = converter->CopyPixels(nullptr, descriptor.rowStride,
        static_cast<UINT>(image.Value().ByteSize()),
        reinterpret_cast<BYTE*>(image.Value().MutableBytes().data()));
    if (FAILED(hr)) {
        return Failure<ImageBuffer>(ApplicationErrorCode::DecodeFailed,
            "DecodeImage", "WIC pixel copy failed", hr);
    }

    bool hasTransparentPixel = false;
    const auto bytes = image.Value().Bytes();
    for (std::size_t offset = 3; offset < bytes.size(); offset += 4) {
        if (std::to_integer<unsigned>(bytes[offset]) != 255) {
            hasTransparentPixel = true;
            break;
        }
    }
    // The converter normalizes straight BGRA8.  A fully opaque result is
    // marked Opaque; this is semantic metadata, not a second pixel format.
    auto normalized = image.Value().Descriptor();
    normalized.alphaMode = hasTransparentPixel
        ? ImageAlphaMode::Straight : ImageAlphaMode::Opaque;
    auto owned = ImageBuffer::Copy(normalized, image.Value().Bytes(),
        options.limits);
    if (!owned) return owned;
    return owned;
}

ApplicationResult<ImageBuffer> DecodeThumbnailEncoded(
    const ImageEncodedPayload& payload,
    const ImageResizeOptions& resize,
    const ImageDecodeOptions& options) {
    const auto valid = CheckEncodedPayload(payload, options.limits);
    if (!valid) return ApplicationResult<ImageBuffer>::Failure(valid.Error());
    if (resize.maxWidth == 0 || resize.maxHeight == 0) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidOptions,
            "DecodeThumbnail", "thumbnail bounds must be non-zero");
    }

    ComApartmentScope apartment;
    if (!apartment.Usable()) {
        return Failure<ImageBuffer>(ApplicationErrorCode::BackendUnavailable,
            "DecodeThumbnail", "WIC requires a usable COM apartment",
            apartment.Result());
    }
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) return Failure<ImageBuffer>(
        ApplicationErrorCode::BackendUnavailable, "DecodeThumbnail",
        "WIC factory creation failed", hr);

    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(stream.GetAddressOf());
    if (SUCCEEDED(hr)) hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(payload.bytes.data())),
        static_cast<DWORD>(payload.bytes.size()));
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(stream.Get(),
        nullptr, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    UINT frameCount = 0;
    if (SUCCEEDED(hr)) hr = decoder->GetFrameCount(&frameCount);
    if (SUCCEEDED(hr) && frameCount > 1 && !options.firstFrameOnly) {
        return Failure<ImageBuffer>(ApplicationErrorCode::Unsupported,
            "DecodeThumbnail", "multi-frame images require explicit animation support");
    }
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) return Failure<ImageBuffer>(
        ApplicationErrorCode::DecodeFailed, "DecodeThumbnail",
        "WIC could not decode the thumbnail source", hr);

    UINT sourceWidth = 0;
    UINT sourceHeight = 0;
    hr = frame->GetSize(&sourceWidth, &sourceHeight);
    if (FAILED(hr) || sourceWidth == 0 || sourceHeight == 0) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeThumbnail", "thumbnail source dimensions are empty", hr);
    }
    if (sourceWidth > options.limits.maxDimension ||
        sourceHeight > options.limits.maxDimension) {
        return Failure<ImageBuffer>(ApplicationErrorCode::TooLarge,
            "DecodeThumbnail", "source dimensions exceed the configured limit");
    }

    const double widthScale = static_cast<double>(resize.maxWidth) /
        static_cast<double>(sourceWidth);
    const double heightScale = static_cast<double>(resize.maxHeight) /
        static_cast<double>(sourceHeight);
    double scale = std::min(widthScale, heightScale);
    if (!resize.allowUpscale) scale = std::min(1.0, scale);
    if (!(scale > 0.0) || !std::isfinite(scale)) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidOptions,
            "DecodeThumbnail", "thumbnail scale is not finite");
    }
    const auto targetWidth = static_cast<UINT>(std::clamp(
        std::llround(static_cast<double>(sourceWidth) * scale), 1ll,
        static_cast<long long>(resize.maxWidth)));
    const auto targetHeight = static_cast<UINT>(std::clamp(
        std::llround(static_cast<double>(sourceHeight) * scale), 1ll,
        static_cast<long long>(resize.maxHeight)));

    ComPtr<IWICBitmapScaler> scaler;
    IWICBitmapSource* bitmap = frame.Get();
    if (targetWidth != sourceWidth || targetHeight != sourceHeight) {
        if (FAILED(factory->CreateBitmapScaler(scaler.GetAddressOf()))) {
            return Failure<ImageBuffer>(ApplicationErrorCode::DecodeFailed,
                "DecodeThumbnail", "WIC thumbnail scaler creation failed", hr);
        }
        const auto interpolation = resize.filter == ImageResizeFilter::Nearest
            ? WICBitmapInterpolationModeNearestNeighbor
            : WICBitmapInterpolationModeFant;
        hr = scaler->Initialize(frame.Get(), targetWidth, targetHeight,
            interpolation);
        if (FAILED(hr)) return Failure<ImageBuffer>(
            ApplicationErrorCode::DecodeFailed, "DecodeThumbnail",
            "WIC thumbnail scaling failed", hr);
        bitmap = scaler.Get();
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(converter.GetAddressOf());
    if (SUCCEEDED(hr)) hr = converter->Initialize(bitmap,
        GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return Failure<ImageBuffer>(
        ApplicationErrorCode::DecodeFailed, "DecodeThumbnail",
        "WIC thumbnail format conversion failed", hr);

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        return Failure<ImageBuffer>(ApplicationErrorCode::InvalidData,
            "DecodeThumbnail", "WIC thumbnail dimensions are empty", hr);
    }
    const auto stride64 = static_cast<std::uint64_t>(width) * 4u;
    if (stride64 > std::numeric_limits<std::uint32_t>::max()) {
        return Failure<ImageBuffer>(ApplicationErrorCode::TooLarge,
            "DecodeThumbnail", "thumbnail row stride is too large");
    }
    ImageDescriptor descriptor;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.rowStride = static_cast<std::uint32_t>(stride64);
    descriptor.pixelFormat = ImagePixelFormat::BGRA8;
    descriptor.orientation = ImageOrientation::TopDown;
    auto image = ImageBuffer::Allocate(descriptor, resize.limits);
    if (!image) return image;
    if (image.Value().ByteSize() > std::numeric_limits<UINT>::max()) {
        return Failure<ImageBuffer>(ApplicationErrorCode::TooLarge,
            "DecodeThumbnail", "thumbnail is too large for WIC CopyPixels");
    }
    hr = converter->CopyPixels(nullptr, descriptor.rowStride,
        static_cast<UINT>(image.Value().ByteSize()),
        reinterpret_cast<BYTE*>(image.Value().MutableBytes().data()));
    if (FAILED(hr)) return Failure<ImageBuffer>(
        ApplicationErrorCode::DecodeFailed, "DecodeThumbnail",
        "WIC thumbnail pixel copy failed", hr);

    bool hasTransparentPixel = false;
    const auto bytes = image.Value().Bytes();
    for (std::size_t offset = 3; offset < bytes.size(); offset += 4) {
        if (std::to_integer<unsigned>(bytes[offset]) != 255) {
            hasTransparentPixel = true;
            break;
        }
    }
    auto normalized = image.Value().Descriptor();
    normalized.alphaMode = hasTransparentPixel
        ? ImageAlphaMode::Straight : ImageAlphaMode::Opaque;
    return ImageBuffer::Copy(normalized, image.Value().Bytes(), resize.limits);
}

ImageEncodedFormat ContainerFormat(const GUID& container) {
    if (container == GUID_ContainerFormatPng)
        return ImageEncodedFormat::Png;
    if (container == GUID_ContainerFormatJpeg)
        return ImageEncodedFormat::Jpeg;
    if (container == GUID_ContainerFormatBmp)
        return ImageEncodedFormat::Bmp;
    if (container == GUID_ContainerFormatGif)
        return ImageEncodedFormat::Gif;
    return ImageEncodedFormat::Unknown;
}

ApplicationResult<ImageMetadata> ProbeEncoded(
    const ImageEncodedPayload& payload, const ImageProbeOptions& options) {
    const auto valid = CheckEncodedPayload(payload, options.limits);
    if (!valid) return ApplicationResult<ImageMetadata>::Failure(valid.Error());
    ComApartmentScope apartment;
    if (!apartment.Usable()) {
        return Failure<ImageMetadata>(ApplicationErrorCode::BackendUnavailable,
            "ProbeImage", "WIC requires a usable COM apartment",
            apartment.Result());
    }
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) return Failure<ImageMetadata>(
        ApplicationErrorCode::BackendUnavailable, "ProbeImage",
        "WIC factory creation failed", hr);
    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(stream.GetAddressOf());
    if (SUCCEEDED(hr)) hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(payload.bytes.data())),
        static_cast<DWORD>(payload.bytes.size()));
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(stream.Get(),
        nullptr, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf());
    UINT frameCount = 0;
    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(hr)) hr = decoder->GetFrameCount(&frameCount);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (SUCCEEDED(hr)) hr = frame->GetSize(&width, &height);
    if (FAILED(hr)) return Failure<ImageMetadata>(
        ApplicationErrorCode::DecodeFailed, "ProbeImage",
        "WIC could not inspect the encoded image", hr);
    if (width == 0 || height == 0 || frameCount == 0)
        return Failure<ImageMetadata>(ApplicationErrorCode::InvalidData,
            "ProbeImage", "encoded image metadata is empty");
    if (width > options.limits.maxDimension ||
        height > options.limits.maxDimension) {
        return Failure<ImageMetadata>(ApplicationErrorCode::TooLarge,
            "ProbeImage", "image dimensions exceed the configured limit");
    }
    GUID container{};
    if (FAILED(decoder->GetContainerFormat(&container)))
        container = {};
    const auto format = ContainerFormat(container);
    WICPixelFormatGUID pixelFormat{};
    (void)frame->GetPixelFormat(&pixelFormat);
    ImageMetadata result;
    result.encodedFormat = format == ImageEncodedFormat::Unknown
        ? payload.format : format;
    result.width = width;
    result.height = height;
    result.frameCount = frameCount;
    result.alphaMode = result.encodedFormat == ImageEncodedFormat::Jpeg ||
        result.encodedFormat == ImageEncodedFormat::Bmp
        ? ImageAlphaMode::Opaque : ImageAlphaMode::Straight;
    result.orientation = ImageOrientation::TopDown;
    result.encodedBytes = payload.bytes.size();
    result.animated = result.encodedFormat == ImageEncodedFormat::Gif &&
        frameCount > 1;
    return ApplicationResult<ImageMetadata>::Success(std::move(result));
}

ApplicationResult<ImageEncodedPayload> EncodeEncoded(
    const ImageBuffer& image, ImageEncodedFormat outputFormat,
    const ImageEncodeOptions& options) {
    const auto layout = ValidateImageLayout(image.Descriptor(), options.limits);
    if (!layout) return ApplicationResult<ImageEncodedPayload>::Failure(
        layout.Error());
    if (options.limits.maxEncodedBytes == 0) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::InvalidOptions,
            "EncodeImage", "encoded image limit must be non-zero");
    }
    if (outputFormat != ImageEncodedFormat::Png &&
        outputFormat != ImageEncodedFormat::Jpeg) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::Unsupported,
            "EncodeImage", "WIC only encodes PNG and JPEG in this contract");
    }
    if (image.PixelFormat() != ImagePixelFormat::BGRA8 &&
        image.PixelFormat() != ImagePixelFormat::RGBA8) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::Unsupported,
            "EncodeImage", "WIC encoder supports only 8-bit RGBA/BGRA");
    }
    if (image.AlphaMode() == ImageAlphaMode::Premultiplied) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::Unsupported,
            "EncodeImage", "premultiplied input requires explicit unpremultiplication");
    }
    if (layout.Value() > std::numeric_limits<UINT>::max()) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::TooLarge,
            "EncodeImage", "image is too large for WIC WritePixels");
    }

    ComApartmentScope apartment;
    if (!apartment.Usable()) {
        return Failure<ImageEncodedPayload>(
            ApplicationErrorCode::BackendUnavailable, "EncodeImage",
            "WIC requires a usable COM apartment", apartment.Result());
    }
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) return Failure<ImageEncodedPayload>(
        ApplicationErrorCode::BackendUnavailable, "EncodeImage",
        "WIC factory creation failed", hr);

    ComPtr<IStream> stream;
    hr = CreateStreamOnHGlobal(nullptr, TRUE, stream.GetAddressOf());
    if (FAILED(hr)) return Failure<ImageEncodedPayload>(
        ApplicationErrorCode::EncodeFailed, "EncodeImage",
        "WIC output stream creation failed", hr);

    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> properties;
    const GUID container = outputFormat == ImageEncodedFormat::Png
        ? GUID_ContainerFormatPng : GUID_ContainerFormatJpeg;
    hr = factory->CreateEncoder(container, nullptr,
        encoder.GetAddressOf());
    if (SUCCEEDED(hr)) hr = encoder->Initialize(
        stream.Get(), WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(
        frame.GetAddressOf(), properties.GetAddressOf());
    if (SUCCEEDED(hr)) hr = frame->Initialize(properties.Get());
    if (SUCCEEDED(hr)) hr = frame->SetSize(image.Width(), image.Height());
    WICPixelFormatGUID format = outputFormat == ImageEncodedFormat::Png
        ? GUID_WICPixelFormat32bppBGRA : GUID_WICPixelFormat24bppBGR;
    if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&format);
    if (FAILED(hr)) return Failure<ImageEncodedPayload>(
        ApplicationErrorCode::EncodeFailed, "EncodeImage",
        "WIC PNG frame initialization failed", hr);

    std::vector<std::byte> converted;
    const std::byte* source = image.Bytes().data();
    std::uint32_t writeStride = image.RowStride();
    std::size_t writeBytes = layout.Value();
    if (outputFormat == ImageEncodedFormat::Jpeg) {
        const auto stride64 = static_cast<std::uint64_t>(image.Width()) * 3u;
        if (stride64 > std::numeric_limits<std::uint32_t>::max() ||
            stride64 > std::numeric_limits<std::size_t>::max() /
                image.Height()) {
            return Failure<ImageEncodedPayload>(ApplicationErrorCode::TooLarge,
                "EncodeImage", "JPEG row layout is too large");
        }
        writeStride = static_cast<std::uint32_t>(stride64);
        writeBytes = static_cast<std::size_t>(stride64) * image.Height();
        converted.resize(writeBytes);
        const auto sourceBytes = image.Bytes();
        for (std::uint32_t y = 0; y < image.Height(); ++y) {
            const auto* sourceRow = sourceBytes.data() +
                static_cast<std::size_t>(y) * image.RowStride();
            auto* targetRow = converted.data() +
                static_cast<std::size_t>(y) * writeStride;
            for (std::uint32_t x = 0; x < image.Width(); ++x) {
                targetRow[x * 3 + 0] = image.PixelFormat() == ImagePixelFormat::RGBA8
                    ? sourceRow[x * 4 + 2] : sourceRow[x * 4 + 0];
                targetRow[x * 3 + 1] = sourceRow[x * 4 + 1];
                targetRow[x * 3 + 2] = image.PixelFormat() == ImagePixelFormat::RGBA8
                    ? sourceRow[x * 4 + 0] : sourceRow[x * 4 + 2];
            }
        }
        source = converted.data();
    } else if (image.PixelFormat() == ImagePixelFormat::RGBA8) {
        converted.resize(layout.Value());
        const auto sourceBytes = image.Bytes();
        for (std::uint32_t y = 0; y < image.Height(); ++y) {
            const auto* sourceRow = sourceBytes.data() +
                static_cast<std::size_t>(y) * image.RowStride();
            auto* targetRow = converted.data() +
                static_cast<std::size_t>(y) * image.RowStride();
            for (std::uint32_t x = 0; x < image.Width(); ++x) {
                targetRow[x * 4 + 0] = sourceRow[x * 4 + 2];
                targetRow[x * 4 + 1] = sourceRow[x * 4 + 1];
                targetRow[x * 4 + 2] = sourceRow[x * 4 + 0];
                targetRow[x * 4 + 3] = sourceRow[x * 4 + 3];
            }
        }
        source = converted.data();
    }
    if (writeBytes > std::numeric_limits<UINT>::max()) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::TooLarge,
            "EncodeImage", "encoded source is too large for WIC");
    }
    hr = frame->WritePixels(image.Height(), writeStride,
        static_cast<UINT>(writeBytes),
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(source)));
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();
    if (FAILED(hr)) return Failure<ImageEncodedPayload>(
        ApplicationErrorCode::EncodeFailed, "EncodeImage",
        "WIC PNG encode failed", hr);

    HGLOBAL memory = nullptr;
    hr = GetHGlobalFromStream(stream.Get(), &memory);
    if (FAILED(hr) || memory == nullptr) return Failure<ImageEncodedPayload>(
        ApplicationErrorCode::EncodeFailed, "EncodeImage",
        "WIC output memory is unavailable", FAILED(hr) ? hr : E_FAIL);
    STATSTG stat{};
    hr = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hr)) return Failure<ImageEncodedPayload>(
        ApplicationErrorCode::EncodeFailed, "EncodeImage",
        "WIC output stream size is unavailable", hr);
    const auto size64 = stat.cbSize.QuadPart;
    const SIZE_T allocationSize = GlobalSize(memory);
    if (size64 == 0 || size64 >
        options.limits.maxEncodedBytes) {
        return Failure<ImageEncodedPayload>(
            ApplicationErrorCode::TooLarge, "EncodeImage",
            "encoded PNG exceeds the configured limit");
    }
    if (size64 > std::numeric_limits<SIZE_T>::max() ||
        allocationSize < static_cast<SIZE_T>(size64)) {
        return Failure<ImageEncodedPayload>(
            ApplicationErrorCode::TooLarge, "EncodeImage",
            "encoded PNG does not fit the output allocation");
    }
    const SIZE_T size = static_cast<SIZE_T>(size64);
    if (size64 >
        std::numeric_limits<std::size_t>::max()) {
        return Failure<ImageEncodedPayload>(
            ApplicationErrorCode::TooLarge, "EncodeImage",
            "encoded PNG does not fit this process");
    }
    void* locked = GlobalLock(memory);
    if (locked == nullptr) return Failure<ImageEncodedPayload>(
        ApplicationErrorCode::EncodeFailed, "EncodeImage",
        "WIC output memory could not be locked");
    ImageEncodedPayload result;
    result.format = outputFormat;
    result.bytes.assign(reinterpret_cast<const std::byte*>(locked),
        reinterpret_cast<const std::byte*>(locked) + size);
    GlobalUnlock(memory);
    return ApplicationResult<ImageEncodedPayload>::Success(std::move(result));
}

} // namespace

ApplicationResult<ImageBuffer> WicImageCodec::Decode(
    const ImageEncodedPayload& payload, const ImageDecodeOptions& options) {
    if (payload.format != ImageEncodedFormat::Unknown &&
        payload.format != ImageEncodedFormat::Png &&
        payload.format != ImageEncodedFormat::Jpeg &&
        payload.format != ImageEncodedFormat::Bmp &&
        payload.format != ImageEncodedFormat::Gif) {
        return Failure<ImageBuffer>(ApplicationErrorCode::Unsupported,
            "DecodeImage", "WIC backend does not support this encoded format");
    }
    return DecodeEncoded(payload, options);
}

ApplicationResult<ImageBuffer> WicImageCodec::DecodeThumbnail(
    const ImageEncodedPayload& payload,
    const ImageResizeOptions& resize,
    const ImageDecodeOptions& options) {
    if (payload.format != ImageEncodedFormat::Unknown &&
        payload.format != ImageEncodedFormat::Png &&
        payload.format != ImageEncodedFormat::Jpeg &&
        payload.format != ImageEncodedFormat::Bmp &&
        payload.format != ImageEncodedFormat::Gif) {
        return Failure<ImageBuffer>(ApplicationErrorCode::Unsupported,
            "DecodeThumbnail", "WIC backend does not support this encoded format");
    }
    return DecodeThumbnailEncoded(payload, resize, options);
}

ApplicationResult<ImageMetadata> WicImageCodec::Probe(
    const ImageEncodedPayload& payload, const ImageProbeOptions& options) {
    if (payload.format != ImageEncodedFormat::Unknown &&
        payload.format != ImageEncodedFormat::Png &&
        payload.format != ImageEncodedFormat::Jpeg &&
        payload.format != ImageEncodedFormat::Bmp &&
        payload.format != ImageEncodedFormat::Gif) {
        return Failure<ImageMetadata>(ApplicationErrorCode::Unsupported,
            "ProbeImage", "WIC backend does not support this encoded format");
    }
    return ProbeEncoded(payload, options);
}

ApplicationResult<ImageEncodedPayload> WicImageCodec::Encode(
    const ImageBuffer& image, ImageEncodedFormat format,
    const ImageEncodeOptions& options) {
    if (format != ImageEncodedFormat::Png &&
        format != ImageEncodedFormat::Jpeg) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::Unsupported,
            "EncodeImage", "WIC backend does not support this encoded format");
    }
    return EncodeEncoded(image, format, options);
}

bool WicImageCodec::Supports(ImageEncodedFormat format) const noexcept {
    return format == ImageEncodedFormat::Png ||
        format == ImageEncodedFormat::Jpeg ||
        format == ImageEncodedFormat::Bmp ||
        format == ImageEncodedFormat::Gif;
}

} // namespace Engine

#else

namespace Engine {

ApplicationResult<ImageBuffer> WicImageCodec::Decode(
    const ImageEncodedPayload&, const ImageDecodeOptions&) {
    return ApplicationResult<ImageBuffer>::Failure({
        ApplicationErrorCode::BackendUnavailable, "DecodeImage", "image_codec",
        "WIC is unavailable on this platform", std::nullopt, "wic"});
}

ApplicationResult<ImageBuffer> WicImageCodec::DecodeThumbnail(
    const ImageEncodedPayload&, const ImageResizeOptions&,
    const ImageDecodeOptions&) {
    return ApplicationResult<ImageBuffer>::Failure({
        ApplicationErrorCode::BackendUnavailable, "DecodeThumbnail",
        "image_codec", "WIC is unavailable on this platform", std::nullopt,
        "wic"});
}

ApplicationResult<ImageMetadata> WicImageCodec::Probe(
    const ImageEncodedPayload&, const ImageProbeOptions&) {
    return ApplicationResult<ImageMetadata>::Failure({
        ApplicationErrorCode::BackendUnavailable, "ProbeImage", "image_codec",
        "WIC is unavailable on this platform", std::nullopt, "wic"});
}

ApplicationResult<ImageEncodedPayload> WicImageCodec::Encode(
    const ImageBuffer&, ImageEncodedFormat, const ImageEncodeOptions&) {
    return ApplicationResult<ImageEncodedPayload>::Failure({
        ApplicationErrorCode::BackendUnavailable, "EncodeImage", "image_codec",
        "WIC is unavailable on this platform", std::nullopt, "wic"});
}

bool WicImageCodec::Supports(ImageEncodedFormat) const noexcept { return false; }

} // namespace Engine

#endif
