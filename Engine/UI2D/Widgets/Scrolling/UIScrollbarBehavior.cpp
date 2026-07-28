#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include "Foundation/Math/Matrix3.h"
#include "UI2D/Input/UIEventContext.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#include <algorithm>
#include <cmath>

namespace Engine::UI2D {
namespace {
constexpr float kEpsilon = 1.0e-4f;
float AxisValue(Engine::Vec2F value, UIScrollAxis axis) {
    return axis == UIScrollAxis::Horizontal ? value.x : value.y;
}
Engine::Vec2F RootLocalPosition(
    const UIScene& scene, UINodeHandle root, Engine::Vec2F scenePosition) {
    const UINodeRecord* record = scene.TryGet(root);
    if (!record || !record->layoutState.computedTransform.inverseValid) return scenePosition;
    return Engine::TransformPoint(record->layoutState.computedTransform.sceneToLocal, scenePosition);
}
}

bool UIWidgetRuntime::BindScrollbar(
    UIWidgetHandle scrollbar, UIWidgetHandle scrollView, UIScrollAxis axis) {
    UIWidgetRecord* bar = m_Registry.TryGet(scrollbar);
    UIWidgetRecord* view = m_Registry.TryGet(scrollView);
    if (!bar || bar->kind != UIWidgetKind::Scrollbar || !bar->scrollbar ||
        !view || view->kind != UIWidgetKind::ScrollView || !view->scrollView) return false;

    if (UIWidgetRecord* old = m_Registry.TryGet(bar->scrollbar->boundScrollView)) {
        if (old->scrollView) {
            if (old->scrollView->verticalScrollbar == scrollbar)
                old->scrollView->verticalScrollbar = {};
            if (old->scrollView->horizontalScrollbar == scrollbar)
                old->scrollView->horizontalScrollbar = {};
        }
    }
    UIWidgetHandle& binding = axis == UIScrollAxis::Vertical
        ? view->scrollView->verticalScrollbar : view->scrollView->horizontalScrollbar;
    if (UIWidgetRecord* replaced = m_Registry.TryGet(binding)) {
        if (replaced->scrollbar) replaced->scrollbar->boundScrollView = {};
    }
    binding = scrollbar;
    bar->scrollbar->axis = axis;
    bar->scrollbar->boundScrollView = scrollView;
    bar->scrollbar->syncedModelRevision = 0;
    return SyncScrollbarGeometry(scrollbar);
}

bool UIWidgetRuntime::SyncScrollbarGeometry(UIWidgetHandle scrollbar) {
    UIWidgetRecord* bar = m_Registry.TryGet(scrollbar);
    if (!bar || !bar->scrollbar) return false;
    UIScrollbarState& state = *bar->scrollbar;
    UIWidgetRecord* view = m_Registry.TryGet(state.boundScrollView);
    if (!view || !view->scrollView) return false;
    UINodeRecord* track = m_Runtime->Scene().TryGet(bar->rootNode);
    UINodeRecord* thumb = m_Runtime->Scene().TryGet(state.thumbNode);
    if (!track || !thumb || !track->layoutState.arrangeValid) return false;

    const UIScrollModel& model = view->scrollView->model;
    const float trackLength = state.axis == UIScrollAxis::Horizontal
        ? track->layoutState.arrangedRect.width : track->layoutState.arrangedRect.height;
    const float crossLength = state.axis == UIScrollAxis::Horizontal
        ? track->layoutState.arrangedRect.height : track->layoutState.arrangedRect.width;
    const float viewport = AxisValue(model.ViewportSize(), state.axis);
    const float content = AxisValue(model.ContentSize(), state.axis);
    const float ratio = content > kEpsilon ? std::clamp(viewport / content, 0.0f, 1.0f) : 1.0f;
    const float thumbLength = std::clamp(trackLength * ratio,
        std::min(state.minimumThumbLength, trackLength), trackLength);
    const float travel = std::max(0.0f, trackLength - thumbLength);
    const float normalized = AxisValue(model.NormalizedOffset(), state.axis);
    const float position = travel * normalized;

    bool changed = false;
    UILayoutParams layout = thumb->layout;
    const Engine::Vec2F desired = state.axis == UIScrollAxis::Horizontal
        ? Engine::Vec2F{thumbLength, crossLength} : Engine::Vec2F{crossLength, thumbLength};
    if (layout.sizeRule.width.mode != UISizeMode::Fixed ||
        layout.sizeRule.height.mode != UISizeMode::Fixed ||
        std::abs(layout.sizeRule.width.value - desired.x) > kEpsilon ||
        std::abs(layout.sizeRule.height.value - desired.y) > kEpsilon) {
        layout.sizeRule.width = {UISizeMode::Fixed, desired.x};
        layout.sizeRule.height = {UISizeMode::Fixed, desired.y};
        changed |= m_Runtime->Scene().SetLayout(state.thumbNode, layout);
    }
    const Engine::Vec2F offset = state.axis == UIScrollAxis::Horizontal
        ? Engine::Vec2F{position, 0.0f} : Engine::Vec2F{0.0f, position};
    changed |= m_Runtime->Scene().SetWidgetRuntimeOffset(state.thumbNode, offset);
    state.trackLength = trackLength;
    state.thumbLength = thumbLength;
    state.thumbTravel = travel;
    state.thumbPosition = position;
    state.syncedModelRevision = model.Revision();
    return changed;
}

void UIWidgetRuntime::InstallScrollbarBehavior(UIWidgetHandle widget) {
    UIWidgetRecord* record = m_Registry.TryGet(widget);
    if (!record || !record->scrollbar) return;
    const UINodeHandle root = record->rootNode;
    const UINodeHandle thumb = record->scrollbar->thumbNode;

    (void)Connect(widget, thumb, UIEventType::PointerDown, UIEventPhaseMask::Bubble,
        [this, widget](UIEventContext& context) {
            UIWidgetRecord* bar = m_Registry.TryGet(widget);
            if (!bar || !bar->scrollbar || !IsUserEnabled(*bar) ||
                context.Event().button != Engine::PointerButton::Primary) return;
            UIWidgetRecord* view = m_Registry.TryGet(bar->scrollbar->boundScrollView);
            if (!view || !view->scrollView || !IsUserEnabled(*view)) return;
            UIScrollbarState& state = *bar->scrollbar;
            state.pointerId = context.Event().pointerId;
            state.dragStartScenePosition = RootLocalPosition(
                m_Runtime->Scene(), bar->rootNode, context.Event().scenePosition);
            state.dragStartNormalizedOffset = AxisValue(
                view->scrollView->model.NormalizedOffset(), state.axis);
            state.dragging = true;
            view->scrollView->smooth.active = false;
            view->scrollView->inertia.active = false;
            SetScrollingStyle(*view, true);
            (void)context.RequestPointerCapture(state.pointerId, state.thumbNode);
            context.MarkHandled();
            context.StopPropagation();
        });

    (void)Connect(widget, thumb, UIEventType::PointerMove, UIEventPhaseMask::Bubble,
        [this, widget](UIEventContext& context) {
            UIWidgetRecord* bar = m_Registry.TryGet(widget);
            if (!bar || !bar->scrollbar || !bar->scrollbar->dragging ||
                bar->scrollbar->pointerId != context.Event().pointerId) return;
            UIWidgetRecord* view = m_Registry.TryGet(bar->scrollbar->boundScrollView);
            if (!view || !view->scrollView) return;
            UIScrollbarState& state = *bar->scrollbar;
            const Engine::Vec2F local = RootLocalPosition(
                m_Runtime->Scene(), bar->rootNode, context.Event().scenePosition);
            const float distance = AxisValue(local, state.axis) -
                AxisValue(state.dragStartScenePosition, state.axis);
            const float normalized = std::clamp(state.dragStartNormalizedOffset +
                (state.thumbTravel > kEpsilon ? distance / state.thumbTravel : 0.0f), 0.0f, 1.0f);
            Engine::Vec2F target = view->scrollView->model.Offset();
            const Engine::Vec2F maximum = view->scrollView->model.MaximumOffset();
            if (state.axis == UIScrollAxis::Horizontal) target.x = maximum.x * normalized;
            else target.y = maximum.y * normalized;
            (void)SetScrollOffsetInternal(state.boundScrollView, target, false,
                UIScrollInputMode::Scrollbar, false);
            context.MarkHandled();
            context.StopPropagation();
        });

    auto finish = [this, widget](UIEventContext& context) {
        UIWidgetRecord* bar = m_Registry.TryGet(widget);
        if (!bar || !bar->scrollbar || !bar->scrollbar->dragging ||
            bar->scrollbar->pointerId != context.Event().pointerId) return;
        const UIWidgetHandle viewHandle = bar->scrollbar->boundScrollView;
        context.ReleasePointerCapture(bar->scrollbar->pointerId);
        bar->scrollbar->dragging = false;
        if (UIWidgetRecord* view = m_Registry.TryGet(viewHandle)) {
            if (view->scrollView) {
                view->scrollView->completionPending = true;
                SetScrollingStyle(*view, false);
            }
        }
        context.MarkHandled();
        context.StopPropagation();
    };
    (void)Connect(widget, thumb, UIEventType::PointerUp, UIEventPhaseMask::Bubble,
        [finish](UIEventContext& context) mutable { finish(context); });
    (void)Connect(widget, thumb, UIEventType::PointerCancel, UIEventPhaseMask::Bubble,
        [finish](UIEventContext& context) mutable { finish(context); });

    // A scrollbar consumes pointer gestures so they cannot arm the surrounding ScrollView drag.
    (void)Connect(widget, root, UIEventType::PointerDown, UIEventPhaseMask::Bubble,
        [](UIEventContext& context) { context.StopPropagation(); });
    (void)Connect(widget, root, UIEventType::Click, UIEventPhaseMask::Bubble,
        [this, widget](UIEventContext& context) {
            UIWidgetRecord* bar = m_Registry.TryGet(widget);
            if (!bar || !bar->scrollbar || context.Event().target == bar->scrollbar->thumbNode ||
                !IsUserEnabled(*bar)) return;
            UIWidgetRecord* view = m_Registry.TryGet(bar->scrollbar->boundScrollView);
            if (!view || !view->scrollView || !IsUserEnabled(*view)) return;
            UIScrollbarState& state = *bar->scrollbar;
            const Engine::Vec2F local = RootLocalPosition(
                m_Runtime->Scene(), bar->rootNode, context.Event().scenePosition);
            const float coordinate = AxisValue(local, state.axis);
            float direction = 0.0f;
            if (coordinate < state.thumbPosition) direction = -1.0f;
            else if (coordinate > state.thumbPosition + state.thumbLength) direction = 1.0f;
            if (direction != 0.0f) {
                Engine::Vec2F target = view->scrollView->model.Offset();
                const float page = view->scrollView->model.PageStep(state.axis) * direction;
                if (state.axis == UIScrollAxis::Horizontal) target.x += page;
                else target.y += page;
                (void)SetScrollOffsetInternal(state.boundScrollView, target, false,
                    UIScrollInputMode::Scrollbar, true);
            }
            context.MarkHandled();
            context.StopPropagation();
        });
}

} // namespace Engine::UI2D
