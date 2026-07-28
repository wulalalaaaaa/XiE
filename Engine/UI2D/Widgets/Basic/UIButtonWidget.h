#pragma once
#include "UI2D/Runtime/UIWindowRuntime.h"
namespace Engine::UI2D {
struct UIButtonWidget {
    static UIWidgetHandle Create(UIWidgetContext& context, const UIButtonWidgetDesc& desc,
        UIWidgetHandle parent = {}) { return context.windowRuntime.Widgets().CreateButton(desc, parent); }
};
} // namespace Engine::UI2D
