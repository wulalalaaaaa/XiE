#pragma once
#include "UI2D/Runtime/UIWindowRuntime.h"
namespace Engine::UI2D {
struct UIProgressBarWidget {
    static UIWidgetHandle Create(UIWidgetContext& context, const UIProgressBarWidgetDesc& desc,
        UIWidgetHandle parent = {}) { return context.windowRuntime.Widgets().CreateProgressBar(desc, parent); }
};
} // namespace Engine::UI2D
