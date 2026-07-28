#pragma once

#include "UI2D/Core/UINodeHandle.h"
#include "UI2D/HitTest/UIHitTestTypes.h"

#include <vector>

namespace Engine::UI2D {

struct UIEventRoute {
    std::vector<UINodeHandle> capturePath;
    UINodeHandle target{};
    std::vector<UINodeHandle> bubblePath;
};

} // namespace Engine::UI2D
