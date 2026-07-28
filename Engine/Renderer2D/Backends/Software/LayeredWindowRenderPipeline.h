#pragma once

#include "Renderer2D/IWindowRenderPipeline.h"

namespace Engine {

class LayeredWindowPresenter;
class Software2DRenderer;

// Adapter only: rendering remains BGRA8 premultiplied software composition.
class LayeredWindowRenderPipeline final : public IWindowRenderPipeline {
public:
    LayeredWindowRenderPipeline(Software2DRenderer& renderer, LayeredWindowPresenter& presenter);

    void Resize(int logicalWidth, int logicalHeight, float dpiScale) override;
    Render2DStats Render(const DrawList2D& drawList, const Render2DView& view) override;
    void Present() override;

private:
    Software2DRenderer& m_Renderer;
    LayeredWindowPresenter& m_Presenter;
    float m_DpiScale = 1.0f;
};

} // namespace Engine
