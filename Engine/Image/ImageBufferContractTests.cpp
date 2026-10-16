#include "Image/ImageBuffer.h"

#include <cstdint>
#include <iostream>
#include <limits>
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

template <typename T>
bool HasError(const Engine::ApplicationResult<T>& result,
    Engine::ApplicationErrorCode code) {
    return !result && result.Error().code == code;
}

Engine::ImageDescriptor Descriptor() {
    Engine::ImageDescriptor descriptor;
    descriptor.width = 2;
    descriptor.height = 2;
    descriptor.rowStride = 8;
    descriptor.pixelFormat = Engine::ImagePixelFormat::BGRA8;
    descriptor.alphaMode = Engine::ImageAlphaMode::Straight;
    descriptor.colorSpace = Engine::ImageColorSpace::SRGB;
    descriptor.orientation = Engine::ImageOrientation::TopDown;
    return descriptor;
}

} // namespace

int main() {
    const auto descriptor = Descriptor();
    const auto allocated = Engine::ImageBuffer::Allocate(descriptor);
    Check(allocated && allocated.Value().ByteSize() == 16,
        "valid layout allocates the declared byte count");
    if (allocated) {
        auto image = std::move(allocated.Value());
        image.MutableBytes()[0] = std::byte{0x11};
        const auto copied = Engine::ImageBuffer::Copy(
            image.Descriptor(), image.Bytes());
        Check(copied && copied.Value().Bytes()[0] == std::byte{0x11},
            "copy preserves descriptor and pixel data");
        image.MutableBytes()[0] = std::byte{0x22};
        Check(copied.Value().Bytes()[0] == std::byte{0x11},
            "ImageBuffer copy owns an independent payload");
        Check(image.View().descriptor == descriptor &&
            image.View().bytes.size() == 16,
            "read-only view exposes the complete owned layout");
    }

    auto zeroWidth = descriptor;
    zeroWidth.width = 0;
    Check(HasError(Engine::ImageBuffer::Allocate(zeroWidth),
        Engine::ApplicationErrorCode::InvalidData),
        "zero dimensions are rejected");

    auto shortStride = descriptor;
    shortStride.rowStride = 7;
    Check(HasError(Engine::ImageBuffer::Allocate(shortStride),
        Engine::ApplicationErrorCode::InvalidData),
        "a row stride shorter than one row is rejected");

    auto bottomUp = descriptor;
    bottomUp.orientation = Engine::ImageOrientation::BottomUp;
    Check(HasError(Engine::ImageBuffer::Allocate(bottomUp),
        Engine::ApplicationErrorCode::Unsupported),
        "ImageBuffer accepts only canonical top-down storage");

    auto tooWide = descriptor;
    tooWide.width = 3;
    Engine::ImageLimits dimensionLimits;
    dimensionLimits.maxDimension = 2;
    Check(HasError(Engine::ImageBuffer::Allocate(tooWide, dimensionLimits),
        Engine::ApplicationErrorCode::TooLarge),
        "dimension limits are enforced before allocation");

    Engine::ImageLimits byteLimits;
    byteLimits.maxBytes = 15;
    Check(HasError(Engine::ImageBuffer::Allocate(descriptor, byteLimits),
        Engine::ApplicationErrorCode::TooLarge),
        "byte limits are enforced before allocation");

    auto invalidFormat = descriptor;
    invalidFormat.pixelFormat = static_cast<Engine::ImagePixelFormat>(255);
    Check(HasError(Engine::ImageBuffer::Allocate(invalidFormat),
        Engine::ApplicationErrorCode::Unsupported),
        "unknown pixel formats are rejected");

    auto invalidOptions = Engine::ImageLimits{};
    invalidOptions.maxBytes = 0;
    Check(HasError(Engine::ImageBuffer::Allocate(descriptor, invalidOptions),
        Engine::ApplicationErrorCode::InvalidOptions),
        "zero image limits are rejected");

    const std::vector<std::byte> shortPayload(15);
    Check(HasError(Engine::ImageBuffer::Copy(descriptor, shortPayload),
        Engine::ApplicationErrorCode::InvalidData),
        "short pixel payloads are rejected");

    auto rgba = descriptor;
    rgba.pixelFormat = Engine::ImagePixelFormat::RGBA8;
    Check(Engine::ImageBuffer::Allocate(rgba).HasValue(),
        "RGBA8 is an explicitly supported exchange format");

    auto huge = descriptor;
    huge.width = 1;
    huge.height = std::numeric_limits<std::uint32_t>::max();
    huge.rowStride = std::numeric_limits<std::uint32_t>::max();
    Engine::ImageLimits hugeLimits;
    hugeLimits.maxDimension = std::numeric_limits<std::uint32_t>::max();
    hugeLimits.maxBytes = 1ull << 40;
    Check(HasError(Engine::ValidateImageLayout(huge, hugeLimits),
        Engine::ApplicationErrorCode::TooLarge),
        "an impractically large layout is rejected without allocation");

    std::cout << "XiE image buffer contract tests: " << checks
              << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
