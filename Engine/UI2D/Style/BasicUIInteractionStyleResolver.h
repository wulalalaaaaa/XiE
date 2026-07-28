#pragma once

#include "UI2D/Style/IUIInteractionStyleResolver.h"

namespace Engine::UI2D {

class BasicUIInteractionStyleResolver final : public IUIInteractionStyleResolver {
public:
    UIStyleUpdateResult Update(UIScene& scene, const UIStyleResolveContext& context,
        IUIAnimator& animator) override;
    void OnNodeInvalidated(UINodeHandle) override {}
};

} // namespace Engine::UI2D
