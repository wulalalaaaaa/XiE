#pragma once

#include "Foundation/Math/Types2D.h"
#include "UI2D/Core/UINodeHandle.h"

namespace Engine::UI2D {

struct ResolvedUITheme;

struct UILayoutContext {
    Engine::Vec2F logicalWindowSize{};
    float dpiScale = 1.0f;
    const ResolvedUITheme& theme;
    UINodeHandle layoutRoot{};
};

} // namespace Engine::UI2D
