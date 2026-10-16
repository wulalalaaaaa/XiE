#include "Image/ImageAsync.h"
#include "Image/ImageFile.h"
#include "Image/Windows/WicImageCodec.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace {

int failures = 0;
int checks = 0;

void Check(bool condition, const char* message) {
    ++checks;
    if (condition) {
        std::cout << "[PASS] " << message << '\n';
    } else {
        ++failures;
        std::cerr << "[FAIL] " << message << '\n';
    }
}

Engine::ImageBuffer MakeFixture(bool opaque) {
    Engine::ImageDescriptor descriptor;
    descriptor.width = 2;
    descriptor.height = 2;
    descriptor.rowStride = 8;
    descriptor.pixelFormat = Engine::ImagePixelFormat::BGRA8;
    descriptor.alphaMode = opaque ? Engine::ImageAlphaMode::Opaque
                                  : Engine::ImageAlphaMode::Straight;
    descriptor.colorSpace = Engine::ImageColorSpace::SRGB;
    const auto allocated = Engine::ImageBuffer::Allocate(descriptor);
    if (!allocated) return {};
    auto image = std::move(allocated.Value());
    const std::vector<std::byte> pixels = {
        std::byte{10}, std::byte{20}, std::byte{30},
        opaque ? std::byte{255} : std::byte{96},
        std::byte{40}, std::byte{50}, std::byte{60}, std::byte{255},
        std::byte{70}, std::byte{80}, std::byte{90}, std::byte{255},
        std::byte{100}, std::byte{110}, std::byte{120}, std::byte{255}};
    std::copy(pixels.begin(), pixels.end(), image.MutableBytes().begin());
    return image;
}

bool WriteBytes(const std::filesystem::path& path,
    std::span<const std::byte> bytes) {
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

bool WaitForCompletion(Engine::ImageAsyncService& service,
    Engine::ImageTaskToken token, Engine::ImageAsyncResult& completed) {
    for (int attempt = 0; attempt < 500; ++attempt) {
        const auto drained = service.DrainResults(8,
            [token, &completed](Engine::ImageAsyncResult result) {
                if (result.token == token) completed = std::move(result);
            });
        if (drained && completed.token == token) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return completed.token == token;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    using Clock = std::chrono::steady_clock;

    const auto root = fs::temp_directory_path() / "xie_image_file_wic";
    std::error_code cleanup;
    fs::remove_all(root, cleanup);
    fs::create_directories(root, cleanup);

    auto codec = std::make_shared<Engine::WicImageCodec>();
    Check(codec->Supports(Engine::ImageEncodedFormat::Png) &&
        codec->Supports(Engine::ImageEncodedFormat::Jpeg),
        "WIC advertises the Phase 9 PNG/JPEG memory codec contract");

    const auto transparent = MakeFixture(false);
    const auto opaque = MakeFixture(true);
    const auto png = codec->Encode(transparent, Engine::ImageEncodedFormat::Png);
    const auto jpeg = codec->Encode(opaque, Engine::ImageEncodedFormat::Jpeg);
    Check(png && !png.Value().bytes.empty(),
        "WIC encodes an owned PNG payload for the file service");
    Check(jpeg && !jpeg.Value().bytes.empty(),
        "WIC encodes an owned JPEG payload without exposing COM types");

    const auto pngPath = root / "fixture.png";
    const auto jpegPath = root / "fixture.jpg";
    if (png) Check(WriteBytes(pngPath, png.Value().bytes),
        "PNG fixture is written through an explicit test-owned file");
    if (jpeg) Check(WriteBytes(jpegPath, jpeg.Value().bytes),
        "JPEG fixture is written through an explicit test-owned file");

    auto files = std::make_shared<Engine::ImageFileService>(codec);
    const auto pngMetadata = files->Inspect(pngPath);
    Check(pngMetadata && pngMetadata.Value().encodedFormat ==
        Engine::ImageEncodedFormat::Png && pngMetadata.Value().width == 2 &&
        pngMetadata.Value().height == 2 && pngMetadata.Value().frameCount == 1,
        "WIC probes PNG format, dimensions and frame count from memory-backed file bytes");
    const auto jpegMetadata = files->Inspect(jpegPath);
    Check(jpegMetadata && jpegMetadata.Value().encodedFormat ==
        Engine::ImageEncodedFormat::Jpeg && jpegMetadata.Value().alphaMode ==
        Engine::ImageAlphaMode::Opaque,
        "WIC probes JPEG as an opaque image");

    const auto decoded = files->Decode(pngPath);
    Check(decoded && decoded.Value().image.PixelFormat() ==
        Engine::ImagePixelFormat::BGRA8 &&
        decoded.Value().image.Orientation() == Engine::ImageOrientation::TopDown,
        "WIC file decode returns owned canonical top-down BGRA8 pixels");
    if (decoded) Check(decoded.Value().image.AlphaMode() ==
        Engine::ImageAlphaMode::Straight &&
        decoded.Value().image.Bytes()[3] == std::byte{96},
        "WIC memory decode preserves straight alpha semantics");

    const auto thumbnail = files->DecodeThumbnail(pngPath, {
        1, 1, false, Engine::ImageResizeFilter::Linear, {}});
    Check(thumbnail && thumbnail.Value().image.Width() == 1 &&
        thumbnail.Value().image.Height() == 1 &&
        thumbnail.Value().image.RowStride() == 4,
        "file service performs bounded aspect-preserving thumbnail scaling");

    const auto corruptPath = root / "corrupt.png";
    const std::vector<std::byte> corrupt = {
        std::byte{0x89}, std::byte{'P'}, std::byte{'N'}, std::byte{'G'}};
    Check(WriteBytes(corruptPath, corrupt),
        "corrupt fixture is created only inside the temporary test directory");
    const auto corruptResult = files->Decode(corruptPath);
    Check(!corruptResult && (corruptResult.Error().code ==
        Engine::ApplicationErrorCode::DecodeFailed ||
        corruptResult.Error().code == Engine::ApplicationErrorCode::InvalidData),
        "WIC rejects truncated image data with a structured decode error");

    Engine::ImageFileReadOptions limited;
    limited.limits.maxEncodedBytes = 1;
    const auto tooLarge = files->ReadEncoded(pngPath, limited);
    Check(!tooLarge && tooLarge.Error().code ==
        Engine::ApplicationErrorCode::TooLarge,
        "file service rejects oversized encoded payloads before decode");

    Engine::ImageAsyncOptions asyncOptions;
    asyncOptions.workerCount = 1;
    asyncOptions.maxQueuedTasks = 32;
    asyncOptions.maxPendingResults = 32;
    Engine::ImageAsyncService tasks(files, asyncOptions);
    Engine::ImageAsyncRequest oneRequest;
    oneRequest.sourcePath = pngPath;
    oneRequest.resize = Engine::ImageResizeOptions{
        1, 1, false, Engine::ImageResizeFilter::Linear, {}};
    const auto singleStart = Clock::now();
    const auto one = tasks.Submit(oneRequest);
    Engine::ImageAsyncResult oneResult;
    const bool oneDone = one && WaitForCompletion(tasks, one.Value(), oneResult);
    const auto singleUs = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now() - singleStart).count();
    Check(oneDone && oneResult.state == Engine::ImageTaskState::Completed &&
        oneResult.value && oneResult.value->image.Width() == 1,
        "WIC-backed async service returns a completed owned thumbnail");

    std::vector<Engine::ImageTaskToken> batch;
    const auto batchStart = Clock::now();
    for (int index = 0; index < 10; ++index) {
        const auto submitted = tasks.Submit(oneRequest);
        if (submitted) batch.push_back(submitted.Value());
    }
    std::size_t completed = 0;
    for (int attempt = 0; attempt < 1000 && completed < batch.size(); ++attempt) {
        const auto drained = tasks.DrainResults(32,
            [&completed, &batch](Engine::ImageAsyncResult result) {
                if (std::find(batch.begin(), batch.end(), result.token) !=
                    batch.end() && result.state == Engine::ImageTaskState::Completed)
                    ++completed;
            });
        if (completed == batch.size()) break;
        if (!drained || drained.Value() == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const auto batchUs = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now() - batchStart).count();
    Check(batch.size() == 10 && completed == batch.size(),
        "WIC async batch completes ten bounded preview tasks without loss");
    std::cout << "phase9_wic_perf single_decode_us=" << singleUs
              << " batch10_us=" << batchUs << '\n';

    (void)tasks.Shutdown();
    (void)tasks.Shutdown();
    fs::remove_all(root, cleanup);
    std::cout << "XiE WIC image file tests: " << checks
              << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
