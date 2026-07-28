#pragma once

namespace Engine::UI2D {

class UIWindowRuntime;
class UIScene;
class UIEventListenerRegistry;
class ResolvedUITheme;

struct UIWidgetContext {
    UIWindowRuntime& windowRuntime;
    UIScene& scene;
    UIEventListenerRegistry& listeners;
    const ResolvedUITheme& theme;
};

} // namespace Engine::UI2D
