#pragma once
#include "UI2D/Runtime/UIWindowRuntime.h"
namespace Engine::UI2D {
struct UIPanelWidget {
    static UIWidgetHandle Create(UIWidgetContext& context, const UIPanelWidgetDesc& desc,
        UIWidgetHandle parent = {}) { return context.windowRuntime.Widgets().CreatePanel(desc, parent); }
};
} // namespace Engine::UI2D
