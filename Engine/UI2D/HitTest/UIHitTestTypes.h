#pragma once

#include "Foundation/Math/Types2D.h"
#include "UI2D/Core/UINodeHandle.h"

#include <cstdint>
#include <vector>

namespace Engine::UI2D {

struct UIHitTestStats {
    std::uint32_t visitedNodes = 0;
    std::uint32_t aabbRejected = 0;
    std::uint32_t clipRejected = 0;
    std::uint32_t geometryRejected = 0;
    std::uint32_t invalidTransformRejected = 0;
};

struct UIHitTestContext {
    UINodeHandle root{};
    bool includeDisabled = false;
    bool debugDumpEnabled = false;
    UIHitTestStats* stats = nullptr;
};

struct UIHitTestResult {
    UINodeHandle target{};
    std::vector<UINodeHandle> route;
    Engine::Vec2F scenePosition{};
    Engine::Vec2F targetLocalPosition{};
    bool hit = false;
};

} // namespace Engine::UI2D
