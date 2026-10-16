#pragma once

#include "Application/ApplicationResult.h"
#include "Image/ImageBuffer.h"
#include "Renderer2D/TextureRegistry2D.h"

#include <cstdint>
#include <thread>
#include <vector>

namespace Engine {

struct ImageTextureBridgeOptions {
    ImageLimits limits{};
};

// The bridge is the platform-neutral boundary between owned CPU image data
// and renderer-owned compact RGBA8 textures.  It intentionally does not own
// either the ImageBuffer or the TextureRegistry2D.
class ImageTextureBridge final {
public:
    explicit ImageTextureBridge(
        TextureRegistry2D& textures,
        ImageTextureBridgeOptions options = {},
        std::thread::id ownerThread = std::this_thread::get_id());

    ImageTextureBridge(const ImageTextureBridge&) = delete;
    ImageTextureBridge& operator=(const ImageTextureBridge&) = delete;

    ApplicationResult<TextureHandle> CreateTexture(const ImageView& image);
    ApplicationResult<TextureHandle> CreateTexture(const ImageBuffer& image) {
        return CreateTexture(image.View());
    }

    ApplicationResult<void> UpdateTexture(
        TextureHandle handle, const ImageView& image);
    ApplicationResult<void> UpdateTexture(
        TextureHandle handle, const ImageBuffer& image) {
        return UpdateTexture(handle, image.View());
    }

    // Destroy is idempotent for an invalid or already-stale handle.  A
    // missing-texture handle is rejected because it is registry-owned.
    ApplicationResult<void> DestroyTexture(TextureHandle handle);
    ApplicationResult<void> Shutdown();

    [[nodiscard]] bool IsShuttingDown() const noexcept {
        return m_ShuttingDown;
    }
    [[nodiscard]] std::thread::id OwnerThread() const noexcept {
        return m_OwnerThread;
    }

private:
    struct ConvertedPixels {
        int width = 0;
        int height = 0;
        AlphaMode alphaMode = AlphaMode::Straight;
        std::vector<std::uint8_t> rgba;
    };

    ApplicationResult<ConvertedPixels> Convert(const ImageView& image) const;
    ApplicationResult<void> CheckThread(const char* operation) const;
    ApplicationResult<void> CheckActive(const char* operation) const;
    ApplicationResult<void> CheckHandle(
        TextureHandle handle, const char* operation) const;

    TextureRegistry2D* m_Textures = nullptr;
    ImageTextureBridgeOptions m_Options{};
    std::thread::id m_OwnerThread;
    bool m_ShuttingDown = false;
};

} // namespace Engine
