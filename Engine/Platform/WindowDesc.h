#pragma once

#include <string>

namespace Engine {

struct WindowDesc {
    std::string title;
    int width = 1280;
    int height = 720;

    bool visible = true;
    bool resizable = true;
    bool decorated = true;
    bool transparent = false;
    bool alwaysOnTop = false;
    bool toolWindow = false;
    bool noActivate = false;
    bool clickThrough = false;
    float opacity = 1.0f;
};

} // namespace Engine
