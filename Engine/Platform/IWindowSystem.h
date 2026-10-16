#pragma once

#include "NativeWindowHandle.h"
#include "Foundation/Math/Types2D.h"
#include "Platform/IRenderSurface.h"
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

    // Optional task-oriented operations used by the application composition
    // layer.  Low-level window systems may keep their historical behavior by
    // leaving these defaults in place.
    virtual bool ActivateWindow(WindowHandle window) {
        (void)window;
        return false;
    }
    virtual bool RequestClose(WindowHandle window) {
        (void)window;
        return false;
    }
    virtual bool ClearCloseRequest(WindowHandle window) {
        (void)window;
        return false;
    }

    virtual bool IsVisible(WindowHandle window) const = 0;
    virtual bool IsMinimized(WindowHandle window) const { (void)window; return false; }
    virtual bool IsMaximized(WindowHandle window) const { (void)window; return false; }
    virtual bool IsFocused(WindowHandle window) const { (void)window; return false; }
    virtual bool IsCloseRequested(WindowHandle window) const = 0;

    virtual float GetDpiScale(WindowHandle window) const = 0;
    virtual RenderSurfaceSize GetFramebufferSize(WindowHandle window) const {
        (void)window;
        return {};
    }
    virtual NativeWindowHandle GetNativeHandle(WindowHandle window) const = 0;
    virtual Vec2F GetWindowScreenPositionPhysical(
        WindowHandle window) const { (void)window; return {}; }
};

} // namespace Engine
