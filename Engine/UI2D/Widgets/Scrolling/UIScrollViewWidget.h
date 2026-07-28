#pragma once

#include "UI2D/Widgets/Core/UIWidgetContext.h"
#include "UI2D/Widgets/Core/UIWidgetTypes.h"
#include "UI2D/Widgets/Scrolling/UIScrollTypes.h"

namespace Engine::UI2D {

struct UIScrollViewWidgetDesc {
    UIWidgetCommonProperties common;
    UIScrollPolicy policy{};
    UILayoutParams contentLayout{};
    UIStyleClassId viewportStyle{};
    UIStyleClassId verticalScrollbarTrackStyle{};
    UIStyleClassId verticalScrollbarThumbStyle{};
    UIStyleClassId horizontalScrollbarTrackStyle{};
    UIStyleClassId horizontalScrollbarThumbStyle{};
    Engine::Color4f backgroundColor{0,0,0,0};
    Engine::Color4f scrollbarTrackColor{0.16f,0.18f,0.22f,0.75f};
    Engine::Color4f scrollbarThumbColor{0.48f,0.54f,0.66f,0.9f};
    UIScrollbarPlacement scrollbarPlacement = UIScrollbarPlacement::Overlay;
    float scrollbarThickness = 12.0f;
    float minimumThumbLength = 18.0f;
    bool createVerticalScrollbar = true;
    bool createHorizontalScrollbar = true;
    bool bringFocusedNodeIntoView = true;
};

struct UIScrollViewWidget {
    static UIWidgetHandle Create(UIWidgetContext& context, const UIScrollViewWidgetDesc& desc,
        UIWidgetHandle parent = {});
};

} // namespace Engine::UI2D
