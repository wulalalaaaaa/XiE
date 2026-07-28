#pragma once

#include "Foundation/Handles/GenerationalHandle.h"

namespace Engine::UI2D {

struct UINodeHandleTag;
using UINodeHandle = Engine::GenerationalHandle<UINodeHandleTag>;

} // namespace Engine::UI2D
