#pragma once

#include "UI2D/Input/UIFocusTypes.h"

#include <span>

namespace Engine::UI2D {

class UIScene;

class IUIFocusManager {
public:
    virtual ~IUIFocusManager() = default;
    [[nodiscard]] virtual UINodeHandle GetFocusedNode() const = 0;
    [[nodiscard]] virtual bool HasActiveFocus() const = 0;
    [[nodiscard]] virtual bool IsFocused(UINodeHandle node) const = 0;
    [[nodiscard]] virtual bool HasFocusWithin(UINodeHandle node) const = 0;
    [[nodiscard]] virtual std::span<const UINodeHandle> FocusRoute() const = 0;
    [[nodiscard]] virtual UINodeHandle GetFocusRoot() const = 0;
    [[nodiscard]] virtual bool IsWindowFocused() const = 0;

    virtual UIFocusRequestResult RequestFocus(
        UIScene& scene, const UIFocusRequest& request, UIFocusDispatchContext& context) = 0;
    virtual UIFocusRequestResult ClearFocus(
        UIScene& scene, UIFocusReason reason, UIFocusDispatchContext& context) = 0;
    virtual UIFocusMoveResult MoveFocus(
        UIScene& scene, UIFocusMoveDirection direction,
        const UIFocusMoveContext& moveContext, UIFocusDispatchContext& dispatchContext) = 0;
    virtual void SetFocusRoot(
        UIScene& scene, UINodeHandle root, UIFocusDispatchContext& context) = 0;
    virtual void OnWindowFocusChanged(
        UIScene& scene, bool focused, UIFocusDispatchContext& context) = 0;
    virtual void OnNodeInvalidated(
        UIScene& scene, const UIFocusInvalidation& invalidation,
        UIFocusDispatchContext& context) = 0;
};

} // namespace Engine::UI2D
