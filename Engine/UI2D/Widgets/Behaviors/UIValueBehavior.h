#pragma once
#include "UI2D/Runtime/UIWindowRuntime.h"
namespace Engine::UI2D {
struct UIValueBehavior {
    static bool SetValue(UIWidgetContext& context, UIWidgetHandle widget, float value) {
        return context.windowRuntime.Widgets().SetValue(widget, value);
    }
};
} // namespace Engine::UI2D
