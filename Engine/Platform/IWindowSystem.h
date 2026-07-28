#pragma once

#include "NativeWindowHandle.h"
#include "WindowDesc.h"
#include "WindowHandle.h"

namespace Engine {

class IWindowSystem {
public:
    virtual ~IWindowSystem() = default;

    virtual WindowHandle CreateWindow(const WindowDesc& desc) = 0;
    virtual void DestroyWindow(WindowHandle window) = 0;

    virtual void ShowWindow(WindowHandle window) = 0;
    virtual void HideWindow(WindowHandle window) = 0;

    virtual void SetWindowPosition(WindowHandle window, int x, int y) = 0;
    virtual void SetWindowSize(WindowHandle window, int width, int height) = 0;

    virtual bool IsVisible(WindowHandle window) const = 0;
    virtual bool IsCloseRequested(WindowHandle window) const = 0;

    virtual float GetDpiScale(WindowHandle window) const = 0;
    virtual NativeWindowHandle GetNativeHandle(WindowHandle window) const = 0;
};

} // namespace Engine
