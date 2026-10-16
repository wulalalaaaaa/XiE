#pragma once

#include "Image/ImageCodec.h"
#include "Image/ImageScale.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>

#include <cstdint>

namespace Engine {

struct ImageFileReadOptions {
    ImageLimits limits{};
    bool requireRegularFile = true;
};

struct ImageDecodedFile {
    ImageBuffer image;
    ImageMetadata metadata;
};

// Optional diagnostics for image-file work.  Callbacks run on the calling
// thread (the ImageAsync worker for async requests) and receive no borrowed
// pixel or platform resource.  Zero-valued stages mean that the stage was not
// used, for example when a codec performs native thumbnail scaling.
struct ImageFileTiming {
    std::filesystem::path sourcePath;
    std::uint64_t readUs = 0;
    std::uint64_t formatDetectUs = 0;
    std::uint64_t codecProbeUs = 0;
    std::uint64_t codecDecodeUs = 0;
    std::uint64_t scaleUs = 0;
    std::uint64_t totalUs = 0;
    bool nativeThumbnail = false;
    bool success = false;
};

using ImageFileTimingCallback = std::function<void(const ImageFileTiming&)>;

class IImageFileService {
public:
    virtual ~IImageFileService() = default;

    virtual ApplicationResult<ImageEncodedPayload> ReadEncoded(
        const std::filesystem::path& path,
        const ImageFileReadOptions& options = {}) const = 0;
    virtual ApplicationResult<ImageMetadata> Inspect(
        const std::filesystem::path& path,
        const ImageFileReadOptions& options = {}) const = 0;
    virtual ApplicationResult<ImageDecodedFile> Decode(
        const std::filesystem::path& path,
        const ImageDecodeOptions& options = {}) const = 0;
    virtual ApplicationResult<ImageDecodedFile> DecodeThumbnail(
        const std::filesystem::path& path,
        const ImageResizeOptions& resize,
        const ImageDecodeOptions& decode = {}) const = 0;
    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;
};

// File I/O and codec composition live here.  The codec remains injectable and
// owns no path, stream, COM or native handle after a call returns.
class ImageFileService final : public IImageFileService {
public:
    explicit ImageFileService(std::shared_ptr<IImageCodec> codec = {});

    ApplicationResult<ImageEncodedPayload> ReadEncoded(
        const std::filesystem::path& path,
        const ImageFileReadOptions& options = {}) const override;
    ApplicationResult<ImageMetadata> Inspect(
        const std::filesystem::path& path,
        const ImageFileReadOptions& options = {}) const override;
    ApplicationResult<ImageDecodedFile> Decode(
        const std::filesystem::path& path,
        const ImageDecodeOptions& options = {}) const override;
    ApplicationResult<ImageDecodedFile> DecodeThumbnail(
        const std::filesystem::path& path,
        const ImageResizeOptions& resize,
        const ImageDecodeOptions& decode = {}) const override;
    void SetTimingCallback(ImageFileTimingCallback callback);
    [[nodiscard]] const char* BackendName() const noexcept override {
        return "xie_image_file";
    }

private:
    ApplicationResult<ImageEncodedPayload> ReadEncodedInternal(
        const std::filesystem::path& path,
        const ImageFileReadOptions& options,
        ImageFileTiming* timing) const;
    void EmitTiming(const ImageFileTiming& timing) const;

    std::shared_ptr<IImageCodec> m_Codec;
    mutable std::mutex m_TimingMutex;
    ImageFileTimingCallback m_TimingCallback;
};

} // namespace Engine
