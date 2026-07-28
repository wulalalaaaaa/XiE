#include "Renderer2D/Backends/OpenGL/OpenGLWindowRenderPipeline.h"

#include "Platform/IRenderSurface.h"
#include "Renderer/IRenderBackend.h"
#include "Renderer2D/Backends/OpenGL/OpenGL2DRenderer.h"

#include <algorithm>
#include <cmath>

namespace Engine {

OpenGLWindowRenderPipeline::OpenGLWindowRenderPipeline(
    IRenderSurface& surface, IRenderBackend& backend, OpenGL2DRenderer& renderer)
    : m_Surface(surface), m_Backend(backend), m_Renderer(renderer) {}

void OpenGLWindowRenderPipeline::Resize(int logicalWidth, int logicalHeight, float dpiScale) {
    const float scale = std::max(0.01f, dpiScale);
    m_Surface.Resize(
        static_cast<int>(std::lround(static_cast<float>(logicalWidth) * scale)),
        static_cast<int>(std::lround(static_cast<float>(logicalHeight) * scale)));
}

Render2DStats OpenGLWindowRenderPipeline::Render(const DrawList2D& drawList, const Render2DView& view) {
    m_Surface.MakeCurrent();
    m_Renderer.Render(m_Surface, m_Backend, drawList, view);
    return m_Renderer.GetStats();
}

void OpenGLWindowRenderPipeline::Present() { m_Surface.Present(); }

} // namespace Engine
