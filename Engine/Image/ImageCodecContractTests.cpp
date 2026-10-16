#include "Image/ImageClipboard.h"
#include "Image/ImageCodec.h"
#include "Image/ImageServices.h"

#include <iostream>
#include <memory>
#include <utility>

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

Engine::ApplicationError Error(Engine::ApplicationErrorCode code,
    const char* message) {
    return {code, "ImageCodecContractTest", "image", message,
        std::nullopt, "fake"};
}

Engine::ImageBuffer MakeImage() {
    Engine::ImageDescriptor descriptor;
    descriptor.width = 1;
    descriptor.height = 1;
    descriptor.rowStride = 4;
    descriptor.pixelFormat = Engine::ImagePixelFormat::RGBA8;
    descriptor.alphaMode = Engine::ImageAlphaMode::Straight;
    const auto result = Engine::ImageBuffer::Allocate(descriptor);
    if (!result) return {};
    auto image = std::move(result.Value());
    image.MutableBytes()[0] = std::byte{1};
    image.MutableBytes()[1] = std::byte{2};
    image.MutableBytes()[2] = std::byte{3};
    image.MutableBytes()[3] = std::byte{4};
    return image;
}

class FakeCodec final : public Engine::IImageCodec {
public:
    int decodeCalls = 0;
    int encodeCalls = 0;

    Engine::ApplicationResult<Engine::ImageBuffer> Decode(
        const Engine::ImageEncodedPayload& payload,
        const Engine::ImageDecodeOptions&) override {
        ++decodeCalls;
        if (payload.bytes.empty()) {
            return Engine::ApplicationResult<Engine::ImageBuffer>::Failure(
                Error(Engine::ApplicationErrorCode::NoData,
                    "encoded payload is empty"));
        }
        return Engine::ApplicationResult<Engine::ImageBuffer>::Success(
            MakeImage());
    }

    Engine::ApplicationResult<Engine::ImageEncodedPayload> Encode(
        const Engine::ImageBuffer& image, Engine::ImageEncodedFormat format,
        const Engine::ImageEncodeOptions&) override {
        ++encodeCalls;
        if (format != Engine::ImageEncodedFormat::Png) {
            return Engine::ApplicationResult<
                Engine::ImageEncodedPayload>::Failure(Error(
                    Engine::ApplicationErrorCode::Unsupported,
                    "format is not supported"));
        }
        Engine::ImageEncodedPayload payload;
        payload.format = format;
        payload.bytes.assign(image.Bytes().begin(), image.Bytes().end());
        return Engine::ApplicationResult<
            Engine::ImageEncodedPayload>::Success(std::move(payload));
    }

    bool Supports(Engine::ImageEncodedFormat format) const noexcept override {
        return format == Engine::ImageEncodedFormat::Png;
    }
    const char* BackendName() const noexcept override { return "fake"; }
};

class FakeClipboard final : public Engine::IImageClipboardSource {
public:
    Engine::ApplicationResult<Engine::ImageClipboardCapture> ReadImage(
        const Engine::ImageClipboardReadOptions&) override {
        return Engine::ApplicationResult<
            Engine::ImageClipboardCapture>::Success({MakeImage(), 9});
    }
    const char* BackendName() const noexcept override { return "fake"; }
};

} // namespace

int main() {
    auto codec = std::make_shared<FakeCodec>();
    Check(codec->Supports(Engine::ImageEncodedFormat::Png),
        "fake codec advertises its supported format");
    const auto encoded = codec->Encode(
        MakeImage(), Engine::ImageEncodedFormat::Png, {});
    Check(encoded && encoded.Value().bytes.size() == 4,
        "codec result owns encoded bytes");
    if (encoded) {
        const auto decoded = codec->Decode(encoded.Value(), {});
        Check(decoded && decoded.Value().ByteSize() == 4,
            "codec contract round-trips an owned image payload");
    }
    const auto empty = codec->Decode({}, {});
    Check(!empty && empty.Error().code == Engine::ApplicationErrorCode::NoData,
        "codec reports empty payload as structured no-data");

    auto clipboard = std::make_shared<FakeClipboard>();
    Engine::ImageServices services(codec, clipboard);
    Check(services.HasCodec() && services.HasClipboard(),
        "image services discover independently injected capabilities");
    Check(services.CodecBackendName() == "fake" &&
        services.ClipboardBackendName() == "fake",
        "service discovery exposes backend identity without native types");
    const auto borrowedCodec = services.Codec();
    const auto borrowedClipboard = services.Clipboard();
    Check(borrowedCodec && borrowedClipboard,
        "consumers can borrow services with shared ownership");
    Check(services.Shutdown().HasValue(), "image service shutdown succeeds");
    Check(services.IsShuttingDown() && !services.HasCodec() &&
        !services.HasClipboard() && !services.Codec() &&
        !services.Clipboard(),
        "shutdown removes discoverable service entries");
    Check(services.Shutdown().HasValue(),
        "repeated image service shutdown is idempotent");
    Check(borrowedCodec && borrowedClipboard,
        "borrowed implementation lifetime is independent of registry reset");

    std::cout << "XiE image codec contract tests: " << checks
              << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
