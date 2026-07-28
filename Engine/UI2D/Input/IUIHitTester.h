#pragma once

#include "Foundation/Math/Types2D.h"
#include "UI2D/HitTest/UIHitTestTypes.h"

namespace Engine::UI2D {

class UIScene;

class IUIHitTester {
public:
    virtual ~IUIHitTester() = default;
    virtual UIHitTestResult HitTest(
        const UIScene& scene,
        const UIHitTestContext& context,
        Engine::Vec2F scenePosition) const = 0;
};

} // namespace Engine::UI2D
