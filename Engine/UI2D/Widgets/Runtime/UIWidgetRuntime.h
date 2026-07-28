#pragma once

#include "UI2D/Widgets/Core/UIWidgetContext.h"
#include "UI2D/Widgets/Core/UIWidgetMutation.h"
#include "UI2D/Widgets/Core/UIWidgetRegistry.h"
#include "UI2D/Widgets/Scrolling/UIScrollViewWidget.h"
#include "UI2D/Widgets/Scrolling/UIScrollbarWidget.h"

namespace Engine::UI2D {

class UIWindowRuntime;

class UIWidgetRuntime {
public:
    explicit UIWidgetRuntime(UIWindowRuntime& runtime);

    UIWidgetRegistry& Registry() noexcept { return m_Registry; }
    const UIWidgetRegistry& Registry() const noexcept { return m_Registry; }
    UIWidgetContext Context();
    UIWidgetUpdateResult FlushMutations(UIWindowRuntime& runtime);
    void OnNodeInvalidated(UINodeHandle node);
    void Clear(UIWindowRuntime& runtime);
    [[nodiscard]] bool HasPendingMutations() const noexcept { return !m_Mutations.Empty(); }

    UIWidgetHandle CreatePanel(const UIPanelWidgetDesc& desc, UIWidgetHandle parent = {});
    UIWidgetHandle CreateImage(const UIImageWidgetDesc& desc, UIWidgetHandle parent = {});
    UIWidgetHandle CreateText(const UITextWidgetDesc& desc, UIWidgetHandle parent = {});
    UIWidgetHandle CreateButton(const UIButtonWidgetDesc& desc, UIWidgetHandle parent = {});
    UIWidgetHandle CreateToggleButton(const UIToggleButtonWidgetDesc& desc, UIWidgetHandle parent = {});
    UIWidgetHandle CreateProgressBar(const UIProgressBarWidgetDesc& desc, UIWidgetHandle parent = {});
    UIWidgetHandle CreateScrollView(const UIScrollViewWidgetDesc& desc, UIWidgetHandle parent = {});
    UIWidgetHandle CreateScrollbar(const UIScrollbarWidgetDesc& desc, UIWidgetHandle parent = {});

    bool Destroy(UIWidgetHandle widget);
    bool Reparent(UIWidgetHandle child, UIWidgetHandle parent);
    bool SetVisible(UIWidgetHandle widget, bool visible);
    bool SetEnabled(UIWidgetHandle widget, bool enabled);
    bool SetFocusable(UIWidgetHandle widget, bool focusable);
    bool SetTabIndex(UIWidgetHandle widget, std::int32_t tabIndex);
    bool SetLayout(UIWidgetHandle widget, const UILayoutParams& layout);
    bool SetTransform(UIWidgetHandle widget, const UITransform& transform);
    bool SetStyleClass(UIWidgetHandle widget, UIStyleClassId styleClass);

    bool SetTexture(UIWidgetHandle widget, Engine::TextureHandle texture);
    bool SetImageFit(UIWidgetHandle widget, UIImageFit fit);
    bool SetTint(UIWidgetHandle widget, Engine::Color4f tint);
    bool SetTextLayout(UIWidgetHandle widget, Engine::TextLayoutHandle layout);
    bool SetTextColor(UIWidgetHandle widget, Engine::Color4f color);

    UIWidgetConnectionHandle Connect(UIWidgetHandle widget, UINodeHandle node,
        UIEventType eventType, UIEventPhaseMask phases, UIEventCallback callback);
    bool Disconnect(UIWidgetConnectionHandle connection) { return m_Registry.Disconnect(connection); }
    UIWidgetConnectionHandle OnActivated(UIWidgetHandle widget, UIButtonActivatedCallback callback);
    bool Activate(UIWidgetHandle widget,
        UIButtonActivatedEvent::Source source = UIButtonActivatedEvent::Source::Programmatic);
    [[nodiscard]] bool IsChecked(UIWidgetHandle widget) const;
    bool SetChecked(UIWidgetHandle widget, bool checked, bool emitEvent = true);
    UIWidgetConnectionHandle OnCheckedChanged(UIWidgetHandle widget, UIToggleCheckedChangedCallback callback);

    [[nodiscard]] float Value(UIWidgetHandle widget) const;
    bool SetValue(UIWidgetHandle widget, float value);
    bool SetRange(UIWidgetHandle widget, float minimum, float maximum);
    bool SetProgressDirection(UIWidgetHandle widget, UIProgressDirection direction);

    [[nodiscard]] UIWidgetHandle ContentHost(UIWidgetHandle scrollView) const;
    bool AddContent(UIWidgetHandle scrollView, UIWidgetHandle child);
    bool RemoveContent(UIWidgetHandle scrollView, UIWidgetHandle child);
    bool SetScrollOffset(UIWidgetHandle scrollView, Engine::Vec2F offset, bool animated);
    bool ScrollBy(UIWidgetHandle scrollView, Engine::Vec2F delta, bool animated);
    bool ScrollToStart(UIWidgetHandle scrollView, UIScrollAxis axis, bool animated);
    bool ScrollToEnd(UIWidgetHandle scrollView, UIScrollAxis axis, bool animated);
    bool ScrollNodeIntoView(UIWidgetHandle scrollView, UINodeHandle node,
        UIScrollAlignment alignment, bool animated);
    [[nodiscard]] Engine::Vec2F GetScrollOffset(UIWidgetHandle scrollView) const;
    [[nodiscard]] Engine::Vec2F GetMaximumScrollOffset(UIWidgetHandle scrollView) const;
    [[nodiscard]] UIScrollSnapshot GetScrollSnapshot(UIWidgetHandle scrollView) const;
    bool BindScrollbar(UIWidgetHandle scrollbar, UIWidgetHandle scrollView, UIScrollAxis axis);
    UIWidgetConnectionHandle OnScrollChanged(UIWidgetHandle scrollView, UIScrollChangedCallback callback);
    UIWidgetConnectionHandle OnScrollCompleted(UIWidgetHandle scrollView, UIScrollCompletedCallback callback);

    bool SyncScrollLayout();
    bool UpdateScrolling(double deltaSeconds);
    void OnWindowHidden();
    [[nodiscard]] bool HasActiveScrolling() const;

private:
    UINodeHandle ParentNode(UIWidgetHandle parent) const;
    UIWidgetHandle RegisterOrRollback(UIWidgetRecord record);
    bool ApplyCommon(UINodeHandle node, const UIWidgetCommonProperties& common);
    void InstallButtonBehavior(UIWidgetHandle widget);
    bool IsUserEnabled(const UIWidgetRecord& record) const;
    bool UpdateProgressGeometry(UIWidgetRecord& record);
    void InstallScrollViewBehavior(UIWidgetHandle widget);
    void InstallScrollbarBehavior(UIWidgetHandle widget);
    bool SetScrollOffsetInternal(UIWidgetHandle widget, Engine::Vec2F offset,
        bool animated, UIScrollInputMode source, bool completedWhenImmediate);
    bool ApplyScrollOffset(UIWidgetHandle widget, UIWidgetRecord& record);
    bool SyncScrollbarGeometry(UIWidgetHandle scrollbar);
    void SetScrollingStyle(UIWidgetRecord& record, bool scrolling);
    void QueueScrollChanged(UIWidgetRecord& record, Engine::Vec2F previous, UIScrollInputMode source);
    void ClaimScrollDrag(UIWidgetHandle owner, Engine::PointerId pointerId);
    void AddVelocitySample(UIScrollViewState& state, Engine::Vec2F delta, double dt);
    Engine::Vec2F ResolveDragVelocity(const UIScrollViewState& state) const;
    bool IsNodeDescendantOf(UINodeHandle node, UINodeHandle ancestor) const;
    Engine::RectF LayoutRectRelativeTo(UINodeHandle node, UINodeHandle ancestor) const;

    UIWindowRuntime* m_Runtime = nullptr;
    UIWidgetRegistry m_Registry;
    UIWidgetMutationQueue m_Mutations;
    std::vector<UIWidgetHandle> m_ScrollViews;
    std::vector<UIWidgetHandle> m_Scrollbars;
};

} // namespace Engine::UI2D
