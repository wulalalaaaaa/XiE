#pragma once

#include "UI2D/Input/IUIInputRouter.h"
#include "UI2D/Input/UIEventContext.h"
#include "UI2D/Input/UIHoverTracker.h"
#include "UI2D/HitTest/UIHitTestTypes.h"

#include <string>
#include <vector>

namespace Engine::UI2D {

struct UIInputRouterStats {
    std::uint32_t inputEvents = 0;
    std::uint32_t routedEvents = 0;
    std::uint32_t listenerInvocations = 0;
    std::uint32_t hitTests = 0;
    std::uint32_t hoverEnters = 0;
    std::uint32_t hoverLeaves = 0;
    std::uint32_t capturesStarted = 0;
    std::uint32_t capturesReleased = 0;
    std::uint32_t pressesStarted = 0;
    std::uint32_t pressesCanceled = 0;
    std::uint32_t clicksGenerated = 0;
};

class BasicUIInputRouter final : public IUIInputRouter {
public:
    UIInputDispatchResult Dispatch(
        UIScene& scene,
        std::span<const Engine::InputEvent> events,
        UIInputDispatchContext& context) override;
    void OnNodeInvalidated(UIScene& scene, UINodeHandle node) override;
    void OnWindowHidden(UIScene& scene) override;
    void OnWindowFocusLost(UIScene& scene) override;

    void SetDebugLoggingEnabled(bool enabled) { m_DebugLoggingEnabled = enabled; }
    [[nodiscard]] const std::string& LastDebugLog() const noexcept { return m_LastDebugLog; }
    [[nodiscard]] const UIInputRouterStats& LastStats() const noexcept { return m_LastStats; }

private:
    struct RoutedEventResult {
        UIEvent event;
        UIEventDispatchRequests requests;
    };

    UIHitTestResult HitTest(
        const UIScene& scene,
        Engine::Vec2F position,
        UIInputDispatchContext& context,
        UIInputDispatchResult& result);
    bool BuildRoute(const UIScene& scene, UINodeHandle target, std::vector<UINodeHandle>& route) const;
    RoutedEventResult RouteEvent(
        UIScene& scene,
        UIEvent event,
        std::span<const UINodeHandle> route,
        UIInputDispatchContext& context,
        UIInputDispatchResult& result);
    void UpdateHover(
        UIScene& scene,
        Engine::PointerId pointerId,
        const UIHitTestResult& hit,
        const UIEvent& source,
        UIInputDispatchContext& context,
        UIInputDispatchResult& result);
    void DispatchHoverTransition(
        UIScene& scene,
        Engine::PointerId pointerId,
        const UIHoverTransition& transition,
        const UIEvent& source,
        UIInputDispatchContext& context,
        UIInputDispatchResult& result);
    void DispatchDirect(
        UIScene& scene,
        UINodeHandle node,
        UIEvent event,
        UIInputDispatchContext& context,
        UIInputDispatchResult& result);
    void HandleFocusLost(
        UIScene& scene,
        const Engine::WindowFocusEvent& input,
        UIInputDispatchContext& context,
        UIInputDispatchResult& result);
    UINodeHandle ResolveKeyboardTarget(UIScene& scene, IUIFocusManager& focus) const;
    void MarkVisual(UIScene& scene, UINodeHandle node, UIInputDispatchResult& result);
    void AppendDebug(const std::string& line);

    bool m_DispatchInProgress = false;
    bool m_DebugLoggingEnabled = false;
    UIInputDispatchContext* m_ActiveContext = nullptr;
    UIInputRouterStats m_LastStats{};
    std::string m_LastDebugLog;
};

} // namespace Engine::UI2D
