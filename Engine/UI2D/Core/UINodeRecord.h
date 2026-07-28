#pragma once

#include "UI2D/Core/UIDirtyFlags.h"
#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/Core/UINodeHandle.h"
#include "UI2D/Core/UITransform.h"
#include "UI2D/Core/UIRuntimeProperties.h"
#include "UI2D/Core/UIVisual.h"
#include "UI2D/HitTest/UIHitShape.h"
#include "UI2D/Input/UIFocusTypes.h"
#include "UI2D/Layout/UILayoutState.h"
#include "UI2D/Layout/UILayoutTypes.h"
#include "UI2D/Style/UIStyleTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Engine::UI2D {

struct UINodeRecord {
    UINodeHandle handle{};
    UINodeHandle parent{};
    std::vector<UINodeHandle> children;
    std::string debugName;
    UITransform transform{};
    UIAnimatedOverrides animated{};
    UIWidgetRuntimeProperties widgetRuntime{};
    UINodeStyleState style{};
    UILayoutParams layout{};
    UILayoutState layoutState{};
    UIVisual visual{};
    UIHitShape hitShape{};
    float opacity = 1.0f;
    std::int32_t zOrder = 0;
    std::uint64_t insertionOrder = 0;
    bool visible = true;
    bool enabled = true;
    bool hitTestVisible = true;
    UIFocusProperties focus{};
    bool clipChildren = false;
    UIDirtyFlags dirtyFlags = UIDirtyFlags::All;
};

} // namespace Engine::UI2D
