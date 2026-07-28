#pragma once
#include "UI2D/Runtime/UIWindowRuntime.h"
namespace Engine::UI2D {
struct UIButtonBehavior {
    static UIWidgetConnectionHandle OnActivated(UIWidgetContext& context, UIWidgetHandle widget,
        UIButtonActivatedCallback callback) {
        return context.windowRuntime.Widgets().OnActivated(widget, std::move(callback));
    }
    static bool Activate(UIWidgetContext& context, UIWidgetHandle widget,
        UIButtonActivatedEvent::Source source = UIButtonActivatedEvent::Source::Programmatic) {
        return context.windowRuntime.Widgets().Activate(widget, source);
    }
};
} // namespace Engine::UI2D
