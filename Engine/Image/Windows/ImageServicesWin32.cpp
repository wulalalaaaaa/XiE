#include "Image/Windows/ImageServicesWin32.h"

#include "Image/Windows/Win32ImageClipboardSource.h"
#include "Image/Windows/WicImageCodec.h"

namespace Engine {

std::shared_ptr<ImageServices> CreateWin32ImageServices() {
    auto codec = std::make_shared<WicImageCodec>();
    auto clipboard = std::make_shared<Win32ImageClipboardSource>(codec);
    auto files = std::make_shared<ImageFileService>(codec);
    auto async = std::make_shared<ImageAsyncService>(files);
    return std::make_shared<ImageServices>(std::move(codec),
        std::move(clipboard), std::move(files), std::move(async));
}

} // namespace Engine
