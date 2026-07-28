#pragma once
#include "UI2D/Runtime/UIWindowRuntime.h"
namespace Engine::UI2D {
struct UITextWidget {
    static UIWidgetHandle Create(UIWidgetContext& context, const UITextWidgetDesc& desc,
        UIWidgetHandle parent = {}) { return context.windowRuntime.Widgets().CreateText(desc, parent); }
};
} // namespace Engine::UI2D
