#pragma once

#include "Foundation/Math/Types2D.h"

namespace Engine::UI2D {

struct UITransform {
    Engine::Vec2F position{};
    Engine::Vec2F size{};
    Engine::Vec2F scale{1.0f, 1.0f};
    Engine::Vec2F pivot{};
    float rotationRadians = 0.0f;

    friend constexpr bool operator==(UITransform, UITransform) = default;
};

} // namespace Engine::UI2D
