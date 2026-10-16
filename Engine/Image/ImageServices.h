#pragma once

#include "Image/ImageClipboard.h"
#include "Image/ImageAsync.h"
#include "Image/ImageCodec.h"

#include <memory>
#include <mutex>
#include <string>

namespace Engine {

// Small service registry owned by ApplicationContext.  It is a discovery and
// lifetime boundary, not an image-processing God Object; codec and clipboard
// implementations remain separate objects and can be injected independently.
class ImageServices final {
public:
    ImageServices(
        std::shared_ptr<IImageCodec> codec = {},
        std::shared_ptr<IImageClipboardSource> clipboard = {},
        std::shared_ptr<IImageFileService> files = {},
        std::shared_ptr<ImageAsyncService> async = {});
    ~ImageServices() noexcept;

    ImageServices(const ImageServices&) = delete;
    ImageServices& operator=(const ImageServices&) = delete;

    [[nodiscard]] std::shared_ptr<IImageCodec> Codec() const;
    [[nodiscard]] std::shared_ptr<IImageClipboardSource> Clipboard() const;
    [[nodiscard]] std::shared_ptr<IImageFileService> Files() const;
    [[nodiscard]] std::shared_ptr<ImageAsyncService> Async() const;
    [[nodiscard]] bool HasCodec() const;
    [[nodiscard]] bool HasClipboard() const;
    [[nodiscard]] bool HasFiles() const;
    [[nodiscard]] bool HasAsync() const;
    [[nodiscard]] std::string CodecBackendName() const;
    [[nodiscard]] std::string ClipboardBackendName() const;
    [[nodiscard]] std::string FileBackendName() const;
    [[nodiscard]] std::string AsyncBackendName() const;
    [[nodiscard]] bool IsShuttingDown() const noexcept;

    ApplicationResult<void> Shutdown();

private:
    mutable std::mutex m_Mutex;
    std::shared_ptr<IImageCodec> m_Codec;
    std::shared_ptr<IImageClipboardSource> m_Clipboard;
    std::shared_ptr<IImageFileService> m_Files;
    std::shared_ptr<ImageAsyncService> m_Async;
    bool m_ShuttingDown = false;
};

} // namespace Engine
