#include "Image/ImageFile.h"

#include <chrono>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace Engine {
namespace {

ApplicationError Error(ApplicationErrorCode code, std::string operation,
    std::string message, std::string backend = "xie_image_file") {
    return {code, std::move(operation), "image_file", std::move(message),
        std::nullopt, std::move(backend)};
}

template <typename T>
ApplicationResult<T> Failure(ApplicationErrorCode code,
    std::string operation, std::string message,
    std::string backend = "xie_image_file") {
    return ApplicationResult<T>::Failure(Error(code, std::move(operation),
        std::move(message), std::move(backend)));
}

ApplicationResult<ImageMetadata> MetadataFromImage(
    const ImageEncodedPayload& payload, const ImageBuffer& image) {
    ImageMetadata metadata;
    metadata.encodedFormat = payload.format;
    metadata.width = image.Width();
    metadata.height = image.Height();
    metadata.frameCount = 1;
    metadata.alphaMode = image.AlphaMode();
    metadata.colorSpace = image.ColorSpace();
    metadata.orientation = image.Orientation();
    metadata.encodedBytes = payload.bytes.size();
    return ApplicationResult<ImageMetadata>::Success(std::move(metadata));
}

} // namespace

ImageFileService::ImageFileService(std::shared_ptr<IImageCodec> codec)
    : m_Codec(std::move(codec)) {}

ApplicationResult<ImageEncodedPayload> ImageFileService::ReadEncodedInternal(
    const std::filesystem::path& path,
    const ImageFileReadOptions& options,
    ImageFileTiming* timing) const {
    const auto readStart = std::chrono::steady_clock::now();
    if (path.empty()) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::InvalidOptions,
            "ReadImageFile", "image file path is empty");
    }
    if (options.limits.maxEncodedBytes == 0) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::InvalidOptions,
            "ReadImageFile", "maximum encoded image size must be non-zero");
    }
    std::error_code statusError;
    const auto status = std::filesystem::status(path, statusError);
    if (statusError) {
        const auto code = statusError == std::errc::permission_denied
            ? ApplicationErrorCode::PermissionDenied
            : ApplicationErrorCode::FileNotFound;
        return Failure<ImageEncodedPayload>(code, "ReadImageFile",
            statusError.message());
    }
    if (options.requireRegularFile && !std::filesystem::is_regular_file(status)) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::FileNotFound,
            "ReadImageFile", "image path is not a regular file");
    }
    std::error_code sizeError;
    const auto fileSize = std::filesystem::file_size(path, sizeError);
    if (sizeError) {
        const auto code = sizeError == std::errc::permission_denied
            ? ApplicationErrorCode::PermissionDenied
            : ApplicationErrorCode::ReadFailed;
        return Failure<ImageEncodedPayload>(code, "ReadImageFile",
            sizeError.message());
    }
    if (fileSize == 0) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::NoData,
            "ReadImageFile", "image file is empty");
    }
    if (fileSize > options.limits.maxEncodedBytes ||
        fileSize > std::numeric_limits<std::size_t>::max()) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::TooLarge,
            "ReadImageFile", "encoded image file exceeds the configured limit");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::PermissionDenied,
            "ReadImageFile", "image file could not be opened");
    }
    ImageEncodedPayload payload;
    payload.bytes.resize(static_cast<std::size_t>(fileSize));
    input.read(reinterpret_cast<char*>(payload.bytes.data()),
        static_cast<std::streamsize>(payload.bytes.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(payload.bytes.size())) {
        return Failure<ImageEncodedPayload>(ApplicationErrorCode::ReadFailed,
            "ReadImageFile", "image file read was incomplete");
    }
    if (timing) timing->readUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - readStart).count());
    const auto detectStart = std::chrono::steady_clock::now();
    payload.format = DetectEncodedFormat(payload.bytes);
    if (timing) timing->formatDetectUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - detectStart).count());
    return ApplicationResult<ImageEncodedPayload>::Success(std::move(payload));
}

ApplicationResult<ImageEncodedPayload> ImageFileService::ReadEncoded(
    const std::filesystem::path& path,
    const ImageFileReadOptions& options) const {
    return ReadEncodedInternal(path, options, nullptr);
}

void ImageFileService::SetTimingCallback(ImageFileTimingCallback callback) {
    std::lock_guard lock(m_TimingMutex);
    m_TimingCallback = std::move(callback);
}

void ImageFileService::EmitTiming(const ImageFileTiming& timing) const {
    ImageFileTimingCallback callback;
    {
        std::lock_guard lock(m_TimingMutex);
        callback = m_TimingCallback;
    }
    if (callback) callback(timing);
}

ApplicationResult<ImageMetadata> ImageFileService::Inspect(
    const std::filesystem::path& path,
    const ImageFileReadOptions& options) const {
    if (!m_Codec) return Failure<ImageMetadata>(
        ApplicationErrorCode::BackendUnavailable, "InspectImageFile",
        "image codec backend is unavailable");
    const auto payload = ReadEncoded(path, options);
    if (!payload) return ApplicationResult<ImageMetadata>::Failure(
        payload.Error());
    ImageProbeOptions probe;
    probe.limits = options.limits;
    auto metadata = m_Codec->Probe(payload.Value(), probe);
    if (!metadata) return metadata;
    metadata.Value().encodedBytes = payload.Value().bytes.size();
    return metadata;
}

ApplicationResult<ImageDecodedFile> ImageFileService::Decode(
    const std::filesystem::path& path,
    const ImageDecodeOptions& options) const {
    if (!m_Codec) return Failure<ImageDecodedFile>(
        ApplicationErrorCode::BackendUnavailable, "DecodeImageFile",
        "image codec backend is unavailable");
    ImageFileReadOptions read;
    read.limits = options.limits;
    const auto payload = ReadEncoded(path, read);
    if (!payload) return ApplicationResult<ImageDecodedFile>::Failure(
        payload.Error());
    auto decoded = m_Codec->Decode(payload.Value(), options);
    if (!decoded) return ApplicationResult<ImageDecodedFile>::Failure(
        decoded.Error());

    ImageMetadata metadata;
    ImageProbeOptions probe;
    probe.limits = options.limits;
    const auto probed = m_Codec->Probe(payload.Value(), probe);
    if (probed) metadata = probed.Value();
    else {
        const auto fallback = MetadataFromImage(payload.Value(), decoded.Value());
        metadata = fallback.Value();
    }
    metadata.encodedBytes = payload.Value().bytes.size();
    return ApplicationResult<ImageDecodedFile>::Success({
        std::move(decoded.Value()), std::move(metadata)});
}

ApplicationResult<ImageDecodedFile> ImageFileService::DecodeThumbnail(
    const std::filesystem::path& path,
    const ImageResizeOptions& resize,
    const ImageDecodeOptions& decode) const {
    ImageFileTiming timing;
    timing.sourcePath = path;
    const auto totalStart = std::chrono::steady_clock::now();
    const auto finish = [&](bool success) {
        timing.success = success;
        timing.totalUs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - totalStart).count());
        EmitTiming(timing);
    };
    if (!m_Codec) {
        finish(false);
        return Failure<ImageDecodedFile>(
            ApplicationErrorCode::BackendUnavailable, "DecodeImageThumbnail",
            "image codec backend is unavailable");
    }
    ImageFileReadOptions read;
    read.limits = decode.limits;
    const auto payload = ReadEncodedInternal(path, read, &timing);
    if (!payload) {
        finish(false);
        return ApplicationResult<ImageDecodedFile>::Failure(payload.Error());
    }

    const auto nativeStart = std::chrono::steady_clock::now();
    const auto native = m_Codec->DecodeThumbnail(
        payload.Value(), resize, decode);
    const auto nativeElapsed = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - nativeStart).count());
    if (native) {
        timing.nativeThumbnail = true;
        timing.codecDecodeUs = nativeElapsed;
        const auto metadata = MetadataFromImage(payload.Value(), native.Value());
        finish(true);
        return ApplicationResult<ImageDecodedFile>::Success({
            std::move(native.Value()), metadata.Value()});
    }
    if (native.Error().code != ApplicationErrorCode::Unsupported) {
        finish(false);
        return ApplicationResult<ImageDecodedFile>::Failure(native.Error());
    }

    const auto decodeStart = std::chrono::steady_clock::now();
    auto source = m_Codec->Decode(payload.Value(), decode);
    timing.codecDecodeUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - decodeStart).count());
    if (!source) {
        finish(false);
        return ApplicationResult<ImageDecodedFile>::Failure(source.Error());
    }
    auto sourceImage = std::move(source.Value());

    ImageMetadata metadata;
    ImageProbeOptions probe;
    probe.limits = decode.limits;
    const auto probeStart = std::chrono::steady_clock::now();
    const auto probed = m_Codec->Probe(payload.Value(), probe);
    timing.codecProbeUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - probeStart).count());
    if (probed) metadata = probed.Value();
    else metadata = MetadataFromImage(payload.Value(), sourceImage).Value();
    metadata.encodedBytes = payload.Value().bytes.size();

    const auto scaleStart = std::chrono::steady_clock::now();
    auto scaled = ResizeImage(sourceImage, resize);
    timing.scaleUs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - scaleStart).count());
    if (!scaled) {
        finish(false);
        return ApplicationResult<ImageDecodedFile>::Failure(scaled.Error());
    }
    auto scaledImage = std::move(scaled.Value());
    metadata.width = scaledImage.Width();
    metadata.height = scaledImage.Height();
    metadata.alphaMode = scaledImage.AlphaMode();
    metadata.colorSpace = scaledImage.ColorSpace();
    metadata.orientation = scaledImage.Orientation();
    finish(true);
    return ApplicationResult<ImageDecodedFile>::Success({
        std::move(scaledImage), std::move(metadata)});
}

} // namespace Engine
