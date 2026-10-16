#include "Image/ImageServices.h"

#include <utility>

namespace Engine {

ImageServices::ImageServices(
    std::shared_ptr<IImageCodec> codec,
    std::shared_ptr<IImageClipboardSource> clipboard,
    std::shared_ptr<IImageFileService> files,
    std::shared_ptr<ImageAsyncService> async)
    : m_Codec(std::move(codec)), m_Clipboard(std::move(clipboard)),
      m_Files(std::move(files)), m_Async(std::move(async)) {}

ImageServices::~ImageServices() noexcept {
    (void)Shutdown();
}

std::shared_ptr<IImageCodec> ImageServices::Codec() const {
    std::lock_guard lock(m_Mutex);
    return m_ShuttingDown ? nullptr : m_Codec;
}

std::shared_ptr<IImageClipboardSource> ImageServices::Clipboard() const {
    std::lock_guard lock(m_Mutex);
    return m_ShuttingDown ? nullptr : m_Clipboard;
}

std::shared_ptr<IImageFileService> ImageServices::Files() const {
    std::lock_guard lock(m_Mutex);
    return m_ShuttingDown ? nullptr : m_Files;
}

std::shared_ptr<ImageAsyncService> ImageServices::Async() const {
    std::lock_guard lock(m_Mutex);
    return m_ShuttingDown ? nullptr : m_Async;
}

bool ImageServices::HasCodec() const {
    std::lock_guard lock(m_Mutex);
    return !m_ShuttingDown && m_Codec != nullptr;
}

bool ImageServices::HasClipboard() const {
    std::lock_guard lock(m_Mutex);
    return !m_ShuttingDown && m_Clipboard != nullptr;
}

bool ImageServices::HasFiles() const {
    std::lock_guard lock(m_Mutex);
    return !m_ShuttingDown && m_Files != nullptr;
}

bool ImageServices::HasAsync() const {
    std::lock_guard lock(m_Mutex);
    return !m_ShuttingDown && m_Async != nullptr &&
        !m_Async->IsShuttingDown();
}

std::string ImageServices::CodecBackendName() const {
    const auto codec = Codec();
    if (!codec || codec->BackendName() == nullptr) return {};
    return codec->BackendName();
}

std::string ImageServices::ClipboardBackendName() const {
    const auto clipboard = Clipboard();
    if (!clipboard || clipboard->BackendName() == nullptr) return {};
    return clipboard->BackendName();
}

std::string ImageServices::FileBackendName() const {
    const auto files = Files();
    if (!files || files->BackendName() == nullptr) return {};
    return files->BackendName();
}

std::string ImageServices::AsyncBackendName() const {
    const auto async = Async();
    if (!async || async->BackendName() == nullptr) return {};
    return async->BackendName();
}

bool ImageServices::IsShuttingDown() const noexcept {
    std::lock_guard lock(m_Mutex);
    return m_ShuttingDown;
}

ApplicationResult<void> ImageServices::Shutdown() {
    std::shared_ptr<ImageAsyncService> async;
    {
        std::lock_guard lock(m_Mutex);
        if (m_ShuttingDown) return ApplicationResult<void>::Success();
        m_ShuttingDown = true;
        async = m_Async;
        m_Async.reset();
        m_Files.reset();
        m_Codec.reset();
        m_Clipboard.reset();
    }
    if (async) (void)async->Shutdown();
    return ApplicationResult<void>::Success();
}

} // namespace Engine
