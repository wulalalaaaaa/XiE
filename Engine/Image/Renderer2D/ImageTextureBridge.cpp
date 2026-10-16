#include "Image/Renderer2D/ImageTextureBridge.h"

#include <cstddef>
#include <exception>
#include <limits>
#include <string>
#include <utility>

namespace Engine {
namespace {

ApplicationError Error(
    ApplicationErrorCode code,
    std::string operation,
    std::string message) {
    return {code, std::move(operation), "image_renderer2d", std::move(message),
        std::nullopt, "xie_image_renderer2d"};
}

template <typename T>
ApplicationResult<T> Failure(
    ApplicationErrorCode code,
    std::string operation,
    std::string message) {
    return ApplicationResult<T>::Failure(
        Error(code, std::move(operation), std::move(message)));
}

AlphaMode ToRendererAlpha(ImageAlphaMode mode) {
    return mode == ImageAlphaMode::Premultiplied
        ? AlphaMode::Premultiplied : AlphaMode::Straight;
}

} // namespace

ImageTextureBridge::ImageTextureBridge(
    TextureRegistry2D& textures,
    ImageTextureBridgeOptions options,
    std::thread::id ownerThread)
    : m_Textures(&textures),
      m_Options(std::move(options)),
      m_OwnerThread(ownerThread) {}

ApplicationResult<void> ImageTextureBridge::CheckThread(
    const char* operation) const {
    if (std::this_thread::get_id() != m_OwnerThread) {
        return ApplicationResult<void>::Failure(Error(
            ApplicationErrorCode::WrongThread, operation,
            "texture upload must run on the renderer owner thread"));
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ImageTextureBridge::CheckActive(
    const char* operation) const {
    if (!m_Textures) {
        return ApplicationResult<void>::Failure(Error(
            ApplicationErrorCode::BackendUnavailable, operation,
            "texture registry is unavailable"));
    }
    if (m_ShuttingDown) {
        return ApplicationResult<void>::Failure(Error(
            ApplicationErrorCode::ShuttingDown, operation,
            "image texture bridge is shutting down"));
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ImageTextureBridge::CheckHandle(
    TextureHandle handle, const char* operation) const {
    if (!handle.IsValid() || handle == m_Textures->MissingTexture() ||
        m_Textures->GetInfo(handle).state == TextureState::Missing) {
        return ApplicationResult<void>::Failure(Error(
            ApplicationErrorCode::InvalidData, operation,
            "texture handle is invalid or belongs to another registry"));
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<ImageTextureBridge::ConvertedPixels>
ImageTextureBridge::Convert(const ImageView& image) const {
    const auto layout = ValidateImageLayout(
        image.descriptor, m_Options.limits);
    if (!layout) {
        return ApplicationResult<ConvertedPixels>::Failure(layout.Error());
    }
    if (image.bytes.size() < layout.Value()) {
        return Failure<ConvertedPixels>(
            ApplicationErrorCode::InvalidData, "ConvertImageTexture",
            "image payload is shorter than its declared layout");
    }

    const auto width = static_cast<std::uint64_t>(image.descriptor.width);
    const auto height = static_cast<std::uint64_t>(image.descriptor.height);
    if (width > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
        width > std::numeric_limits<std::uint64_t>::max() / height ||
        width * height > std::numeric_limits<std::uint64_t>::max() / 4u ||
        width * height * 4u > std::numeric_limits<std::size_t>::max()) {
        return Failure<ConvertedPixels>(
            ApplicationErrorCode::TooLarge, "ConvertImageTexture",
            "image dimensions do not fit a renderer texture");
    }

    const auto compactBytes = static_cast<std::size_t>(width * height * 4u);
    try {
        ConvertedPixels result;
        result.width = static_cast<int>(width);
        result.height = static_cast<int>(height);
        result.alphaMode = ToRendererAlpha(image.descriptor.alphaMode);
        result.rgba.resize(compactBytes);

        const auto* source = image.bytes.data();
        for (std::uint32_t y = 0; y < image.descriptor.height; ++y) {
            for (std::uint32_t x = 0; x < image.descriptor.width; ++x) {
                const auto sourceOffset =
                    static_cast<std::size_t>(y) * image.descriptor.rowStride +
                    static_cast<std::size_t>(x) * 4u;
                const auto destinationOffset =
                    (static_cast<std::size_t>(y) * image.descriptor.width + x) *
                    4u;
                const auto c0 = std::to_integer<std::uint8_t>(
                    source[sourceOffset + 0]);
                const auto c1 = std::to_integer<std::uint8_t>(
                    source[sourceOffset + 1]);
                const auto c2 = std::to_integer<std::uint8_t>(
                    source[sourceOffset + 2]);
                const auto alpha = std::to_integer<std::uint8_t>(
                    source[sourceOffset + 3]);
                if (image.descriptor.pixelFormat == ImagePixelFormat::BGRA8) {
                    result.rgba[destinationOffset + 0] = c2;
                    result.rgba[destinationOffset + 1] = c1;
                    result.rgba[destinationOffset + 2] = c0;
                } else {
                    result.rgba[destinationOffset + 0] = c0;
                    result.rgba[destinationOffset + 1] = c1;
                    result.rgba[destinationOffset + 2] = c2;
                }
                result.rgba[destinationOffset + 3] =
                    image.descriptor.alphaMode == ImageAlphaMode::Opaque
                    ? 255 : alpha;
            }
        }
        return ApplicationResult<ConvertedPixels>::Success(std::move(result));
    } catch (const std::exception& exception) {
        return ApplicationResult<ConvertedPixels>::Failure(Error(
            ApplicationErrorCode::TooLarge, "ConvertImageTexture",
            exception.what()));
    }
}

ApplicationResult<TextureHandle> ImageTextureBridge::CreateTexture(
    const ImageView& image) {
    const auto thread = CheckThread("CreateImageTexture");
    if (!thread) return ApplicationResult<TextureHandle>::Failure(thread.Error());
    const auto active = CheckActive("CreateImageTexture");
    if (!active) return ApplicationResult<TextureHandle>::Failure(active.Error());
    const auto converted = Convert(image);
    if (!converted) {
        return ApplicationResult<TextureHandle>::Failure(converted.Error());
    }
    try {
        const auto handle = m_Textures->CreateTextureRGBA8(
            converted.Value().width, converted.Value().height,
            converted.Value().rgba, converted.Value().alphaMode);
        if (!handle.IsValid() || handle == m_Textures->MissingTexture()) {
            return Failure<TextureHandle>(
                ApplicationErrorCode::BackendFailure, "CreateImageTexture",
                "texture registry rejected the image");
        }
        return ApplicationResult<TextureHandle>::Success(handle);
    } catch (const std::exception& exception) {
        return ApplicationResult<TextureHandle>::Failure(Error(
            ApplicationErrorCode::BackendFailure, "CreateImageTexture",
            exception.what()));
    }
}

ApplicationResult<void> ImageTextureBridge::UpdateTexture(
    TextureHandle handle, const ImageView& image) {
    const auto thread = CheckThread("UpdateImageTexture");
    if (!thread) return thread;
    const auto active = CheckActive("UpdateImageTexture");
    if (!active) return active;
    const auto validHandle = CheckHandle(handle, "UpdateImageTexture");
    if (!validHandle) return validHandle;
    const auto converted = Convert(image);
    if (!converted) return ApplicationResult<void>::Failure(converted.Error());
    try {
        if (!m_Textures->UpdateTextureRGBA8(
                handle, converted.Value().width, converted.Value().height,
                converted.Value().rgba, converted.Value().alphaMode)) {
            return ApplicationResult<void>::Failure(Error(
                ApplicationErrorCode::BackendFailure, "UpdateImageTexture",
                "texture registry rejected the update"));
        }
        return ApplicationResult<void>::Success();
    } catch (const std::exception& exception) {
        return ApplicationResult<void>::Failure(Error(
            ApplicationErrorCode::BackendFailure, "UpdateImageTexture",
            exception.what()));
    }
}

ApplicationResult<void> ImageTextureBridge::DestroyTexture(
    TextureHandle handle) {
    const auto thread = CheckThread("DestroyImageTexture");
    if (!thread) return thread;
    const auto active = CheckActive("DestroyImageTexture");
    if (!active) return active;
    if (!handle.IsValid()) return ApplicationResult<void>::Success();
    if (handle == m_Textures->MissingTexture()) {
        return ApplicationResult<void>::Failure(Error(
            ApplicationErrorCode::InvalidData, "DestroyImageTexture",
            "the registry missing texture cannot be destroyed"));
    }
    // TextureRegistry2D::Destroy is generation-safe and idempotent.  Calling
    // it for a stale handle is therefore a stable no-op by contract.
    m_Textures->Destroy(handle);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ImageTextureBridge::Shutdown() {
    const auto thread = CheckThread("ShutdownImageTextureBridge");
    if (!thread) return thread;
    if (m_ShuttingDown) return ApplicationResult<void>::Success();
    m_ShuttingDown = true;
    return ApplicationResult<void>::Success();
}

} // namespace Engine
