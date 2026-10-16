#pragma once

#include "Software2DRenderer.h"
#include "Platform/NativeWindowHandle.h"

namespace Engine {

class LayeredWindowPresenter {
public:
    explicit LayeredWindowPresenter(NativeWindowHandle hwnd = 0);

    void SetWindow(NativeWindowHandle hwnd);
    void SetOpacity(float opacity);
    bool Present(const PremultipliedBitmap& bitmap);

private:
    NativeWindowHandle m_Window = 0;
    float m_Opacity = 1.0f;
};

} // namespace Engine
