#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Engine {

enum class ApplicationCapability {
    StandardWindow,
    TransparentWindow,
    UI2D,
    PointerAndKeyboardInput,
    TextInput,
    TextClipboard,
    OpenGLPresentation,
    LayeredPresentation,
    Diagnostics,
    DesktopIntegration,
    SystemTray,
    GlobalShortcut,
    SingleInstance,
    PrimaryInstanceActivation,
    MediaPlayback,
    ImageCodec,
    ImageClipboard,
    ImageFile,
    ImageAsyncTasks,
};

enum class CapabilityStatus {
    Supported,
    Unavailable,
    Degraded,
};

// Runtime health is kept beside the existing capability description rather
// than in a second diagnostics hierarchy.  The fields are intentionally
// operation-oriented so callers can distinguish a capability being absent
// from a recent operation failing while still retaining the last failure
// after recovery.
struct CapabilityHealth {
    CapabilityStatus status = CapabilityStatus::Unavailable;
    std::string reason;
    std::string lastSuccessfulOperation;
    std::string lastFailedOperation;
    std::optional<std::int64_t> lastNativeError;
    std::uint32_t consecutiveFailures = 0;
    bool recovering = false;
    std::uint64_t revision = 0;
};

struct CapabilityInfo {
    ApplicationCapability capability{};
    CapabilityStatus status = CapabilityStatus::Unavailable;
    bool compileTimeAvailable = false;
    bool platformAvailable = false;
    bool runtimeInitialized = false;
    std::string reason;
    std::string backend;
    CapabilityHealth health{};
};

inline const char* ToString(ApplicationCapability capability) noexcept {
    switch (capability) {
    case ApplicationCapability::StandardWindow: return "standard_window";
    case ApplicationCapability::TransparentWindow: return "transparent_window";
    case ApplicationCapability::UI2D: return "ui2d";
    case ApplicationCapability::PointerAndKeyboardInput: return "pointer_keyboard_input";
    case ApplicationCapability::TextInput: return "text_input";
    case ApplicationCapability::TextClipboard: return "text_clipboard";
    case ApplicationCapability::OpenGLPresentation: return "opengl_presentation";
    case ApplicationCapability::LayeredPresentation: return "layered_presentation";
    case ApplicationCapability::Diagnostics: return "diagnostics";
    case ApplicationCapability::DesktopIntegration: return "desktop_integration";
    case ApplicationCapability::SystemTray: return "system_tray";
    case ApplicationCapability::GlobalShortcut: return "global_shortcut";
    case ApplicationCapability::SingleInstance: return "single_instance";
    case ApplicationCapability::PrimaryInstanceActivation:
        return "primary_instance_activation";
    case ApplicationCapability::MediaPlayback: return "media_playback";
    case ApplicationCapability::ImageCodec: return "image_codec";
    case ApplicationCapability::ImageClipboard: return "image_clipboard";
    case ApplicationCapability::ImageFile: return "image_file";
    case ApplicationCapability::ImageAsyncTasks: return "image_async_tasks";
    }
    return "unknown_capability";
}

inline const char* ToString(CapabilityStatus status) noexcept {
    switch (status) {
    case CapabilityStatus::Supported: return "supported";
    case CapabilityStatus::Unavailable: return "unavailable";
    case CapabilityStatus::Degraded: return "degraded";
    }
    return "unavailable";
}

class CapabilitySnapshot final {
public:
    CapabilitySnapshot() = default;
    explicit CapabilitySnapshot(std::vector<CapabilityInfo> values)
        : m_Values(std::move(values)) {}

    [[nodiscard]] std::span<const CapabilityInfo> All() const noexcept {
        return m_Values;
    }
    [[nodiscard]] const CapabilityInfo* Find(
        ApplicationCapability capability) const noexcept {
        for (const auto& value : m_Values) {
            if (value.capability == capability) return &value;
        }
        return nullptr;
    }
    [[nodiscard]] bool Supports(ApplicationCapability capability) const noexcept {
        const auto* value = Find(capability);
        return value != nullptr && value->status != CapabilityStatus::Unavailable;
    }

private:
    std::vector<CapabilityInfo> m_Values;
};

} // namespace Engine
