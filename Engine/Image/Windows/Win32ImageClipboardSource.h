#pragma once

#include "Image/ImageClipboard.h"
#include "Image/ImageCodec.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace Engine {

enum class Win32ClipboardImageFormat : std::uint8_t {
    Png,
    DibV5,
    Dib,
};

struct Win32ClipboardPayload {
    Win32ClipboardImageFormat format = Win32ClipboardImageFormat::Png;
    std::vector<std::byte> bytes;
};

struct Win32ClipboardSnapshot {
    std::vector<Win32ClipboardPayload> payloads;
    std::uint64_t sequenceNumber = 0;
    bool hasSupportedFormat = false;
    bool hasUnsupportedImageFormat = false;
    bool hasOversizedPayload = false;
};

class IWin32ClipboardProvider {
public:
    virtual ~IWin32ClipboardProvider() = default;
    virtual ApplicationResult<Win32ClipboardSnapshot> Read(
        const ImageClipboardReadOptions& options) = 0;
};

// Parses a CF_DIB/CF_DIBV5 payload and normalizes it to owned top-down BGRA8.
// The function is public for deterministic parser tests; it exposes no native
// Windows types or handles.
ApplicationResult<ImageBuffer> DecodeWin32BitmapPayload(
    Win32ClipboardImageFormat format,
    std::span<const std::byte> bytes,
    const ImageLimits& limits = {});

class Win32ImageClipboardSource final : public IImageClipboardSource {
public:
    explicit Win32ImageClipboardSource(
        std::shared_ptr<IImageCodec> codec = {},
        std::shared_ptr<IWin32ClipboardProvider> provider = {});

    ApplicationResult<ImageClipboardCapture> ReadImage(
        const ImageClipboardReadOptions& options = {}) override;
    [[nodiscard]] const char* BackendName() const noexcept override {
        return "win32_clipboard";
    }

private:
    std::shared_ptr<IImageCodec> m_Codec;
    std::shared_ptr<IWin32ClipboardProvider> m_Provider;
    std::thread::id m_OwnerThread;
};

} // namespace Engine
