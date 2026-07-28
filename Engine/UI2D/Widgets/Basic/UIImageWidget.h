#pragma once
#include "UI2D/Runtime/UIWindowRuntime.h"
namespace Engine::UI2D {
struct UIImageWidget {
    static UIWidgetHandle Create(UIWidgetContext& context, const UIImageWidgetDesc& desc,
        UIWidgetHandle parent = {}) { return context.windowRuntime.Widgets().CreateImage(desc, parent); }
};
} // namespace Engine::UI2D
