#include "FrameScheduler.h"

#include <algorithm>

namespace Engine {

std::uint64_t FrameScheduler::MakeKey(WindowHandle handle) {
    return (static_cast<std::uint64_t>(handle.generation) << 32u) | handle.index;
}

FrameDecision FrameScheduler::Evaluate(
    const WindowContext& window,
    bool hasActiveAnimation,
    bool hasPendingCommand,
    double nowSeconds
) {
    FrameDecision decision{};

    if (!window.visible || window.framePolicy == FramePolicy::Hidden) {
        decision.waitSeconds = 0.1;
        return decision;
    }

    const bool hasWork = window.dirty || hasActiveAnimation || hasPendingCommand;
    double targetInterval = 0.0;

    switch (window.framePolicy) {
    case FramePolicy::Active:
        targetInterval = 1.0 / 60.0;
        break;
    case FramePolicy::VisibleIdle:
        targetInterval = 1.0 / 30.0;
        break;
    case FramePolicy::Static:
        if (!hasWork) {
            decision.waitSeconds = 0.25;
            return decision;
        }
        decision.update = true;
        decision.render = true;
        m_LastFrameSeconds[MakeKey(window.window)] = nowSeconds;
        return decision;
    case FramePolicy::Hidden:
        decision.waitSeconds = 0.1;
        return decision;
    }

    const std::uint64_t key = MakeKey(window.window);
    auto it = m_LastFrameSeconds.find(key);
    if (it == m_LastFrameSeconds.end()) {
        decision.update = true;
        decision.render = true;
        m_LastFrameSeconds[key] = nowSeconds;
        return decision;
    }

    const double elapsed = nowSeconds - it->second;
    if (elapsed >= targetInterval || hasWork) {
        decision.update = true;
        decision.render = true;
        it->second = nowSeconds;
        return decision;
    }

    decision.waitSeconds = std::max(0.0, targetInterval - elapsed);
    return decision;
}

} // namespace Engine
