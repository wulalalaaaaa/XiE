#pragma once

#include "Platform/IRenderSurface.h"
#include "Platform/WindowHandle.h"
#include "Renderer2D/IWindowRenderPipeline.h"

#include <memory>

namespace Engine {

enum class FramePolicy {
    Active,
    VisibleIdle,
    Static,
    Hidden
};

struct WindowContext {
    WindowHandle window;
    std::unique_ptr<IRenderSurface> surface;
    std::unique_ptr<IWindowRenderPipeline> renderPipeline;

    float dpiScale = 1.0f;
    bool visible = false;
    bool dirty = true;
    bool closeRequested = false;

    FramePolicy framePolicy = FramePolicy::Active;
};

} // namespace Engine
