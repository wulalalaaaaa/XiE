#pragma once

#include "WindowContext.h"

#include <cstdint>
#include <unordered_map>

namespace Engine {

struct FrameDecision {
    bool update = false;
    bool render = false;
    double waitSeconds = 0.0;
};

class FrameScheduler {
public:
    FrameDecision Evaluate(
        const WindowContext& window,
        bool hasActiveAnimation,
        bool hasPendingCommand,
        double nowSeconds
    );

private:
    static std::uint64_t MakeKey(WindowHandle handle);

private:
    std::unordered_map<std::uint64_t, double> m_LastFrameSeconds;
};

} // namespace Engine
