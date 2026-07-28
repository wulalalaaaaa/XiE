#pragma once

#include "UI2D/Layout/UILayoutContext.h"
#include "UI2D/Layout/UILayoutResult.h"

namespace Engine::UI2D {

class UIScene;

class IUILayoutEngine {
public:
    virtual ~IUILayoutEngine() = default;
    virtual UILayoutResult UpdateLayout(UIScene& scene, const UILayoutContext& context) = 0;
};

// Phase 3A compatibility/test helper only. Production UI composition uses an
// injected BasicLayoutEngine; this type is not a complete layout algorithm.
class PassthroughLayoutEngine final : public IUILayoutEngine {
public:
    UILayoutResult UpdateLayout(UIScene& scene, const UILayoutContext& context) override;
};

} // namespace Engine::UI2D
