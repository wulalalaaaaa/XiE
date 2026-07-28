#include "UI2D/Runtime/UIWindowRuntime.h"

#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/Input/UIFocusUtils.h"
#include "UI2D/Input/IUIHitTester.h"
#include "UI2D/Input/IUIInputRouter.h"
#include "UI2D/Layout/IUILayoutEngine.h"
#include "UI2D/Rendering/IUIRenderBuilder.h"

#include <algorithm>

namespace Engine::UI2D {

UIWindowRuntime::UIWindowRuntime(
    Engine::WindowHandle window,
    UI2DServices services,
    std::shared_ptr<const ResolvedUITheme> theme)
    : m_Window(window),
      m_Services(services),
      m_Theme(theme ? std::move(theme) : MakeDefaultResolvedUITheme()),
      m_Input(window),
      m_Widgets(*this) {
    m_Scene.SetNodeInvalidatedCallback([this](UINodeHandle node) { OnNodeInvalidated(node); });
    SetFocusRoot(m_Scene.Root());
}

UIWindowRuntime::~UIWindowRuntime() { m_Widgets.Clear(*this); }

bool UIWindowRuntime::EnqueueInput(Engine::InputEvent event) {
    if (!m_Visible && !Engine::IsRetainableWhileHidden(event)) return false;
    return m_Input.Push(std::move(event));
}

UIHitTestResult UIWindowRuntime::HitTest(
    Engine::Vec2F scenePosition, UIHitTestContext context) const {
    if (!m_Visible || m_Scene.HasDirty(UIDirtyFlags::Layout)) {
        UIHitTestResult result;
        result.scenePosition = scenePosition;
        return result;
    }
    return m_Services.hitTester.HitTest(m_Scene, context, scenePosition);
}

UIFocusDispatchContext UIWindowRuntime::MakeFocusContext() {
    return {m_Window, m_Listeners, m_Mutations, m_Interaction.capture,
        m_FocusRequests, m_FocusPolicy};
}

UIFocusRequestResult UIWindowRuntime::FlushFocusRequests() {
    UIFocusDispatchContext context = MakeFocusContext();
    return m_FocusRequests.Flush(m_Scene, m_FocusManager, context);
}

void UIWindowRuntime::RequestFocus(UINodeHandle node, UIFocusReason reason) {
    m_FocusRequests.Enqueue({node, reason, UIFocusRequestPriority::Explicit, false});
}

void UIWindowRuntime::ClearFocus(UIFocusReason reason) {
    m_FocusRequests.Enqueue({{}, reason, UIFocusRequestPriority::Explicit, true});
}

void UIWindowRuntime::SetFocusRoot(UINodeHandle root) {
    UIFocusDispatchContext context = MakeFocusContext();
    m_FocusManager.SetFocusRoot(m_Scene, root, context);
}

UI2DUpdateResult UIWindowRuntime::Update(
    double deltaSeconds, Engine::Vec2F logicalWindowSize, float dpiScale) {
    UI2DUpdateResult result;
    if (!m_Visible) {
        m_Input.DiscardForHiddenWindow();
        return result;
    }

    const float safeDpi = std::max(0.01f, dpiScale);
    if (logicalWindowSize != m_LastLogicalSize || safeDpi != m_LastDpiScale) {
        m_LastLogicalSize = logicalWindowSize;
        m_LastDpiScale = safeDpi;
        m_Scene.MarkAllDirty(UIDirtyFlags::Layout | UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
    }

    auto updateLayout = [&]() {
        // Scrollbar visibility/thumb sizing may invalidate layout once after measuring content.
        // Keep the convergence bound explicit so malformed custom widgets cannot loop forever.
        for (int pass = 0; pass < 3; ++pass) {
            if (m_Scene.HasDirty(UIDirtyFlags::Layout)) {
                UILayoutContext context{logicalWindowSize, safeDpi, *m_Theme, m_Scene.Root()};
                const UILayoutResult layout = m_Services.layoutEngine.UpdateLayout(m_Scene, context);
                result.layoutUpdated = result.layoutUpdated || layout.Succeeded();
                result.layoutFailed = !layout.Succeeded();
                result.layoutError = layout.error;
                if (!layout.Succeeded()) return;
                m_Scene.ClearDirty(UIDirtyFlags::Layout);
                if (layout.layoutChanged || layout.visualInvalidated || layout.hitTestInvalidated) {
                    m_Scene.MarkAllDirty(UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
                }
                SanitizeInteractionState();
                SanitizeFocusState();
            }
            (void)m_Widgets.SyncScrollLayout();
            if (!m_Scene.HasDirty(UIDirtyFlags::Layout)) break;
        }
    };

    // Pointer hit testing must never consume stale layout after resize/DPI/scene changes.
    updateLayout();
    UpdateAnimatedTransforms(m_Scene);
    (void)FlushFocusRequests();

    std::vector<Engine::InputEvent> events = m_Input.Drain();
    result.consumedInputCount = events.size();
    if (!events.empty()) {
        UIInputDispatchContext inputContext{
            m_Window, m_Services.hitTester, m_FocusManager, m_Listeners,
            m_Mutations, m_Interaction, {}, m_FocusRequests, m_FocusPolicy};
        result.inputDispatch = m_Services.inputRouter.Dispatch(m_Scene, events, inputContext);
    }

    auto mergeWidgetResult = [&](const UIWidgetUpdateResult& update) {
        result.widgets.appliedMutationCount += update.appliedMutationCount;
        result.widgets.rejectedMutationCount += update.rejectedMutationCount;
        result.widgets.destroyedWidgetCount += update.destroyedWidgetCount;
        result.widgets.sceneChanged |= update.sceneChanged;
    };
    mergeWidgetResult(m_Widgets.FlushMutations(*this));

    UISceneMutationResult mutations = m_Mutations.Flush(m_Scene);
    result.appliedMutationCount += mutations.appliedCount;
    result.mutationErrorCount += mutations.errors.size();
    SanitizeInteractionState();
    SanitizeFocusState();
    (void)FlushFocusRequests();
    updateLayout();
    (void)FlushFocusRequests();

    (void)m_Widgets.UpdateScrolling(deltaSeconds);

    const UIInteractionSnapshot interaction = Interaction();
    const UIStyleResolveContext styleContext{
        *m_Theme, interaction, m_FocusManager.IsWindowFocused(), m_ForceImmediateStyleResolve};
    result.style = m_StyleResolver.Update(m_Scene, styleContext, m_Animator);
    m_ForceImmediateStyleResolve = false;

    result.animation = m_Animator.Update(m_Scene, deltaSeconds);

    mergeWidgetResult(m_Widgets.FlushMutations(*this));

    mutations = m_Mutations.Flush(m_Scene);
    result.appliedMutationCount += mutations.appliedCount;
    result.mutationErrorCount += mutations.errors.size();
    SanitizeInteractionState();
    SanitizeFocusState();
    (void)FlushFocusRequests();
    updateLayout();
    UpdateAnimatedTransforms(m_Scene);
    (void)FlushFocusRequests();

    constexpr UIDirtyFlags renderDirty = UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::Children;
    const std::uint64_t resourceRevision = m_Services.renderBuilder.ResourceRevision();
    const bool needsBuild = m_Scene.HasDirty(renderDirty) ||
        m_BuiltVisualRevision != m_Scene.VisualRevision() ||
        m_BuiltResourceRevision != resourceRevision;
    if (needsBuild) {
        const UIRenderContext renderContext{logicalWindowSize, safeDpi, m_Scene.Root(), false};
        const UIRenderResult render = m_Services.renderBuilder.Build(
            m_Scene, *m_Theme, m_DrawList, renderContext);
        result.drawListRebuilt = render.success && render.drawListChanged;
        if (render.success) {
            m_Scene.ClearDirty(renderDirty);
            m_BuiltVisualRevision = m_Scene.VisualRevision();
            m_BuiltResourceRevision = resourceRevision;
        }
    }

    result.activity = QueryFrameActivity();
    result.activity.needsRender = result.drawListRebuilt;
    return result;
}

UIFrameActivity UIWindowRuntime::QueryFrameActivity() const {
    if (!m_Visible) return {};
    UIFrameActivity activity;
    activity.hasPendingInput = !m_Input.Empty();
    activity.hasPendingMutation = !m_Mutations.Empty() || !m_FocusRequests.Empty() ||
        m_Widgets.HasPendingMutations();
    activity.needsLayout = m_Scene.HasDirty(UIDirtyFlags::Layout);
    constexpr UIDirtyFlags renderDirty = UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::Children;
    activity.needsBuildDrawList = m_Scene.HasDirty(renderDirty) ||
        m_BuiltVisualRevision != m_Scene.VisualRevision() ||
        m_BuiltResourceRevision != m_Services.renderBuilder.ResourceRevision();
    activity.needsRender = activity.needsBuildDrawList;
    activity.hasActiveInteraction = m_Interaction.HasActiveInteraction() ||
        m_Widgets.HasActiveScrolling() || activity.hasPendingInput;
    activity.hasActiveAnimation = m_Animator.HasActiveAnimations();
    return activity;
}

void UIWindowRuntime::SetVisible(bool visible) {
    if (m_Visible == visible) return;
    m_Visible = visible;
    if (!visible) {
        m_Widgets.OnWindowHidden();
        m_Input.DiscardForHiddenWindow();
        m_Interaction.ClearAll();
        UIFocusDispatchContext context = MakeFocusContext();
        m_FocusManager.OnWindowFocusChanged(m_Scene, false, context);
        m_FocusManager.DiscardRestoreCandidate();
        m_FocusRequests.Clear();
        m_Services.inputRouter.OnWindowHidden(m_Scene);
    } else {
        UIFocusDispatchContext context = MakeFocusContext();
        m_FocusManager.OnWindowFocusChanged(m_Scene, true, context);
        (void)m_Animator.CancelAllForChannel(m_Scene,
            UIAnimationChannel::InteractionStyle, UIAnimationCancelMode::RestoreBase);
        m_Scene.MarkAllDirty(UIDirtyFlags::Layout | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
        m_Scene.MarkAllStyleDirty();
        m_ForceImmediateStyleResolve = true;
    }
}

void UIWindowRuntime::SetTheme(
    std::shared_ptr<const ResolvedUITheme> theme, UIThemeSwitchMode mode) {
    std::shared_ptr<const ResolvedUITheme> next = theme ? std::move(theme) : MakeDefaultResolvedUITheme();
    if (next == m_Theme) return;
    if (mode == UIThemeSwitchMode::Immediate) {
        (void)m_Animator.CancelAllForChannel(m_Scene,
            UIAnimationChannel::InteractionStyle, UIAnimationCancelMode::RestoreBase);
        m_ForceImmediateStyleResolve = true;
    }
    for (UINodeHandle node : m_Scene.LiveNodes()) {
        const UIStyleClassId oldId = m_Scene.GetStyleClass(node);
        if (!oldId.IsValid()) continue;
        const std::string_view className = m_Theme->StyleClassName(oldId);
        if (!className.empty()) (void)m_Scene.SetStyleClass(node, next->FindStyleClass(className));
    }
    m_Theme = std::move(next);
    m_Scene.MarkAllStyleDirty();
    m_Scene.MarkAllDirty(UIDirtyFlags::Layout | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
}

void UIWindowRuntime::OnNodeInvalidated(UINodeHandle node) {
    UIFocusInvalidation invalidation;
    invalidation.node = node;
    invalidation.reason = UIFocusInvalidationReason::Destroyed;
    if (const UINodeRecord* record = m_Scene.TryGet(node)) {
        invalidation.lastKnownParent = record->parent;
        invalidation.lastKnownTraversalOrder = record->insertionOrder;
    }
    UIFocusDispatchContext focusContext = MakeFocusContext();
    m_FocusManager.OnNodeInvalidated(m_Scene, invalidation, focusContext);
    m_Widgets.OnNodeInvalidated(node);
    m_Listeners.RemoveAllForNode(node);
    m_Interaction.OnNodeInvalidated(node);
    m_Animator.OnNodeInvalidated(m_Scene, node);
    m_StyleResolver.OnNodeInvalidated(node);
    m_Services.inputRouter.OnNodeInvalidated(m_Scene, node);
}

void UIWindowRuntime::SanitizeInteractionState() {
    const std::vector<UINodeHandle> hovered = m_Interaction.hover.Sanitize(m_Scene);
    const std::vector<UINodeHandle> captured = m_Interaction.capture.Sanitize(m_Scene);
    const std::vector<UINodeHandle> pressed = m_Interaction.press.Sanitize(m_Scene);
    for (UINodeHandle node : hovered) if (m_Scene.TryGet(node)) m_Scene.MarkDirty(node, UIDirtyFlags::Visual);
    for (UINodeHandle node : captured) if (m_Scene.TryGet(node)) m_Scene.MarkDirty(node, UIDirtyFlags::Visual);
    for (UINodeHandle node : pressed) if (m_Scene.TryGet(node)) m_Scene.MarkDirty(node, UIDirtyFlags::Visual);
    for (UINodeHandle node : hovered) if (m_Scene.TryGet(node)) m_Scene.MarkStyleDirty(node);
    for (UINodeHandle node : captured) if (m_Scene.TryGet(node)) m_Scene.MarkStyleDirty(node);
    for (UINodeHandle node : pressed) if (m_Scene.TryGet(node)) m_Scene.MarkStyleDirty(node);
    if (!hovered.empty() || !captured.empty() || !pressed.empty()) m_Interaction.Touch();
}

void UIWindowRuntime::SanitizeFocusState() {
    const UINodeHandle focused = m_FocusManager.GetFocusedNode();
    const UINodeRecord* focusedNode = m_Scene.TryGet(focused);
    if (!focused.IsValid()) return;
    UIFocusDispatchContext context = MakeFocusContext();
    if (!focusedNode) {
        if (m_FocusRequests.Empty()) {
            m_FocusManager.OnNodeInvalidated(m_Scene,
                {focused, UIFocusInvalidationReason::Destroyed, {}, 0}, context);
        }
        return;
    }
    UINodeHandle focusRoot = m_FocusManager.GetFocusRoot();
    if (!m_Scene.TryGet(focusRoot)) focusRoot = m_Scene.Root();
    if (!IsFocusable(m_Scene, focused, focusRoot)) {
        const UIFocusInvalidationReason reason =
            (!focusedNode->visible || !focusedNode->layoutState.effectiveVisible)
            ? UIFocusInvalidationReason::Hidden : UIFocusInvalidationReason::Disabled;
        m_FocusManager.OnNodeInvalidated(m_Scene,
            {focused, reason, focusedNode->parent, focusedNode->insertionOrder}, context);
        return;
    }
    std::vector<UINodeHandle> currentRoute;
    for (UINodeHandle node = focused; node.IsValid();) {
        currentRoute.push_back(node);
        const UINodeRecord* record = m_Scene.TryGet(node);
        if (!record || node == m_Scene.Root()) break;
        node = record->parent;
    }
    std::reverse(currentRoute.begin(), currentRoute.end());
    const std::span<const UINodeHandle> oldRoute = m_FocusManager.FocusRoute();
    if (!std::equal(currentRoute.begin(), currentRoute.end(), oldRoute.begin(), oldRoute.end())) {
        m_FocusManager.OnNodeInvalidated(m_Scene,
            {focused, UIFocusInvalidationReason::Reparented,
             focusedNode->parent, focusedNode->insertionOrder}, context);
    }
}

} // namespace Engine::UI2D
