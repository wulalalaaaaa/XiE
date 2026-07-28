#include "Renderer2D/Backends/Software/LayeredWindowRenderPipeline.h"

#include "Renderer2D/Backends/Software/LayeredWindowPresenter.h"

#include <algorithm>
#include <cmath>

namespace Engine {

LayeredWindowRenderPipeline::LayeredWindowRenderPipeline(
    Software2DRenderer& renderer, LayeredWindowPresenter& presenter)
    : m_Renderer(renderer), m_Presenter(presenter) {}

void LayeredWindowRenderPipeline::Resize(int logicalWidth, int logicalHeight, float dpiScale) {
    m_DpiScale = std::max(0.01f, dpiScale);
    m_Renderer.Resize(
        static_cast<int>(std::lround(static_cast<float>(logicalWidth) * m_DpiScale)),
        static_cast<int>(std::lround(static_cast<float>(logicalHeight) * m_DpiScale)));
}

Render2DStats LayeredWindowRenderPipeline::Render(const DrawList2D& drawList, const Render2DView& view) {
    m_DpiScale = std::max(0.01f, view.dpiScale);
    m_Renderer.Render(drawList, m_DpiScale);
    return m_Renderer.GetStats();
}

void LayeredWindowRenderPipeline::Present() { (void)m_Presenter.Present(m_Renderer.GetBitmap()); }

} // namespace Engine
