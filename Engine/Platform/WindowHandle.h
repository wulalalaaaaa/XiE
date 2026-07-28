#pragma once

#include "Foundation/Handles/GenerationalHandle.h"

namespace Engine {

struct WindowHandleTag;
using WindowHandle = GenerationalHandle<WindowHandleTag>;

} // namespace Engine
