#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/Input/UIEventContext.h"
#include "UI2D/Layout/UIContentExtent.h"
#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Widgets/Scrolling/UIScrollPhysics.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {
namespace {
constexpr float kEpsilon = 1.0e-4f;

bool Near(float a, float b) { return std::abs(a - b) <= kEpsilon; }
bool Near(Engine::Vec2F a, Engine::Vec2F b) { return Near(a.x, b.x) && Near(a.y, b.y); }
float ClampFinite(float value, float minimum, float maximum) {
    return std::clamp(std::isfinite(value) ? value : minimum, minimum, maximum);
}
Engine::Vec2F ClampOffset(Engine::Vec2F value, Engine::Vec2F maximum) {
    return {ClampFinite(value.x, 0.0f, maximum.x), ClampFinite(value.y, 0.0f, maximum.y)};
}
bool HasShift(Engine::InputModifiers modifiers) {
    return (static_cast<std::uint8_t>(modifiers) &
        static_cast<std::uint8_t>(Engine::InputModifiers::Shift)) != 0;
}
float AxisValue(Engine::Vec2F value, UIScrollAxis axis) {
    return axis == UIScrollAxis::Horizontal ? value.x : value.y;
}
void SetAxisValue(Engine::Vec2F& value, UIScrollAxis axis, float component) {
    if (axis == UIScrollAxis::Horizontal) value.x = component;
    else value.y = component;
}
float ResolveAlignedOffset(float current, float viewport, float start, float length,
    UIScrollAlignment alignment) {
    const float end = start + length;
    if (alignment == UIScrollAlignment::Start) return start;
    if (alignment == UIScrollAlignment::Center) return start + length * 0.5f - viewport * 0.5f;
    if (alignment == UIScrollAlignment::End) return end - viewport;
    if (start < current) return start;
    if (end > current + viewport) return end - viewport;
    return current;
}
}

UIWidgetHandle UIWidgetRuntime::ContentHost(UIWidgetHandle scrollView) const {
    const UIWidgetRecord* record = m_Registry.TryGet(scrollView);
    return record && record->kind == UIWidgetKind::ScrollView && record->scrollView
        ? record->scrollView->contentHost : UIWidgetHandle{};
}

bool UIWidgetRuntime::AddContent(UIWidgetHandle scrollView, UIWidgetHandle child) {
    const UIWidgetHandle host = ContentHost(scrollView);
    return host.IsValid() && child != host && Reparent(child, host);
}

bool UIWidgetRuntime::RemoveContent(UIWidgetHandle scrollView, UIWidgetHandle child) {
    const UIWidgetRecord* childRecord = m_Registry.TryGet(child);
    const UIWidgetHandle host = ContentHost(scrollView);
    return childRecord && host.IsValid() && childRecord->parentWidget == host && Reparent(child, {});
}

void UIWidgetRuntime::QueueScrollChanged(
    UIWidgetRecord& record, Engine::Vec2F previous, UIScrollInputMode source) {
    if (!record.scrollView) return;
    if (!record.scrollView->changePending) record.scrollView->eventPreviousOffset = previous;
    record.scrollView->eventSource = source;
    record.scrollView->changePending = true;
}

void UIWidgetRuntime::SetScrollingStyle(UIWidgetRecord& record, bool scrolling) {
    if (!record.scrollView || record.scrollView->scrollingStyleApplied == scrolling) return;
    record.scrollView->scrollingStyleApplied = scrolling;
    UIInteractionStateMask states =
        m_Runtime->Scene().GetExplicitInteractionStates(record.rootNode);
    if (scrolling) states |= UIInteractionState::Scrolling;
    else states &= ~ToMask(UIInteractionState::Scrolling);
    (void)m_Runtime->Scene().SetExplicitInteractionStates(record.rootNode, states);
}

bool UIWidgetRuntime::ApplyScrollOffset(UIWidgetHandle, UIWidgetRecord& record) {
    if (!record.scrollView) return false;
    UIWidgetRecord* host = m_Registry.TryGet(record.scrollView->contentHost);
    if (!host) return false;
    const Engine::Vec2F offset = record.scrollView->model.Offset();
    const bool changed = m_Runtime->Scene().SetWidgetRuntimeOffset(
        host->rootNode, {-offset.x, -offset.y});
    record.scrollView->appliedModelRevision = record.scrollView->model.Revision();
    if (record.scrollView->verticalScrollbar.IsValid())
        (void)SyncScrollbarGeometry(record.scrollView->verticalScrollbar);
    if (record.scrollView->horizontalScrollbar.IsValid())
        (void)SyncScrollbarGeometry(record.scrollView->horizontalScrollbar);
    if (changed) UpdateAnimatedTransforms(m_Runtime->Scene());
    return changed;
}

bool UIWidgetRuntime::SetScrollOffsetInternal(UIWidgetHandle widget, Engine::Vec2F offset,
    bool animated, UIScrollInputMode source, bool completedWhenImmediate) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || record->kind != UIWidgetKind::ScrollView || !record->scrollView ||
        !IsUserEnabled(*record)) return false;
    UIScrollViewState& state = *record->scrollView;
    const Engine::Vec2F target = ClampOffset(offset, state.model.MaximumOffset());
    if (animated) {
        const Engine::Vec2F base = state.smooth.active ? state.smooth.targetOffset : state.model.Offset();
        if (Near(base, target) && state.smooth.active) return false;
        if (Near(state.model.Offset(), target)) {
            state.smooth.active = false;
            state.inertia.active = false;
            return false;
        }
        state.inertia.active = false;
        state.smooth.startOffset = state.model.Offset();
        state.smooth.targetOffset = target;
        state.smooth.elapsedSeconds = 0.0;
        state.smooth.durationSeconds = state.model.Policy().smoothDurationSeconds;
        state.smooth.source = source;
        state.smooth.active = true;
        SetScrollingStyle(*record, true);
        return true;
    }

    state.smooth.active = false;
    state.inertia.active = false;
    const Engine::Vec2F previous = state.model.Offset();
    if (!state.model.SetOffset(target, source)) return false;
    QueueScrollChanged(*record, previous, source);
    state.completionPending |= completedWhenImmediate;
    (void)ApplyScrollOffset(widget, *record);
    return true;
}

bool UIWidgetRuntime::SetScrollOffset(
    UIWidgetHandle scrollView, Engine::Vec2F offset, bool animated) {
    return SetScrollOffsetInternal(scrollView, offset, animated,
        UIScrollInputMode::Programmatic, true);
}

bool UIWidgetRuntime::ScrollBy(
    UIWidgetHandle scrollView, Engine::Vec2F delta, bool animated) {
    UIWidgetRecord* record = m_Registry.TryGet(scrollView);
    if (!record || !record->scrollView) return false;
    const Engine::Vec2F base = animated && record->scrollView->smooth.active
        ? record->scrollView->smooth.targetOffset : record->scrollView->model.Offset();
    return SetScrollOffsetInternal(scrollView, {base.x + delta.x, base.y + delta.y},
        animated, UIScrollInputMode::Programmatic, true);
}

bool UIWidgetRuntime::ScrollToStart(
    UIWidgetHandle scrollView, UIScrollAxis axis, bool animated) {
    Engine::Vec2F target = GetScrollOffset(scrollView);
    SetAxisValue(target, axis, 0.0f);
    return SetScrollOffset(scrollView, target, animated);
}

bool UIWidgetRuntime::ScrollToEnd(
    UIWidgetHandle scrollView, UIScrollAxis axis, bool animated) {
    Engine::Vec2F target = GetScrollOffset(scrollView);
    SetAxisValue(target, axis, AxisValue(GetMaximumScrollOffset(scrollView), axis));
    return SetScrollOffset(scrollView, target, animated);
}

Engine::Vec2F UIWidgetRuntime::GetScrollOffset(UIWidgetHandle scrollView) const {
    const UIWidgetRecord* record = m_Registry.TryGet(scrollView);
    return record && record->scrollView ? record->scrollView->model.Offset() : Engine::Vec2F{};
}

Engine::Vec2F UIWidgetRuntime::GetMaximumScrollOffset(UIWidgetHandle scrollView) const {
    const UIWidgetRecord* record = m_Registry.TryGet(scrollView);
    return record && record->scrollView ? record->scrollView->model.MaximumOffset() : Engine::Vec2F{};
}

UIScrollSnapshot UIWidgetRuntime::GetScrollSnapshot(UIWidgetHandle scrollView) const {
    UIScrollSnapshot result;
    const UIWidgetRecord* record = m_Registry.TryGet(scrollView);
    if (!record || !record->scrollView) return result;
    result.viewportSize = record->scrollView->model.ViewportSize();
    result.contentSize = record->scrollView->model.ContentSize();
    result.offset = record->scrollView->model.Offset();
    result.maximumOffset = record->scrollView->model.MaximumOffset();
    result.scrolling = record->scrollView->scrollingStyleApplied;
    return result;
}

UIWidgetConnectionHandle UIWidgetRuntime::OnScrollChanged(
    UIWidgetHandle scrollView, UIScrollChangedCallback callback) {
    const UIWidgetRecord* record = m_Registry.TryGet(scrollView);
    if (!record || record->kind != UIWidgetKind::ScrollView) return {};
    return m_Registry.OnScrollChanged(scrollView, std::move(callback));
}

UIWidgetConnectionHandle UIWidgetRuntime::OnScrollCompleted(
    UIWidgetHandle scrollView, UIScrollCompletedCallback callback) {
    const UIWidgetRecord* record = m_Registry.TryGet(scrollView);
    if (!record || record->kind != UIWidgetKind::ScrollView) return {};
    return m_Registry.OnScrollCompleted(scrollView, std::move(callback));
}

bool UIWidgetRuntime::IsNodeDescendantOf(UINodeHandle node, UINodeHandle ancestor) const {
    for (UINodeHandle current = node; current.IsValid();) {
        if (current == ancestor) return true;
        const UINodeRecord* record = m_Runtime->Scene().TryGet(current);
        if (!record) return false;
        current = record->parent;
    }
    return false;
}

Engine::RectF UIWidgetRuntime::LayoutRectRelativeTo(
    UINodeHandle node, UINodeHandle ancestor) const {
    Engine::RectF result{};
    const UINodeRecord* leaf = m_Runtime->Scene().TryGet(node);
    if (!leaf || !leaf->layoutState.arrangeValid || !IsNodeDescendantOf(node, ancestor)) return result;
    result.width = leaf->layoutState.arrangedRect.width;
    result.height = leaf->layoutState.arrangedRect.height;
    for (UINodeHandle current = node; current.IsValid() && current != ancestor;) {
        const UINodeRecord* record = m_Runtime->Scene().TryGet(current);
        if (!record) return {};
        result.x += record->layoutState.arrangedRect.x;
        result.y += record->layoutState.arrangedRect.y;
        current = record->parent;
    }
    return result;
}

bool UIWidgetRuntime::ScrollNodeIntoView(UIWidgetHandle scrollView, UINodeHandle node,
    UIScrollAlignment alignment, bool animated) {
    UIWidgetRecord* record = m_Registry.TryGet(scrollView);
    if (!record || !record->scrollView) return false;
    UIWidgetRecord* host = m_Registry.TryGet(record->scrollView->contentHost);
    if (!host || !IsNodeDescendantOf(node, host->rootNode)) return false;
    const Engine::RectF rect = LayoutRectRelativeTo(node, host->rootNode);
    const Engine::Vec2F viewport = record->scrollView->model.ViewportSize();
    Engine::Vec2F target = record->scrollView->model.Offset();
    if (record->scrollView->model.Policy().horizontal != UIScrollMode::Disabled)
        target.x = ResolveAlignedOffset(target.x, viewport.x, rect.x, rect.width, alignment);
    if (record->scrollView->model.Policy().vertical != UIScrollMode::Disabled)
        target.y = ResolveAlignedOffset(target.y, viewport.y, rect.y, rect.height, alignment);
    return SetScrollOffsetInternal(scrollView, target, animated,
        UIScrollInputMode::Programmatic, true);
}

bool UIWidgetRuntime::SyncScrollLayout() {
    bool changed = false;
    std::erase_if(m_ScrollViews, [this](UIWidgetHandle handle) {
        const UIWidgetRecord* record = m_Registry.TryGet(handle);
        return !record || !record->scrollView;
    });
    UIScene& scene = m_Runtime->Scene();
    for (UIWidgetHandle handle : m_ScrollViews) {
        UIWidgetRecord* record = m_Registry.TryGet(handle);
        if (!record || !record->scrollView) continue;
        UIScrollViewState& state = *record->scrollView;
        const UINodeRecord* viewport = scene.TryGet(state.viewportNode);
        UIWidgetRecord* host = m_Registry.TryGet(state.contentHost);
        UINodeRecord* hostNode = host ? scene.TryGet(host->rootNode) : nullptr;
        if (!viewport || !hostNode || !viewport->layoutState.arrangeValid ||
            !hostNode->layoutState.arrangeValid) continue;
        if (state.syncedLayoutRevision == scene.LayoutRevision() &&
            state.appliedModelRevision == state.model.Revision()) continue;

        const Engine::Vec2F previous = state.model.Offset();
        state.model.SetViewportSize({viewport->layoutState.arrangedRect.width,
            viewport->layoutState.arrangedRect.height});
        const UIContentExtent extent = ComputeUIContentExtent(scene, host->rootNode);
        state.model.SetContentSize({
            std::max({hostNode->layoutState.desiredSize.x,
                hostNode->layoutState.arrangedRect.width, extent.size.x}),
            std::max({hostNode->layoutState.desiredSize.y,
                hostNode->layoutState.arrangedRect.height, extent.size.y})});
        if (!Near(previous, state.model.Offset())) {
            QueueScrollChanged(*record, previous, UIScrollInputMode::Programmatic);
            state.completionPending = true;
        }
        changed |= ApplyScrollOffset(handle, *record);

        auto syncVisibility = [&](UIWidgetHandle scrollbar, UIScrollMode mode, UIScrollAxis axis) {
            UIWidgetRecord* bar = m_Registry.TryGet(scrollbar);
            if (!bar) return;
            const bool visible = mode == UIScrollMode::Always ||
                (mode == UIScrollMode::Auto && state.model.CanScroll(axis));
            if (bar->visible == visible) return;
            bar->visible = visible;
            changed |= scene.SetVisibility(bar->rootNode, visible);
        };
        syncVisibility(state.verticalScrollbar, state.model.Policy().vertical, UIScrollAxis::Vertical);
        syncVisibility(state.horizontalScrollbar, state.model.Policy().horizontal, UIScrollAxis::Horizontal);
        state.syncedLayoutRevision = scene.LayoutRevision();
    }
    return changed;
}

void UIWidgetRuntime::ClaimScrollDrag(UIWidgetHandle owner, Engine::PointerId pointerId) {
    for (UIWidgetHandle handle : m_ScrollViews) {
        if (handle == owner) continue;
        UIWidgetRecord* record = m_Registry.TryGet(handle);
        if (record && record->scrollView && record->scrollView->drag.potential &&
            record->scrollView->drag.pointerId == pointerId && !record->scrollView->drag.dragging)
            record->scrollView->drag = {};
    }
}

void UIWidgetRuntime::AddVelocitySample(
    UIScrollViewState& state, Engine::Vec2F delta, double dt) {
    if (!std::isfinite(dt) || dt <= 0.0) return;
    state.velocitySamples[state.velocitySampleCursor] = {delta, dt};
    state.velocitySampleCursor = (state.velocitySampleCursor + 1) % state.velocitySamples.size();
    state.velocitySampleCount = std::min(state.velocitySampleCount + 1, state.velocitySamples.size());
}

Engine::Vec2F UIWidgetRuntime::ResolveDragVelocity(const UIScrollViewState& state) const {
    Engine::Vec2F delta{};
    double seconds = 0.0;
    for (std::size_t i = 0; i < state.velocitySampleCount; ++i) {
        delta.x += state.velocitySamples[i].delta.x;
        delta.y += state.velocitySamples[i].delta.y;
        seconds += state.velocitySamples[i].deltaSeconds;
    }
    if (seconds <= 0.0) return {};
    const float maximum = state.model.Policy().maximumVelocity;
    return {std::clamp(static_cast<float>(delta.x / seconds), -maximum, maximum),
        std::clamp(static_cast<float>(delta.y / seconds), -maximum, maximum)};
}

void UIWidgetRuntime::InstallScrollViewBehavior(UIWidgetHandle widget) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || !record->scrollView) return;
    const UINodeHandle root = record->rootNode;

    (void)Connect(widget, root, UIEventType::PointerWheel, UIEventPhaseMask::Bubble,
        [this, widget](UIEventContext& context) {
            UIWidgetRecord* current = m_Registry.TryGet(widget);
            if (!current || !current->scrollView || !IsUserEnabled(*current)) return;
            UIScrollViewState& state = *current->scrollView;
            const UIScrollPolicy& policy = state.model.Policy();
            Engine::Vec2F raw = context.Event().wheelDelta;
            Engine::Vec2F target = state.smooth.active ? state.smooth.targetOffset : state.model.Offset();
            const Engine::Vec2F beforeTarget = target;

            auto consume = [&](UIScrollAxis axis, float rawDelta) {
                if (std::abs(rawDelta) <= kEpsilon || !state.model.CanScroll(axis)) return 0.0f;
                const float oldValue = AxisValue(target, axis);
                const float maximum = AxisValue(state.model.MaximumOffset(), axis);
                const float next = std::clamp(oldValue - rawDelta * policy.wheelStep, 0.0f, maximum);
                SetAxisValue(target, axis, next);
                return -(next - oldValue) / policy.wheelStep;
            };

            const bool horizontalFromY = HasShift(context.Event().modifiers) ||
                (!state.model.CanScroll(UIScrollAxis::Vertical) &&
                    state.model.CanScroll(UIScrollAxis::Horizontal));
            if (horizontalFromY) raw.y -= consume(UIScrollAxis::Horizontal, raw.y);
            else raw.y -= consume(UIScrollAxis::Vertical, raw.y);
            raw.x -= consume(UIScrollAxis::Horizontal, raw.x);
            context.Event().wheelDelta = raw;
            if (Near(beforeTarget, target)) return;
            state.inertia.active = false;
            (void)SetScrollOffsetInternal(widget, target, policy.smoothWheel,
                UIScrollInputMode::Wheel, !policy.smoothWheel);
            context.MarkHandled();
        });

    (void)Connect(widget, root, UIEventType::PointerDown, UIEventPhaseMask::Bubble,
        [this, widget](UIEventContext& context) {
            UIWidgetRecord* current = m_Registry.TryGet(widget);
            if (!current || !current->scrollView || !IsUserEnabled(*current) ||
                !current->scrollView->model.Policy().enablePointerDrag ||
                context.Event().button != Engine::PointerButton::Primary) return;
            UIScrollViewState& state = *current->scrollView;
            if (!state.model.CanScroll(UIScrollAxis::Horizontal) &&
                !state.model.CanScroll(UIScrollAxis::Vertical)) return;
            state.drag = {context.Event().pointerId, context.Event().button,
                context.Event().scenePosition, context.Event().scenePosition,
                state.model.Offset(), context.Event().timestampSeconds, false, false, true};
            state.velocitySampleCount = 0;
            state.velocitySampleCursor = 0;
            state.inertia.active = false;
            state.smooth.active = false;
        });

    (void)Connect(widget, root, UIEventType::PointerMove, UIEventPhaseMask::Bubble,
        [this, widget](UIEventContext& context) {
            UIWidgetRecord* current = m_Registry.TryGet(widget);
            if (!current || !current->scrollView) return;
            UIScrollViewState& state = *current->scrollView;
            if (!state.drag.potential || state.drag.pointerId != context.Event().pointerId) return;
            const Engine::Vec2F position = context.Event().scenePosition;
            if (!state.drag.dragging) {
                const float dx = position.x - state.drag.downScenePosition.x;
                const float dy = position.y - state.drag.downScenePosition.y;
                if (std::sqrt(dx * dx + dy * dy) < state.model.Policy().dragThreshold) return;
                const UIScrollAxis dominant = std::abs(dx) > std::abs(dy)
                    ? UIScrollAxis::Horizontal : UIScrollAxis::Vertical;
                if (!state.model.CanScroll(dominant)) {
                    // Let an ancestor pending ScrollView claim a direction this view cannot scroll.
                    state.drag = {};
                    return;
                }
                ClaimScrollDrag(widget, state.drag.pointerId);
                state.drag.thresholdPassed = true;
                state.drag.dragging = true;
                (void)context.RequestPointerCapture(state.drag.pointerId, current->rootNode);
                (void)context.CancelClick(state.drag.pointerId, state.drag.button);
                SetScrollingStyle(*current, true);
            }
            Engine::Vec2F delta{state.drag.lastScenePosition.x - position.x,
                state.drag.lastScenePosition.y - position.y};
            if (!state.model.CanScroll(UIScrollAxis::Horizontal)) delta.x = 0.0f;
            if (!state.model.CanScroll(UIScrollAxis::Vertical)) delta.y = 0.0f;
            const Engine::Vec2F previous = state.model.Offset();
            if (state.model.ScrollBy(delta, UIScrollInputMode::PointerDrag)) {
                QueueScrollChanged(*current, previous, UIScrollInputMode::PointerDrag);
                const Engine::Vec2F actual{state.model.Offset().x - previous.x,
                    state.model.Offset().y - previous.y};
                AddVelocitySample(state, actual,
                    context.Event().timestampSeconds - state.drag.lastTimestampSeconds);
                (void)ApplyScrollOffset(widget, *current);
            }
            state.drag.lastScenePosition = position;
            state.drag.lastTimestampSeconds = context.Event().timestampSeconds;
            context.MarkHandled();
            context.StopPropagation();
        });

    auto finishDrag = [this, widget](UIEventContext& context, bool canceled) {
        UIWidgetRecord* current = m_Registry.TryGet(widget);
        if (!current || !current->scrollView) return;
        UIScrollViewState& state = *current->scrollView;
        if (!state.drag.potential || state.drag.pointerId != context.Event().pointerId) return;
        const bool wasDragging = state.drag.dragging;
        if (wasDragging) {
            context.ReleasePointerCapture(state.drag.pointerId);
            const Engine::Vec2F velocity = ResolveDragVelocity(state);
            const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
            if (!canceled && state.model.Policy().enableInertia &&
                speed >= state.model.Policy().minimumInertiaVelocity) {
                state.inertia.velocity = velocity;
                state.inertia.active = true;
            } else {
                state.inertia = {};
                state.completionPending = true;
            }
            context.MarkHandled();
            context.StopPropagation();
        }
        state.drag = {};
        state.velocitySampleCount = 0;
        if (!state.inertia.active) SetScrollingStyle(*current, false);
    };
    (void)Connect(widget, root, UIEventType::PointerUp, UIEventPhaseMask::Bubble,
        [finishDrag](UIEventContext& context) mutable { finishDrag(context, false); });
    (void)Connect(widget, root, UIEventType::PointerCancel, UIEventPhaseMask::Bubble,
        [finishDrag](UIEventContext& context) mutable { finishDrag(context, true); });

    (void)Connect(widget, root, UIEventType::FocusGained, UIEventPhaseMask::Bubble,
        [this, widget](UIEventContext& context) {
            UIWidgetRecord* current = m_Registry.TryGet(widget);
            if (current && current->scrollView && current->scrollView->bringFocusedNodeIntoView)
                (void)ScrollNodeIntoView(widget, context.Event().target,
                    UIScrollAlignment::Nearest, true);
        });
}

bool UIWidgetRuntime::UpdateScrolling(double deltaSeconds) {
    bool changed = false;
    std::erase_if(m_ScrollViews, [this](UIWidgetHandle handle) {
        const UIWidgetRecord* record = m_Registry.TryGet(handle);
        return !record || !record->scrollView;
    });
    for (UIWidgetHandle handle : m_ScrollViews) {
        UIWidgetRecord* record = m_Registry.TryGet(handle);
        if (!record || !record->scrollView) continue;
        UIScrollViewState& state = *record->scrollView;
        const Engine::Vec2F previous = state.model.Offset();
        UIScrollPhysicsResult physics;
        if (state.smooth.active)
            physics = UIScrollPhysics::AdvanceSmooth(state.model, state.smooth, deltaSeconds);
        else if (state.inertia.active)
            physics = UIScrollPhysics::AdvanceInertia(
                state.model, state.inertia, state.model.Policy(), deltaSeconds);
        if (physics.offsetChanged) {
            QueueScrollChanged(*record, previous, state.model.LastSource());
            changed |= ApplyScrollOffset(handle, *record);
        }
        if (physics.completed) state.completionPending = true;
        const bool active = state.drag.dragging || state.smooth.active || state.inertia.active;
        SetScrollingStyle(*record, active);
        if (state.appliedModelRevision != state.model.Revision())
            changed |= ApplyScrollOffset(handle, *record);

        if (state.changePending) {
            const UIScrollChangedEvent event{handle, state.eventPreviousOffset,
                state.model.Offset(), state.model.MaximumOffset(), state.eventSource};
            state.changePending = false;
            m_Registry.DispatchScrollChanged(event);
            changed = true;
        }
        record = m_Registry.TryGet(handle);
        if (record && record->scrollView && record->scrollView->completionPending) {
            const UIScrollCompletedEvent event{handle, record->scrollView->model.Offset()};
            record->scrollView->completionPending = false;
            m_Registry.DispatchScrollCompleted(event);
            changed = true;
        }
    }
    return changed;
}

void UIWidgetRuntime::OnWindowHidden() {
    for (UIWidgetHandle handle : m_ScrollViews) {
        UIWidgetRecord* record = m_Registry.TryGet(handle);
        if (!record || !record->scrollView) continue;
        record->scrollView->drag = {};
        record->scrollView->velocitySampleCount = 0;
        SetScrollingStyle(*record, false);
    }
    for (UIWidgetHandle handle : m_Scrollbars) {
        UIWidgetRecord* record = m_Registry.TryGet(handle);
        if (record && record->scrollbar) record->scrollbar->dragging = false;
    }
}

bool UIWidgetRuntime::HasActiveScrolling() const {
    for (UIWidgetHandle handle : m_ScrollViews) {
        const UIWidgetRecord* record = m_Registry.TryGet(handle);
        if (!record || !record->scrollView) continue;
        const UIScrollViewState& state = *record->scrollView;
        if (state.drag.dragging || state.smooth.active || state.inertia.active ||
            state.changePending || state.completionPending) return true;
    }
    for (UIWidgetHandle handle : m_Scrollbars) {
        const UIWidgetRecord* record = m_Registry.TryGet(handle);
        if (record && record->scrollbar && record->scrollbar->dragging) return true;
    }
    return false;
}

} // namespace Engine::UI2D
