#pragma once

#include "Foundation/Math/Types2D.h"

namespace Engine::UI2D {

// Backend-neutral visual offset owned by Widget behaviors. It is composed after
// layout and independently from application and interaction-style animation channels.
struct UIWidgetRuntimeProperties {
    Engine::Vec2F positionOffset{};
};

} // namespace Engine::UI2D
