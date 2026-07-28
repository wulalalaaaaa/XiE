#pragma once

#include "Foundation/Handles/GenerationalHandle.h"

namespace Engine::UI2D {

struct UIWidgetHandleTag;
using UIWidgetHandle = Engine::GenerationalHandle<UIWidgetHandleTag>;
struct UIWidgetConnectionHandleTag;
using UIWidgetConnectionHandle = Engine::GenerationalHandle<UIWidgetConnectionHandleTag>;

} // namespace Engine::UI2D
