#pragma once

#include "Renderer2D/DrawList2D.h"
#include "Renderer2D/DrawTypes2D.h"

namespace Engine {

class IWindowRenderPipeline {
public:
    virtual ~IWindowRenderPipeline() = default;
    virtual void Resize(int logicalWidth, int logicalHeight, float dpiScale) = 0;
    virtual Render2DStats Render(const DrawList2D& drawList, const Render2DView& view) = 0;
    virtual void Present() = 0;
};

} // namespace Engine
