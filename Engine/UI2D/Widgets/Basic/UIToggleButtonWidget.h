#pragma once
#include "UI2D/Runtime/UIWindowRuntime.h"
namespace Engine::UI2D {
struct UIToggleButtonWidget {
    static UIWidgetHandle Create(UIWidgetContext& context, const UIToggleButtonWidgetDesc& desc,
        UIWidgetHandle parent = {}) { return context.windowRuntime.Widgets().CreateToggleButton(desc, parent); }
};
} // namespace Engine::UI2D
