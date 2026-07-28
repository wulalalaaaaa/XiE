#pragma once
#include "UI2D/Runtime/UIWindowRuntime.h"
namespace Engine::UI2D {
struct UIToggleBehavior {
    static bool SetChecked(UIWidgetContext& context, UIWidgetHandle widget, bool checked,
        bool emitEvent = true) { return context.windowRuntime.Widgets().SetChecked(widget, checked, emitEvent); }
};
} // namespace Engine::UI2D
