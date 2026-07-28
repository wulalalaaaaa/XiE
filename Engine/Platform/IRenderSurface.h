#pragma once

#include "WindowHandle.h"

namespace Engine {

struct RenderSurfaceSize {
    int width = 0;
    int height = 0;
};

class IRenderSurface {
public:
    virtual ~IRenderSurface() = default;

    virtual WindowHandle GetWindow() const = 0;
    virtual void MakeCurrent() = 0;
    virtual void Present() = 0;
    virtual void Resize(int width, int height) = 0;
    virtual RenderSurfaceSize GetFramebufferSize() const = 0;
};

} // namespace Engine
