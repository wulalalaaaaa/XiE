#include "Image/ImageFile.h"
#include "Image/Renderer2D/ImageTextureBridge.h"
#include "Image/Windows/WicImageCodec.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
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

Engine::ImageBuffer Fixture() {
    Engine::ImageDescriptor descriptor;
    descriptor.width = 2;
    descriptor.height = 2;
    descriptor.rowStride = 8;
    descriptor.pixelFormat = Engine::ImagePixelFormat::BGRA8;
    descriptor.alphaMode = Engine::ImageAlphaMode::Straight;
    descriptor.colorSpace = Engine::ImageColorSpace::SRGB;
    const auto allocated = Engine::ImageBuffer::Allocate(descriptor);
    if (!allocated) return {};
    auto image = std::move(allocated.Value());
    const std::vector<std::byte> pixels{
        std::byte{10}, std::byte{20}, std::byte{30}, std::byte{96},
        std::byte{40}, std::byte{50}, std::byte{60}, std::byte{255},
        std::byte{70}, std::byte{80}, std::byte{90}, std::byte{255},
        std::byte{100}, std::byte{110}, std::byte{120}, std::byte{255}};
    std::copy(pixels.begin(), pixels.end(), image.MutableBytes().begin());
    return image;
}

bool Write(const std::filesystem::path& path,
    std::span<const std::byte> bytes) {
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}
}

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
        "xie_image_texture_bridge_wic";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root, ignored);

    auto codec = std::make_shared<Engine::WicImageCodec>();
    const auto source = Fixture();
    const auto encoded = codec->Encode(source, Engine::ImageEncodedFormat::Png);
    Check(static_cast<bool>(encoded), "WIC PNG encoding");
    const auto path = root / "fixture.png";
    if (encoded) Check(Write(path, encoded.Value().bytes),
        "test-owned WIC PNG file");

    auto files = std::make_shared<Engine::ImageFileService>(codec);
    const auto decoded = files->Decode(path);
    Check(static_cast<bool>(decoded), "real WIC file decode");
    Check(decoded && decoded.Value().image.PixelFormat() ==
        Engine::ImagePixelFormat::BGRA8, "WIC canonical BGRA8 output");
    Check(decoded && decoded.Value().image.Orientation() ==
        Engine::ImageOrientation::TopDown, "WIC top-down output");

    Engine::TextureRegistry2D textures;
    Engine::ImageTextureBridge bridge(textures);
    const auto texture = decoded
        ? bridge.CreateTexture(decoded.Value().image)
        : Engine::ApplicationResult<Engine::TextureHandle>::Failure({
            Engine::ApplicationErrorCode::NoData, "test", "test",
            "decode unavailable"});
    Check(static_cast<bool>(texture), "WIC ImageBuffer to renderer bridge");
    if (texture) {
        const auto* pixels = textures.Get(texture.Value());
        Check(pixels && pixels->info.width == 2 && pixels->info.height == 2,
            "bridged texture dimensions");
        Check(pixels && pixels->rgba.size() == 16,
            "bridged texture compact RGBA8 payload");
        Check(pixels && pixels->rgba[0] == 30 && pixels->rgba[1] == 20 &&
            pixels->rgba[2] == 10 && pixels->rgba[3] == 96,
            "bridged WIC channel and alpha semantics");
    }

    (void)bridge.Shutdown();
    fs::remove_all(root, ignored);
    std::cout << "ImageTextureBridge WIC checks=" << checks
              << " failures=" << failures << '\n';
    return failures == 0 ? 0 : 1;
}
