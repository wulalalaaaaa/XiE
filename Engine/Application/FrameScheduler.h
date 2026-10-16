#pragma once

#include "WindowContext.h"

#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace Engine {

enum class FrameReason : std::uint8_t {
    None,
    Input,
    Animation,
    Realtime,
    WindowExpose,
    Resize,
    Camera,
    WorkspaceChange,
    Explicit,
    WindowRestore,
};

[[nodiscard]] constexpr std::string_view FrameReasonName(FrameReason reason) noexcept {
    switch (reason) {
    case FrameReason::None: return "None";
    case FrameReason::Input: return "Input";
    case FrameReason::Animation: return "Animation";
    case FrameReason::Realtime: return "Realtime";
    case FrameReason::WindowExpose: return "WindowExpose";
    case FrameReason::Resize: return "Resize";
    case FrameReason::Camera: return "Camera";
    case FrameReason::WorkspaceChange: return "WorkspaceChange";
    case FrameReason::Explicit: return "Explicit";
    case FrameReason::WindowRestore: return "WindowRestore";
    }
    return "None";
}

struct FrameDecision {
    bool update = false;
    bool render = false;
    bool requested = false;
    bool paced = false;
    double waitSeconds = 0.0;
    FrameReason reason = FrameReason::None;
};

class FrameScheduler {
public:
    void RequestFrame(WindowHandle handle, FrameReason reason = FrameReason::Explicit);
    void AcknowledgeFrame(WindowHandle handle, double nowSeconds);
    void ForgetWindow(WindowHandle handle);

    FrameDecision Evaluate(
        const WindowContext& window,
        bool hasActiveAnimation,
        bool hasPendingCommand,
        double nowSeconds
    );

private:
    static std::uint64_t MakeKey(WindowHandle handle);
    static int ReasonPriority(FrameReason reason) noexcept;

private:
    std::unordered_map<std::uint64_t, double> m_LastFrameSeconds;
    std::unordered_map<std::uint64_t, FrameReason> m_PendingReasons;
};

} // namespace Engine
