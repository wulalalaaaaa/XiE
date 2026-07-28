#pragma once

#include "Platform/WindowHandle.h"
#include "UI2D/Core/UINodeHandle.h"

#include <cstdint>

namespace Engine::UI2D {

class UIEventListenerRegistry;
class UIFocusRequestQueue;
class UIPointerCaptureService;
class UISceneMutationQueue;

struct UIFocusProperties {
    bool focusable = false;
    std::int32_t tabIndex = 0;
    friend bool operator==(const UIFocusProperties&, const UIFocusProperties&) = default;
};

enum class UIFocusReason : std::uint8_t {
    Pointer,
    KeyboardTab,
    Programmatic,
    Restore,
    NodeInvalidated,
    WindowActivated,
    WindowDeactivated,
    FocusRootChanged,
    Clear
};

enum class UIFocusRequestPriority : std::uint8_t {
    DefaultBehavior,
    Explicit,
    Lifecycle
};

struct UIFocusRequest {
    UINodeHandle target{};
    UIFocusReason reason = UIFocusReason::Programmatic;
    UIFocusRequestPriority priority = UIFocusRequestPriority::Explicit;
    bool clearFocus = false;
};

enum class UIFocusRequestStatus : std::uint8_t {
    Applied,
    AlreadyFocused,
    Cleared,
    Deferred,
    RejectedInvalidTarget,
    RejectedNotFocusable,
    RejectedPrevented
};

struct UIFocusRequestResult {
    UIFocusRequestStatus status = UIFocusRequestStatus::Deferred;
    UINodeHandle previous{};
    UINodeHandle current{};
};

enum class UIFocusMoveDirection : std::uint8_t { Forward, Backward };

struct UIFocusMoveContext {
    bool wrap = true;
};

struct UIFocusMoveResult {
    UIFocusRequestResult request{};
    UIFocusMoveDirection direction = UIFocusMoveDirection::Forward;
    bool moved = false;
};

struct UIFocusPolicy {
    bool clearFocusOnBackgroundPointerDown = true;
    bool wrapTabNavigation = true;
    bool restoreFocusOnWindowActivated = true;
};

enum class UIFocusInvalidationReason : std::uint8_t {
    Destroyed,
    Hidden,
    Disabled,
    Reparented,
    FocusRootChanged
};

struct UIFocusInvalidation {
    UINodeHandle node{};
    UIFocusInvalidationReason reason = UIFocusInvalidationReason::Destroyed;
    UINodeHandle lastKnownParent{};
    std::uint64_t lastKnownTraversalOrder = 0;
};

struct UIFocusDispatchContext {
    Engine::WindowHandle window{};
    UIEventListenerRegistry& listeners;
    UISceneMutationQueue& mutationQueue;
    UIPointerCaptureService& captureService;
    UIFocusRequestQueue& requestQueue;
    UIFocusPolicy policy{};
};

struct UIFocusDebugStats {
    std::uint32_t requests = 0;
    std::uint32_t applied = 0;
    std::uint32_t rejected = 0;
    std::uint32_t prevented = 0;
    std::uint32_t tabMoves = 0;
    std::uint32_t restores = 0;
    std::uint32_t invalidationFallbacks = 0;
    std::uint32_t focusLostEvents = 0;
    std::uint32_t focusGainedEvents = 0;
    std::uint32_t candidateRebuilds = 0;
};

[[nodiscard]] constexpr bool IsLifecycleFocusReason(UIFocusReason reason) noexcept {
    return reason == UIFocusReason::NodeInvalidated ||
        reason == UIFocusReason::WindowDeactivated ||
        reason == UIFocusReason::FocusRootChanged;
}

} // namespace Engine::UI2D
