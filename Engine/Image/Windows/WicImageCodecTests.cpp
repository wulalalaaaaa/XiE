#include "Image/Windows/WicImageCodec.h"

#include <algorithm>
#include <iostream>
#include <utility>
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

Engine::ImageBuffer MakeImage(bool transparent) {
    Engine::ImageDescriptor descriptor;
    descriptor.width = 2;
    descriptor.height = 2;
    descriptor.rowStride = 8;
    descriptor.pixelFormat = Engine::ImagePixelFormat::BGRA8;
    descriptor.alphaMode = transparent
        ? Engine::ImageAlphaMode::Straight : Engine::ImageAlphaMode::Opaque;
    descriptor.colorSpace = Engine::ImageColorSpace::SRGB;
    const auto result = Engine::ImageBuffer::Allocate(descriptor);
    if (!result) return {};
    auto image = std::move(result.Value());
    const std::vector<std::byte> pixels = {
        std::byte{1}, std::byte{2}, std::byte{3},
        transparent ? std::byte{128} : std::byte{255},
        std::byte{4}, std::byte{5}, std::byte{6}, std::byte{255},
        std::byte{7}, std::byte{8}, std::byte{9}, std::byte{255},
        std::byte{10}, std::byte{11}, std::byte{12}, std::byte{255}};
    std::copy(pixels.begin(), pixels.end(), image.MutableBytes().begin());
    return image;
}

} // namespace

int main() {
    Engine::WicImageCodec codec;
    Check(codec.Supports(Engine::ImageEncodedFormat::Png),
        "WIC advertises in-memory PNG support");

    auto source = MakeImage(true);
    const auto encoded = codec.Encode(
        source, Engine::ImageEncodedFormat::Png);
    Check(encoded && encoded.Value().bytes.size() > 16,
        "WIC encodes an in-memory BGRA8 image");
    if (encoded) {
        const auto decoded = codec.Decode(encoded.Value());
        Check(decoded && decoded.Value().Width() == 2 &&
            decoded.Value().Height() == 2 &&
            decoded.Value().RowStride() == 8 &&
            decoded.Value().PixelFormat() == Engine::ImagePixelFormat::BGRA8,
            "WIC normalizes decoded pixels to BGRA8 with checked stride");
        if (decoded) {
            Check(decoded.Value().Orientation() ==
                    Engine::ImageOrientation::TopDown &&
                decoded.Value().AlphaMode() == Engine::ImageAlphaMode::Straight,
                "WIC preserves top-down orientation and straight alpha metadata");
            const auto pixels = decoded.Value().Bytes();
            Check(pixels.size() >= 4 && pixels[0] == std::byte{1} &&
                pixels[1] == std::byte{2} && pixels[2] == std::byte{3} &&
                pixels[3] == std::byte{128},
                "WIC keeps the first pixel channel order and alpha");
        }

        auto corrupt = encoded.Value();
        corrupt.bytes.resize(7);
        const auto corruptResult = codec.Decode(corrupt);
        Check(!corruptResult &&
            (corruptResult.Error().code ==
                    Engine::ApplicationErrorCode::InvalidData ||
             corruptResult.Error().code ==
                    Engine::ApplicationErrorCode::DecodeFailed),
            "WIC rejects truncated encoded payloads structurally");
    }

    auto opaque = MakeImage(false);
    const auto opaqueEncoded = codec.Encode(
        opaque, Engine::ImageEncodedFormat::Png);
    if (opaqueEncoded) {
        const auto opaqueDecoded = codec.Decode(opaqueEncoded.Value());
        Check(opaqueDecoded && opaqueDecoded.Value().AlphaMode() ==
                Engine::ImageAlphaMode::Opaque,
            "WIC marks an all-opaque decoded image as opaque");
    } else {
        Check(false, "WIC encodes the opaque fixture");
    }

    auto premultiplied = MakeImage(true);
    auto premultipliedDescriptor = premultiplied.Descriptor();
    premultipliedDescriptor.alphaMode = Engine::ImageAlphaMode::Premultiplied;
    const auto normalized = Engine::ImageBuffer::Copy(
        premultipliedDescriptor, premultiplied.Bytes());
    Check(normalized.HasValue(), "premultiplied fixture has a valid descriptor");
    if (normalized) {
        const auto rejected = codec.Encode(
            normalized.Value(), Engine::ImageEncodedFormat::Png);
        Check(!rejected && rejected.Error().code ==
                Engine::ApplicationErrorCode::Unsupported,
            "WIC does not silently reinterpret premultiplied alpha");
    }

    std::cout << "XiE WIC image codec tests: " << checks
              << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
