#pragma once

#include "Input/InputEventQueue.h"
#include "Renderer2D/DrawList2D.h"
#include "UI2D/Animation/BasicUIAnimator.h"
#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/Core/UIScene.h"
#include "UI2D/Core/UISceneMutationQueue.h"
#include "UI2D/HitTest/UIHitTestTypes.h"
#include "UI2D/Input/UIEventListenerRegistry.h"
#include "UI2D/Input/BasicUIFocusManager.h"
#include "UI2D/Input/UIFocusRequestQueue.h"
#include "UI2D/Input/UIInteractionState.h"
#include "UI2D/Runtime/UI2DServices.h"
#include "UI2D/Runtime/UI2DUpdateResult.h"
#include "UI2D/Theme/ResolvedUITheme.h"
#include "UI2D/Style/BasicUIInteractionStyleResolver.h"
#include "UI2D/Widgets/Runtime/UIWidgetRuntime.h"

#include <memory>

namespace Engine::UI2D {

enum class UIThemeSwitchMode { Immediate, Animated };

class UIWindowRuntime {
public:
    UIWindowRuntime(
        Engine::WindowHandle window,
        UI2DServices services,
        std::shared_ptr<const ResolvedUITheme> theme);
    ~UIWindowRuntime();

    UIWindowRuntime(const UIWindowRuntime&) = delete;
    UIWindowRuntime& operator=(const UIWindowRuntime&) = delete;
    UIWindowRuntime(UIWindowRuntime&&) = delete;
    UIWindowRuntime& operator=(UIWindowRuntime&&) = delete;

    UIScene& Scene() noexcept { return m_Scene; }
    const UIScene& Scene() const noexcept { return m_Scene; }
    UISceneMutationQueue& Mutations() noexcept { return m_Mutations; }
    Engine::InputEventQueue& InputQueue() noexcept { return m_Input; }
    UIEventListenerRegistry& EventListeners() noexcept { return m_Listeners; }
    UIWidgetRuntime& Widgets() noexcept { return m_Widgets; }
    const UIWidgetRuntime& Widgets() const noexcept { return m_Widgets; }
    [[nodiscard]] UIInteractionSnapshot Interaction() const noexcept {
        return {&m_Interaction, &m_FocusManager, m_Scene.FocusRevision()};
    }
    UIEventListenerHandle AddEventListener(
        UINodeHandle node, UIEventType type, UIEventPhaseMask phases, UIEventCallback callback) {
        return m_Listeners.AddListener(node, type, phases, std::move(callback));
    }
    bool RemoveEventListener(UIEventListenerHandle handle) {
        return m_Listeners.RemoveListener(handle);
    }
    void RequestFocus(
        UINodeHandle node, UIFocusReason reason = UIFocusReason::Programmatic);
    void ClearFocus(UIFocusReason reason = UIFocusReason::Clear);
    [[nodiscard]] UINodeHandle FocusedNode() const { return m_FocusManager.GetFocusedNode(); }
    [[nodiscard]] bool IsFocused(UINodeHandle node) const { return m_FocusManager.IsFocused(node); }
    [[nodiscard]] bool HasFocusWithin(UINodeHandle node) const {
        return m_FocusManager.HasFocusWithin(node);
    }
    void SetFocusRoot(UINodeHandle root);
    [[nodiscard]] UINodeHandle FocusRoot() const { return m_FocusManager.GetFocusRoot(); }
    UIFocusPolicy& FocusPolicy() noexcept { return m_FocusPolicy; }
    const UIFocusPolicy& FocusPolicy() const noexcept { return m_FocusPolicy; }

    UIAnimationStartResult StartAnimation(const UIAnimationDesc& desc) {
        return m_Animator.Start(m_Scene, desc);
    }
    bool PauseAnimation(UIAnimationHandle handle) { return m_Animator.Pause(handle); }
    bool ResumeAnimation(UIAnimationHandle handle) { return m_Animator.Resume(handle); }
    bool CancelAnimation(UIAnimationHandle handle, UIAnimationCancelMode mode) {
        return m_Animator.Cancel(m_Scene, handle, mode);
    }
    std::uint32_t CancelAnimationsForNode(UINodeHandle node, UIAnimationCancelMode mode) {
        return m_Animator.CancelAllForNodeChannel(
            m_Scene, node, UIAnimationChannel::Application, mode);
    }
    std::uint32_t CancelAllAnimations(UIAnimationCancelMode mode = UIAnimationCancelMode::RestoreBase) {
        return m_Animator.CancelAllForChannel(m_Scene, UIAnimationChannel::Application, mode);
    }
    bool ClearAnimatedOverride(UINodeHandle node, UIAnimatedProperty property) {
        return Engine::UI2D::ClearAnimatedOverride(m_Scene, node, property);
    }
    [[nodiscard]] std::uint32_t ActiveAnimationCount() const noexcept {
        return m_Animator.ActiveAnimationCount();
    }
    bool SetStyleClass(UINodeHandle node, UIStyleClassId styleClass) {
        return m_Scene.SetStyleClass(node, styleClass);
    }
    [[nodiscard]] UIStyleClassId GetStyleClass(UINodeHandle node) const {
        return m_Scene.GetStyleClass(node);
    }
    [[nodiscard]] const UIStyleRuntimeProperties* TryGetRuntimeStyle(UINodeHandle node) const {
        return m_Scene.TryGetRuntimeStyle(node);
    }
    [[nodiscard]] const std::shared_ptr<const ResolvedUITheme>& Theme() const noexcept { return m_Theme; }

    bool EnqueueInput(Engine::InputEvent event);
    UI2DUpdateResult Update(double deltaSeconds, Engine::Vec2F logicalWindowSize, float dpiScale);
    [[nodiscard]] UIHitTestResult HitTest(
        Engine::Vec2F scenePosition,
        UIHitTestContext context = {}) const;

    const Engine::DrawList2D& DrawList() const noexcept { return m_DrawList; }
    UIFrameActivity QueryFrameActivity() const;

    void SetVisible(bool visible);
    bool IsVisible() const noexcept { return m_Visible; }
    void SetTheme(std::shared_ptr<const ResolvedUITheme> theme,
        UIThemeSwitchMode mode = UIThemeSwitchMode::Immediate);

private:
    void OnNodeInvalidated(UINodeHandle node);
    void SanitizeInteractionState();
    void SanitizeFocusState();
    UIFocusDispatchContext MakeFocusContext();
    UIFocusRequestResult FlushFocusRequests();

    Engine::WindowHandle m_Window{};
    UI2DServices m_Services;
    std::shared_ptr<const ResolvedUITheme> m_Theme;
    UIScene m_Scene;
    Engine::InputEventQueue m_Input;
    UISceneMutationQueue m_Mutations;
    UIEventListenerRegistry m_Listeners;
    UIWidgetRuntime m_Widgets;
    UIInteractionRuntimeState m_Interaction;
    BasicUIFocusManager m_FocusManager;
    BasicUIAnimator m_Animator;
    BasicUIInteractionStyleResolver m_StyleResolver;
    UIFocusRequestQueue m_FocusRequests;
    UIFocusPolicy m_FocusPolicy{};
    Engine::DrawList2D m_DrawList;
    Engine::Vec2F m_LastLogicalSize{-1.0f, -1.0f};
    float m_LastDpiScale = -1.0f;
    std::uint64_t m_BuiltVisualRevision = 0;
    std::uint64_t m_BuiltResourceRevision = 0;
    bool m_Visible = true;
    bool m_ForceImmediateStyleResolve = true;
};

} // namespace Engine::UI2D
