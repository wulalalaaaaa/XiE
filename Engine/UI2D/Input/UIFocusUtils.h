#pragma once

#include "UI2D/Core/UINodeHandle.h"

namespace Engine::UI2D {

class UIScene;

[[nodiscard]] bool IsInFocusRoot(
    const UIScene& scene, UINodeHandle node, UINodeHandle focusRoot);
[[nodiscard]] bool IsFocusable(
    const UIScene& scene, UINodeHandle node, UINodeHandle focusRoot);

} // namespace Engine::UI2D
