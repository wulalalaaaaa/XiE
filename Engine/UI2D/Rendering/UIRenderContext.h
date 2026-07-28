#pragma once

#include "Foundation/Math/Types2D.h"
#include "UI2D/Core/UINodeHandle.h"

#include <cstdint>

namespace Engine::UI2D {

struct UIRenderContext {
    Engine::Vec2F logicalViewportSize{};
    float dpiScale = 1.0f;
    UINodeHandle root{};
    bool debugDrawBounds = false;
};

struct UIRenderResult {
    bool success = true;
    bool drawListChanged = false;
    std::uint32_t visitedNodeCount = 0;
    std::uint32_t emittedNodeCount = 0;
    std::uint32_t emittedCommandCount = 0;
    std::uint32_t clipPushCount = 0;
    std::uint32_t clipPopCount = 0;
    std::uint32_t missingTextureCount = 0;
    std::uint32_t loadingTextureCount = 0;
    std::uint32_t failedTextureCount = 0;
    std::uint32_t missingTextLayoutCount = 0;
    std::uint32_t invalidVisualCount = 0;
    std::uint32_t invalidTransformCount = 0;
};

} // namespace Engine::UI2D
