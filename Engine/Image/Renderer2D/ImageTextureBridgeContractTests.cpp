#include "Image/Renderer2D/ImageTextureBridge.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {
int failures = 0;
int checks = 0;

void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

Engine::ImageView View(
    Engine::ImageDescriptor descriptor,
    const std::vector<std::uint8_t>& bytes) {
    return {
        descriptor,
        std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(bytes.data()), bytes.size())};
}
}

int main() {
    Engine::TextureRegistry2D textures;
    Engine::ImageTextureBridge bridge(textures);

    Engine::ImageDescriptor rgba;
    rgba.width = 2;
    rgba.height = 1;
    rgba.rowStride = 8;
    rgba.pixelFormat = Engine::ImagePixelFormat::RGBA8;
    rgba.alphaMode = Engine::ImageAlphaMode::Straight;
    rgba.colorSpace = Engine::ImageColorSpace::SRGB;
    const std::vector<std::uint8_t> rgbaPixels{
        10, 20, 30, 40, 50, 60, 70, 80};
    const auto rgbaTexture = bridge.CreateTexture(View(rgba, rgbaPixels));
    Check(static_cast<bool>(rgbaTexture), "RGBA create");
    if (rgbaTexture) {
        const auto* pixels = textures.Get(rgbaTexture.Value());
        Check(pixels && pixels->rgba == rgbaPixels,
            "RGBA bytes stay in renderer order");
        Check(pixels && pixels->info.alphaMode == Engine::AlphaMode::Straight,
            "straight alpha contract");
    }

    Engine::ImageDescriptor bgra = rgba;
    bgra.pixelFormat = Engine::ImagePixelFormat::BGRA8;
    bgra.rowStride = 12;
    const std::vector<std::uint8_t> paddedBgra{
        1, 2, 3, 4, 5, 6, 7, 8, 0xaa, 0xbb, 0xcc, 0xdd};
    const auto bgraTexture = bridge.CreateTexture(View(bgra, paddedBgra));
    Check(static_cast<bool>(bgraTexture), "BGRA padded create");
    if (bgraTexture) {
        const auto* pixels = textures.Get(bgraTexture.Value());
        Check(pixels && pixels->rgba == std::vector<std::uint8_t>{
            3, 2, 1, 4, 7, 6, 5, 8},
            "BGRA channel conversion and padded stride");
    }

    Engine::ImageDescriptor opaque = rgba;
    opaque.alphaMode = Engine::ImageAlphaMode::Opaque;
    const auto opaqueTexture = bridge.CreateTexture(View(
        opaque, std::vector<std::uint8_t>{100, 90, 80, 0, 1, 2, 3, 0}));
    Check(static_cast<bool>(opaqueTexture), "opaque create");
    if (opaqueTexture) {
        const auto* pixels = textures.Get(opaqueTexture.Value());
        Check(pixels && pixels->rgba[3] == 255 && pixels->rgba[7] == 255,
            "opaque alpha is normalized");
        Check(pixels && pixels->info.alphaMode == Engine::AlphaMode::Straight,
            "opaque maps to straight renderer alpha");
    }

    Engine::ImageDescriptor premultiplied = rgba;
    premultiplied.alphaMode = Engine::ImageAlphaMode::Premultiplied;
    const std::vector<std::uint8_t> premulPixels{
        8, 4, 2, 64, 1, 2, 3, 128};
    const auto premulTexture = bridge.CreateTexture(
        View(premultiplied, premulPixels));
    Check(static_cast<bool>(premulTexture), "premultiplied create");
    if (premulTexture) {
        const auto* pixels = textures.Get(premulTexture.Value());
        Check(pixels && pixels->rgba == premulPixels,
            "premultiplied RGB is not multiplied twice");
        Check(pixels && pixels->info.alphaMode == Engine::AlphaMode::Premultiplied,
            "premultiplied alpha contract");
    }

    const std::vector<std::uint8_t> replacement{
        200, 100, 50, 255, 1, 2, 3, 255};
    Check(rgbaTexture && bridge.UpdateTexture(
        rgbaTexture.Value(), View(rgba, replacement)), "texture update");
    if (rgbaTexture) {
        const auto* pixels = textures.Get(rgbaTexture.Value());
        Check(pixels && pixels->rgba == replacement,
            "updated texture bytes");
    }

    Engine::ImageDescriptor zero = rgba;
    zero.width = 0;
    Check(!bridge.CreateTexture(View(zero, rgbaPixels)) &&
        bridge.CreateTexture(View(zero, rgbaPixels)).Error().code ==
            Engine::ApplicationErrorCode::InvalidData,
        "zero dimensions rejected");
    Engine::ImageDescriptor shortStride = rgba;
    shortStride.rowStride = 4;
    const auto shortStrideResult = bridge.CreateTexture(
        View(shortStride, rgbaPixels));
    Check(!shortStrideResult && shortStrideResult.Error().code ==
        Engine::ApplicationErrorCode::InvalidData,
        "short stride rejected");
    Engine::ImageDescriptor unsupported = rgba;
    unsupported.pixelFormat = static_cast<Engine::ImagePixelFormat>(99);
    const auto unsupportedResult = bridge.CreateTexture(
        View(unsupported, rgbaPixels));
    Check(!unsupportedResult && unsupportedResult.Error().code ==
        Engine::ApplicationErrorCode::Unsupported,
        "unsupported format rejected");

    Engine::ImageTextureBridgeOptions smallOptions;
    smallOptions.limits.maxDimension = 1;
    Engine::ImageTextureBridge limited(textures, smallOptions);
    const auto tooLarge = limited.CreateTexture(View(rgba, rgbaPixels));
    Check(!tooLarge && tooLarge.Error().code ==
        Engine::ApplicationErrorCode::TooLarge,
        "dimension limit rejected");

    Engine::ApplicationResult<Engine::TextureHandle> wrongThread;
    std::thread thread([&] {
        wrongThread = bridge.CreateTexture(View(rgba, rgbaPixels));
    });
    thread.join();
    Check(!wrongThread && wrongThread.Error().code ==
        Engine::ApplicationErrorCode::WrongThread,
        "wrong-thread upload rejected");

    if (rgbaTexture) {
        Check(static_cast<bool>(bridge.DestroyTexture(rgbaTexture.Value())),
            "destroy texture");
        Check(static_cast<bool>(bridge.DestroyTexture(rgbaTexture.Value())),
            "stale destroy is idempotent");
        Check(!bridge.UpdateTexture(rgbaTexture.Value(), View(rgba, replacement)) &&
            bridge.UpdateTexture(rgbaTexture.Value(), View(rgba, replacement))
                .Error().code == Engine::ApplicationErrorCode::InvalidData,
            "stale update rejected");
    }

    Check(static_cast<bool>(bridge.Shutdown()), "bridge shutdown");
    const auto afterShutdown = bridge.CreateTexture(View(rgba, rgbaPixels));
    Check(!afterShutdown && afterShutdown.Error().code ==
        Engine::ApplicationErrorCode::ShuttingDown,
        "shutdown rejects upload");

    std::cout << "ImageTextureBridge checks=" << checks
              << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
