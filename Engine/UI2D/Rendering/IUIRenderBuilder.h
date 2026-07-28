#pragma once

#include "Renderer2D/DrawList2D.h"
#include "UI2D/Rendering/UIRenderContext.h"

namespace Engine::UI2D {

class UIScene;
struct ResolvedUITheme;

class IUIRenderBuilder {
public:
    virtual ~IUIRenderBuilder() = default;
    virtual UIRenderResult Build(
        const UIScene& scene,
        const ResolvedUITheme& theme,
        Engine::DrawList2D& output,
        const UIRenderContext& context) = 0;
    [[nodiscard]] virtual std::uint64_t ResourceRevision() const { return 0; }
};

// Phase 3A only. It validates scene lifetime and intentionally emits no visual fallback.
class EmptyUIRenderBuilder final : public IUIRenderBuilder {
public:
    UIRenderResult Build(
        const UIScene& scene,
        const ResolvedUITheme& theme,
        Engine::DrawList2D& output,
        const UIRenderContext& context) override;
};

} // namespace Engine::UI2D
