#pragma once

#include "UI2D/Widgets/Core/UIWidgetContext.h"
#include "UI2D/Widgets/Core/UIWidgetTypes.h"
#include "UI2D/Widgets/Scrolling/UIScrollTypes.h"

namespace Engine::UI2D {

struct UIScrollbarWidgetDesc {
    UIWidgetCommonProperties common;
    UIScrollAxis axis = UIScrollAxis::Vertical;
    float minimumThumbLength = 18.0f;
    UIStyleClassId trackStyle{};
    UIStyleClassId thumbStyle{};
    Engine::Color4f trackColor{0.16f,0.18f,0.22f,0.75f};
    Engine::Color4f thumbColor{0.48f,0.54f,0.66f,0.9f};
};

struct UIScrollbarWidget {
    static UIWidgetHandle Create(UIWidgetContext& context, const UIScrollbarWidgetDesc& desc,
        UIWidgetHandle parent = {});
};

} // namespace Engine::UI2D
