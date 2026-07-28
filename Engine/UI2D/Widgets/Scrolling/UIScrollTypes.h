#pragma once

#include "Foundation/Math/Types2D.h"
#include "Input/InputEvents.h"
#include "UI2D/Widgets/Core/UIWidgetHandle.h"

#include <cstdint>
#include <functional>

namespace Engine::UI2D {

enum class UIScrollAxis { Horizontal, Vertical };
enum class UIScrollMode { Disabled, Auto, Always };
enum class UIScrollInputMode { Wheel, PointerDrag, Scrollbar, Programmatic };
enum class UIScrollbarPlacement { Overlay, ReserveSpace };
enum class UIScrollAlignment { Nearest, Start, Center, End };

using UIScrollOffset = Engine::Vec2F;

struct UIScrollRange {
    float maximumX = 0.0f;
    float maximumY = 0.0f;
    friend constexpr bool operator==(UIScrollRange, UIScrollRange) = default;
};

struct UIScrollPolicy {
    UIScrollMode horizontal = UIScrollMode::Disabled;
    UIScrollMode vertical = UIScrollMode::Auto;
    bool smoothWheel = true;
    bool enablePointerDrag = true;
    bool enableInertia = true;
    float wheelStep = 48.0f;
    float pageStepRatio = 0.9f;
    float dragThreshold = 4.0f;
    float inertiaDeceleration = 2600.0f;
    float minimumInertiaVelocity = 20.0f;
    float maximumVelocity = 6000.0f;
    double smoothDurationSeconds = 0.16;
    friend constexpr bool operator==(const UIScrollPolicy&, const UIScrollPolicy&) = default;
};

struct UIScrollConsumption {
    Engine::Vec2F requestedDelta{};
    Engine::Vec2F consumedDelta{};
    Engine::Vec2F remainingDelta{};
};

struct UIScrollChangedEvent {
    UIWidgetHandle scrollView{};
    Engine::Vec2F previousOffset{};
    Engine::Vec2F currentOffset{};
    Engine::Vec2F maximumOffset{};
    UIScrollInputMode source = UIScrollInputMode::Programmatic;
};

struct UIScrollCompletedEvent {
    UIWidgetHandle scrollView{};
    Engine::Vec2F finalOffset{};
};
using UIScrollChangedCallback = std::function<void(const UIScrollChangedEvent&)>;
using UIScrollCompletedCallback = std::function<void(const UIScrollCompletedEvent&)>;

struct UIScrollSnapshot {
    Engine::Vec2F viewportSize{};
    Engine::Vec2F contentSize{};
    Engine::Vec2F offset{};
    Engine::Vec2F maximumOffset{};
    bool scrolling = false;
};

struct UIScrollDragState {
    Engine::PointerId pointerId = 0;
    Engine::PointerButton button = Engine::PointerButton::None;
    Engine::Vec2F downScenePosition{};
    Engine::Vec2F lastScenePosition{};
    Engine::Vec2F initialOffset{};
    double lastTimestampSeconds = 0.0;
    bool thresholdPassed = false;
    bool dragging = false;
    bool potential = false;
};

struct UIScrollVelocitySample {
    Engine::Vec2F delta{};
    double deltaSeconds = 0.0;
};

struct UIScrollInertiaState {
    Engine::Vec2F velocity{};
    bool active = false;
};

struct UISmoothScrollState {
    Engine::Vec2F targetOffset{};
    Engine::Vec2F startOffset{};
    double elapsedSeconds = 0.0;
    double durationSeconds = 0.16;
    UIScrollInputMode source = UIScrollInputMode::Programmatic;
    bool active = false;
};

} // namespace Engine::UI2D
