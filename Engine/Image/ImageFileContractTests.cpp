#include "Image/ImageFile.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
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

Engine::ApplicationError Error(Engine::ApplicationErrorCode code,
    const char* message) {
    return {code, "ImageFileContractTest", "fake_image_file", message,
        std::nullopt, "fake"};
}

Engine::ImageBuffer MakeImage() {
    Engine::ImageDescriptor descriptor;
    descriptor.width = 4;
    descriptor.height = 2;
    descriptor.rowStride = 16;
    descriptor.pixelFormat = Engine::ImagePixelFormat::RGBA8;
    descriptor.alphaMode = Engine::ImageAlphaMode::Straight;
    descriptor.colorSpace = Engine::ImageColorSpace::SRGB;
    const auto result = Engine::ImageBuffer::Allocate(descriptor);
    if (!result) return {};
    auto image = std::move(result.Value());
    for (std::size_t index = 0; index < image.ByteSize(); ++index)
        image.MutableBytes()[index] = std::byte{
            static_cast<unsigned char>(index + 1)};
    return image;
}

class FakeCodec final : public Engine::IImageCodec {
public:
    Engine::ApplicationResult<Engine::ImageBuffer> Decode(
        const Engine::ImageEncodedPayload& payload,
        const Engine::ImageDecodeOptions&) override {
        if (payload.bytes.empty())
            return Engine::ApplicationResult<Engine::ImageBuffer>::Failure(
                Error(Engine::ApplicationErrorCode::NoData,
                    "encoded payload is empty"));
        return Engine::ApplicationResult<Engine::ImageBuffer>::Success(
            MakeImage());
    }

    Engine::ApplicationResult<Engine::ImageMetadata> Probe(
        const Engine::ImageEncodedPayload& payload,
        const Engine::ImageProbeOptions&) override {
        if (payload.bytes.empty())
            return Engine::ApplicationResult<Engine::ImageMetadata>::Failure(
                Error(Engine::ApplicationErrorCode::NoData,
                    "encoded payload is empty"));
        Engine::ImageMetadata metadata;
        metadata.encodedFormat = payload.format;
        metadata.width = 4;
        metadata.height = 2;
        metadata.frameCount = 1;
        metadata.alphaMode = Engine::ImageAlphaMode::Straight;
        metadata.colorSpace = Engine::ImageColorSpace::SRGB;
        metadata.orientation = Engine::ImageOrientation::TopDown;
        return Engine::ApplicationResult<Engine::ImageMetadata>::Success(
            metadata);
    }

    Engine::ApplicationResult<Engine::ImageEncodedPayload> Encode(
        const Engine::ImageBuffer&, Engine::ImageEncodedFormat format,
        const Engine::ImageEncodeOptions&) override {
        if (format != Engine::ImageEncodedFormat::Png)
            return Engine::ApplicationResult<
                Engine::ImageEncodedPayload>::Failure(Error(
                    Engine::ApplicationErrorCode::Unsupported,
                    "format is unsupported"));
        Engine::ImageEncodedPayload payload;
        payload.format = format;
        payload.bytes = {std::byte{0x89}, std::byte{'P'}, std::byte{'N'},
            std::byte{'G'}};
        return Engine::ApplicationResult<
            Engine::ImageEncodedPayload>::Success(std::move(payload));
    }

    bool Supports(Engine::ImageEncodedFormat format) const noexcept override {
        return format == Engine::ImageEncodedFormat::Png;
    }
    const char* BackendName() const noexcept override { return "fake"; }
};

} // namespace

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "xie_image_file_contract";
    std::error_code cleanup;
    fs::remove_all(root, cleanup);
    fs::create_directories(root, cleanup);
    const auto source = root / "source.png";
    {
        std::ofstream output(source, std::ios::binary);
        const std::vector<unsigned char> signature = {
            0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
            0x66, 0x69, 0x78, 0x74, 0x75, 0x72, 0x65};
        output.write(reinterpret_cast<const char*>(signature.data()),
            static_cast<std::streamsize>(signature.size()));
    }

    auto codec = std::make_shared<FakeCodec>();
    Engine::ImageFileService files(codec);
    const auto encoded = files.ReadEncoded(source);
    Check(encoded && encoded.Value().format == Engine::ImageEncodedFormat::Png,
        "file service owns bytes and detects PNG without exposing streams");
    const auto metadata = files.Inspect(source);
    Check(metadata && metadata.Value().width == 4 &&
        metadata.Value().height == 2 && metadata.Value().frameCount == 1,
        "metadata inspection returns bounded dimensions and frame count");
    const auto decoded = files.DecodeThumbnail(source, {
        2, 2, false, Engine::ImageResizeFilter::Linear, {}});
    Check(decoded && decoded.Value().image.Width() == 2 &&
        decoded.Value().image.Height() == 1 &&
        decoded.Value().image.Orientation() == Engine::ImageOrientation::TopDown,
        "decode and aspect-preserving resize return canonical owned pixels");

    const auto missing = files.ReadEncoded(root / "missing.png");
    Check(!missing && missing.Error().code ==
        Engine::ApplicationErrorCode::FileNotFound,
        "missing files return FileNotFound");
    const auto emptyPath = root / "empty.png";
    std::ofstream(emptyPath, std::ios::binary).close();
    const auto empty = files.ReadEncoded(emptyPath);
    Check(!empty && empty.Error().code == Engine::ApplicationErrorCode::NoData,
        "empty files return NoData");
    Engine::ImageFileReadOptions limited;
    limited.limits.maxEncodedBytes = 3;
    const auto tooLarge = files.ReadEncoded(source, limited);
    Check(!tooLarge && tooLarge.Error().code ==
        Engine::ApplicationErrorCode::TooLarge,
        "encoded file size is rejected before allocation");
    Check(Engine::DetectEncodedFormat({}) ==
        Engine::ImageEncodedFormat::Unknown,
        "unknown encoded bytes do not guess a format");
    const std::vector<std::byte> bmpSignature = {
        std::byte{'B'}, std::byte{'M'}};
    Check(Engine::DetectEncodedFormat(bmpSignature) ==
        Engine::ImageEncodedFormat::Bmp,
        "BMP signature detection is explicit");

    fs::remove_all(root, cleanup);
    std::cout << "XiE image file contract tests: " << checks
              << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
