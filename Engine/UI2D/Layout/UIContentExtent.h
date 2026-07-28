#pragma once

#include "Foundation/Math/Types2D.h"
#include "UI2D/Core/UINodeHandle.h"

namespace Engine::UI2D {

class UIScene;

struct UIContentExtent {
    Engine::Vec2F minimum{};
    Engine::Vec2F maximum{};
    Engine::Vec2F size{};
};

[[nodiscard]] UIContentExtent ComputeUIContentExtent(const UIScene& scene, UINodeHandle parent);

} // namespace Engine::UI2D
