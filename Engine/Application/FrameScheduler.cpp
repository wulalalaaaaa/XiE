#include "FrameScheduler.h"

#include <algorithm>

namespace Engine {

std::uint64_t FrameScheduler::MakeKey(WindowHandle handle) {
    return (static_cast<std::uint64_t>(handle.generation) << 32u) | handle.index;
}

int FrameScheduler::ReasonPriority(FrameReason reason) noexcept {
    switch (reason) {
    case FrameReason::WindowRestore:
    case FrameReason::WindowExpose:
    case FrameReason::Resize:
        return 5;
    case FrameReason::Realtime:
    case FrameReason::WorkspaceChange:
    case FrameReason::Camera:
        return 4;
    case FrameReason::Input:
        return 3;
    case FrameReason::Animation:
        return 2;
    case FrameReason::Explicit:
        return 1;
    case FrameReason::None:
        return 0;
    }
    return 0;
}

void FrameScheduler::RequestFrame(WindowHandle handle, FrameReason reason) {
    if (!handle.IsValid() || reason == FrameReason::None) return;
    const std::uint64_t key = MakeKey(handle);
    auto [it, inserted] = m_PendingReasons.emplace(key, reason);
    if (!inserted && ReasonPriority(reason) > ReasonPriority(it->second)) {
        it->second = reason;
    }
}

void FrameScheduler::AcknowledgeFrame(WindowHandle handle, double nowSeconds) {
    if (!handle.IsValid()) return;
    const std::uint64_t key = MakeKey(handle);
    m_PendingReasons.erase(key);
    m_LastFrameSeconds[key] = nowSeconds;
}

void FrameScheduler::ForgetWindow(WindowHandle handle) {
    if (!handle.IsValid()) return;
    const std::uint64_t key = MakeKey(handle);
    m_LastFrameSeconds.erase(key);
    m_PendingReasons.erase(key);
}

FrameDecision FrameScheduler::Evaluate(
    const WindowContext& window,
    bool hasActiveAnimation,
    bool hasPendingCommand,
    double nowSeconds
) {
    FrameDecision decision{};
    const std::uint64_t key = MakeKey(window.window);
    const auto request = m_PendingReasons.find(key);
    const bool requested = window.dirty || request != m_PendingReasons.end();
    decision.requested = requested;
    decision.reason = request == m_PendingReasons.end()
        ? (window.dirty ? FrameReason::Explicit : FrameReason::None)
        : request->second;

    if (!window.visible || window.minimized || window.framePolicy == FramePolicy::Hidden) {
        decision.waitSeconds = 0.1;
        return decision;
    }

    const FramePolicy effectivePolicy = !window.focused && window.framePolicy == FramePolicy::Active
        ? FramePolicy::VisibleIdle : window.framePolicy;
    const bool hasWork = requested || hasActiveAnimation || hasPendingCommand ||
        effectivePolicy == FramePolicy::Active;
    if (decision.reason == FrameReason::None && hasActiveAnimation)
        decision.reason = FrameReason::Animation;
    double targetInterval = 0.0;

    switch (effectivePolicy) {
    case FramePolicy::Active:
        targetInterval = 1.0 / 60.0;
        break;
    case FramePolicy::VisibleIdle:
        targetInterval = 1.0 / 15.0;
        break;
    case FramePolicy::Static:
        if (!hasWork) {
            return decision;
        }
        if (hasActiveAnimation) targetInterval = 1.0 / 60.0;
        else if (window.focused &&
                 (decision.reason == FrameReason::Input || decision.reason == FrameReason::Camera))
            targetInterval = 1.0 / 60.0;
        break;
    case FramePolicy::Hidden:
        decision.waitSeconds = 0.1;
        return decision;
    }

    if (!hasWork) return decision;

    auto it = m_LastFrameSeconds.find(key);
    if (it == m_LastFrameSeconds.end()) {
        decision.update = true;
        decision.render = true;
        m_LastFrameSeconds[key] = nowSeconds;
        return decision;
    }

    const double elapsed = nowSeconds - it->second;
    if (targetInterval <= 0.0 || elapsed >= targetInterval) {
        decision.update = true;
        decision.render = true;
        it->second = nowSeconds;
        return decision;
    }

    decision.paced = true;
    decision.waitSeconds = std::max(0.0, targetInterval - elapsed);
    return decision;
}

} // namespace Engine
