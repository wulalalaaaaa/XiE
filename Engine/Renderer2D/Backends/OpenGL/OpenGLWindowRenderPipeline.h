#pragma once

#include "Renderer2D/IWindowRenderPipeline.h"

namespace Engine {

class IRenderBackend;
class IRenderSurface;
class OpenGL2DRenderer;

// Adapter only: it does not change the OpenGL renderer's execution model.
class OpenGLWindowRenderPipeline final : public IWindowRenderPipeline {
public:
    OpenGLWindowRenderPipeline(IRenderSurface& surface, IRenderBackend& backend, OpenGL2DRenderer& renderer);

    void Resize(int logicalWidth, int logicalHeight, float dpiScale) override;
    Render2DStats Render(const DrawList2D& drawList, const Render2DView& view) override;
    void Present() override;

private:
    IRenderSurface& m_Surface;
    IRenderBackend& m_Backend;
    OpenGL2DRenderer& m_Renderer;
};

} // namespace Engine
