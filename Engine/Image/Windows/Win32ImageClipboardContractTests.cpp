#include "Image/Windows/Win32ImageClipboardSource.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
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

Engine::ApplicationError Error(Engine::ApplicationErrorCode code,
    const char* message) {
    return {code, "ClipboardContractTest", "image_clipboard", message,
        std::nullopt, "fake"};
}

void PutU16(std::vector<std::byte>& bytes, std::size_t offset,
    std::uint16_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void PutU32(std::vector<std::byte>& bytes, std::size_t offset,
    std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void PutI32(std::vector<std::byte>& bytes, std::size_t offset,
    std::int32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

std::byte Byte(unsigned value) {
    return static_cast<std::byte>(value & 0xffu);
}

std::vector<std::byte> MakeDib24(bool topDown) {
    constexpr std::uint32_t width = 2;
    constexpr std::uint32_t height = 2;
    constexpr std::uint32_t stride = 8;
    std::vector<std::byte> bytes(40 + stride * height);
    PutU32(bytes, 0, 40);
    PutI32(bytes, 4, static_cast<std::int32_t>(width));
    PutI32(bytes, 8, topDown ? -static_cast<std::int32_t>(height)
                              : static_cast<std::int32_t>(height));
    PutU16(bytes, 12, 1);
    PutU16(bytes, 14, 24);
    PutU32(bytes, 16, 0);
    PutU32(bytes, 20, stride * height);

    // Each row is BGR pixels followed by two padding bytes.  For a
    // bottom-up DIB the first file row is the image's bottom row.
    const std::size_t firstRow = 40;
    const std::size_t secondRow = firstRow + stride;
    bytes[firstRow + 0] = Byte(1);
    bytes[firstRow + 1] = Byte(2);
    bytes[firstRow + 2] = Byte(3);
    bytes[firstRow + 3] = Byte(4);
    bytes[firstRow + 4] = Byte(5);
    bytes[firstRow + 5] = Byte(6);
    bytes[secondRow + 0] = Byte(11);
    bytes[secondRow + 1] = Byte(12);
    bytes[secondRow + 2] = Byte(13);
    bytes[secondRow + 3] = Byte(14);
    bytes[secondRow + 4] = Byte(15);
    bytes[secondRow + 5] = Byte(16);
    return bytes;
}

std::vector<std::byte> MakeDibV5() {
    constexpr std::uint32_t width = 2;
    constexpr std::uint32_t height = 2;
    constexpr std::uint32_t stride = 8;
    std::vector<std::byte> bytes(124 + stride * height);
    PutU32(bytes, 0, 124);
    PutI32(bytes, 4, static_cast<std::int32_t>(width));
    PutI32(bytes, 8, -static_cast<std::int32_t>(height));
    PutU16(bytes, 12, 1);
    PutU16(bytes, 14, 32);
    PutU32(bytes, 16, 3);
    PutU32(bytes, 20, stride * height);
    PutU32(bytes, 40, 0x00ff0000u);
    PutU32(bytes, 44, 0x0000ff00u);
    PutU32(bytes, 48, 0x000000ffu);
    PutU32(bytes, 52, 0xff000000u);
    PutU32(bytes, 56, 0x73524742u);

    // BGRA byte order for the standard masks: B=0x10, G=0x20,
    // R=0x40, A=0x80.  The second pixel has a fully opaque alpha.
    bytes[124 + 0] = Byte(0x10);
    bytes[124 + 1] = Byte(0x20);
    bytes[124 + 2] = Byte(0x40);
    bytes[124 + 3] = Byte(0x80);
    bytes[124 + 4] = Byte(0x01);
    bytes[124 + 5] = Byte(0x02);
    bytes[124 + 6] = Byte(0x03);
    bytes[124 + 7] = Byte(0xff);
    return bytes;
}

class FakeProvider final : public Engine::IWin32ClipboardProvider {
public:
    Engine::ApplicationResult<Engine::Win32ClipboardSnapshot> result =
        Engine::ApplicationResult<Engine::Win32ClipboardSnapshot>::Success({});

    Engine::ApplicationResult<Engine::Win32ClipboardSnapshot> Read(
        const Engine::ImageClipboardReadOptions&) override {
        return result;
    }
};

bool Is(Engine::ApplicationResult<Engine::ImageBuffer>& result,
    Engine::ApplicationErrorCode code) {
    return !result && result.Error().code == code;
}

} // namespace

int main() {
    const auto bottomUp = MakeDib24(false);
    const auto bottomDecoded = Engine::DecodeWin32BitmapPayload(
        Engine::Win32ClipboardImageFormat::Dib, bottomUp);
    Check(bottomDecoded && bottomDecoded.Value().Width() == 2 &&
        bottomDecoded.Value().Height() == 2 &&
        bottomDecoded.Value().Bytes()[0] == Byte(11) &&
        bottomDecoded.Value().Bytes()[1] == Byte(12) &&
        bottomDecoded.Value().Bytes()[2] == Byte(13) &&
        bottomDecoded.Value().Bytes()[3] == Byte(0xff),
        "CF_DIB bottom-up rows are normalized to top-down BGRA8");

    const auto topDown = MakeDib24(true);
    const auto topDecoded = Engine::DecodeWin32BitmapPayload(
        Engine::Win32ClipboardImageFormat::Dib, topDown);
    Check(topDecoded && topDecoded.Value().Bytes()[0] == Byte(1) &&
        topDecoded.Value().Bytes()[1] == Byte(2) &&
        topDecoded.Value().Bytes()[2] == Byte(3) &&
        topDecoded.Value().Bytes()[3] == Byte(0xff),
        "top-down CF_DIB rows remain in display order");

    const auto v5 = MakeDibV5();
    const auto v5Decoded = Engine::DecodeWin32BitmapPayload(
        Engine::Win32ClipboardImageFormat::DibV5, v5);
    Check(v5Decoded && v5Decoded.Value().AlphaMode() ==
            Engine::ImageAlphaMode::Straight &&
        v5Decoded.Value().ColorSpace() == Engine::ImageColorSpace::SRGB &&
        v5Decoded.Value().Bytes()[0] == Byte(0x10) &&
        v5Decoded.Value().Bytes()[1] == Byte(0x20) &&
        v5Decoded.Value().Bytes()[2] == Byte(0x40) &&
        v5Decoded.Value().Bytes()[3] == Byte(0x80),
        "CF_DIBV5 masks, alpha and sRGB metadata are validated and preserved");

    auto truncated = bottomUp;
    truncated.resize(42);
    auto truncatedResult = Engine::DecodeWin32BitmapPayload(
        Engine::Win32ClipboardImageFormat::Dib, truncated);
    Check(Is(truncatedResult, Engine::ApplicationErrorCode::InvalidData),
        "truncated DIB pixel data is rejected");

    auto wrongV5 = bottomUp;
    auto wrongV5Result = Engine::DecodeWin32BitmapPayload(
        Engine::Win32ClipboardImageFormat::DibV5, wrongV5);
    Check(Is(wrongV5Result, Engine::ApplicationErrorCode::InvalidData),
        "DIBV5 requires a complete BITMAPV5HEADER");

    auto limits = Engine::ImageLimits{};
    limits.maxEncodedBytes = 8;
    auto oversizedResult = Engine::DecodeWin32BitmapPayload(
        Engine::Win32ClipboardImageFormat::Dib, bottomUp, limits);
    Check(Is(oversizedResult, Engine::ApplicationErrorCode::TooLarge),
        "encoded DIB limits are enforced before parsing");

    auto provider = std::make_shared<FakeProvider>();
    Engine::Win32ImageClipboardSource source({}, provider);
    Engine::Win32ClipboardSnapshot validSnapshot;
    validSnapshot.sequenceNumber = 77;
    validSnapshot.hasSupportedFormat = true;
    validSnapshot.payloads.push_back({
        Engine::Win32ClipboardImageFormat::Dib, bottomUp});
    provider->result = Engine::ApplicationResult<
        Engine::Win32ClipboardSnapshot>::Success(validSnapshot);
    const auto captured = source.ReadImage();
    Check(captured && captured.Value().sequenceNumber == 77 &&
        captured.Value().image.Width() == 2,
        "clipboard source returns an owned decoded image and sequence");

    provider->result = Engine::ApplicationResult<
        Engine::Win32ClipboardSnapshot>::Failure(
            Error(Engine::ApplicationErrorCode::Busy, "clipboard busy"));
    const auto busy = source.ReadImage();
    Check(!busy && busy.Error().code == Engine::ApplicationErrorCode::Busy,
        "clipboard busy is propagated as a finite structured result");

    provider->result = Engine::ApplicationResult<
        Engine::Win32ClipboardSnapshot>::Success({});
    const auto empty = source.ReadImage();
    Check(!empty && empty.Error().code == Engine::ApplicationErrorCode::NoData,
        "empty clipboard is distinct from unsupported image formats");

    Engine::Win32ClipboardSnapshot unsupported;
    unsupported.hasUnsupportedImageFormat = true;
    provider->result = Engine::ApplicationResult<
        Engine::Win32ClipboardSnapshot>::Success(unsupported);
    const auto unsupportedResult = source.ReadImage();
    Check(!unsupportedResult && unsupportedResult.Error().code ==
            Engine::ApplicationErrorCode::Unsupported,
        "unsupported clipboard formats have a distinct result");

    Engine::Win32ClipboardSnapshot tooLarge;
    tooLarge.hasOversizedPayload = true;
    provider->result = Engine::ApplicationResult<
        Engine::Win32ClipboardSnapshot>::Success(tooLarge);
    const auto tooLargeResult = source.ReadImage();
    Check(!tooLargeResult && tooLargeResult.Error().code ==
            Engine::ApplicationErrorCode::TooLarge,
        "oversized clipboard payloads have a distinct result");

    provider->result = Engine::ApplicationResult<
        Engine::Win32ClipboardSnapshot>::Success({
            {{Engine::Win32ClipboardImageFormat::Dib, truncated}}, 1, true,
            false, false});
    const auto corrupt = source.ReadImage();
    Check(!corrupt && corrupt.Error().code ==
            Engine::ApplicationErrorCode::InvalidData,
        "corrupt supported clipboard payloads are rejected");

    std::optional<Engine::ApplicationResult<Engine::ImageClipboardCapture>>
        wrongThread;
    std::thread worker([&] { wrongThread.emplace(source.ReadImage()); });
    worker.join();
    Check(wrongThread && !wrongThread->HasValue() &&
        wrongThread->Error().code == Engine::ApplicationErrorCode::WrongThread,
        "Win32 clipboard source rejects calls from a foreign thread");

    std::cout << "XiE Win32 image clipboard contract tests: " << checks
              << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
