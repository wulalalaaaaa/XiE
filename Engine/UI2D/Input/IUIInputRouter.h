#pragma once

#include "Input/InputEvents.h"
#include "UI2D/Input/UIEvent.h"

#include <span>

namespace Engine::UI2D {

class UIScene;
class IUIFocusManager;
class IUIHitTester;
class UIEventListenerRegistry;
class UIFocusRequestQueue;
class UISceneMutationQueue;
struct UIInteractionRuntimeState;

struct UIClickPolicy {
    float movementThreshold = 6.0f;
};

struct UIInputDispatchContext {
    Engine::WindowHandle window{};
    IUIHitTester& hitTester;
    IUIFocusManager& focusManager;
    UIEventListenerRegistry& listeners;
    UISceneMutationQueue& mutationQueue;
    UIInteractionRuntimeState& interaction;
    UIClickPolicy clickPolicy{};
    UIFocusRequestQueue& focusRequests;
    UIFocusPolicy focusPolicy{};
};

class IUIInputRouter {
public:
    virtual ~IUIInputRouter() = default;
    virtual UIInputDispatchResult Dispatch(
        UIScene& scene,
        std::span<const Engine::InputEvent> events,
        UIInputDispatchContext& context) = 0;
    virtual void OnNodeInvalidated(UIScene& scene, UINodeHandle node) = 0;
    virtual void OnWindowHidden(UIScene&) {}
    virtual void OnWindowFocusLost(UIScene&) {}
};

} // namespace Engine::UI2D
