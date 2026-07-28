#pragma once

#include "UI2D/Input/IUIFocusManager.h"
#include "UI2D/Input/UIEventTypes.h"

#include <string>
#include <vector>

namespace Engine::UI2D {

struct UIFocusState {
    UINodeHandle focusedNode{};
    UINodeHandle restoreCandidate{};
    UINodeHandle focusRoot{};
    std::vector<UINodeHandle> focusRoute;
    bool windowFocused = true;
};

class BasicUIFocusManager final : public IUIFocusManager {
public:
    [[nodiscard]] UINodeHandle GetFocusedNode() const override { return m_State.focusedNode; }
    [[nodiscard]] bool HasActiveFocus() const override {
        return m_State.windowFocused && m_State.focusedNode.IsValid();
    }
    [[nodiscard]] bool IsFocused(UINodeHandle node) const override {
        return HasActiveFocus() && m_State.focusedNode == node;
    }
    [[nodiscard]] bool HasFocusWithin(UINodeHandle node) const override;
    [[nodiscard]] std::span<const UINodeHandle> FocusRoute() const override {
        return m_State.focusRoute;
    }
    [[nodiscard]] UINodeHandle GetFocusRoot() const override { return m_State.focusRoot; }
    [[nodiscard]] bool IsWindowFocused() const override { return m_State.windowFocused; }

    UIFocusRequestResult RequestFocus(
        UIScene& scene, const UIFocusRequest& request, UIFocusDispatchContext& context) override;
    UIFocusRequestResult ClearFocus(
        UIScene& scene, UIFocusReason reason, UIFocusDispatchContext& context) override;
    UIFocusMoveResult MoveFocus(
        UIScene& scene, UIFocusMoveDirection direction,
        const UIFocusMoveContext& moveContext, UIFocusDispatchContext& dispatchContext) override;
    void SetFocusRoot(
        UIScene& scene, UINodeHandle root, UIFocusDispatchContext& context) override;
    void OnWindowFocusChanged(
        UIScene& scene, bool focused, UIFocusDispatchContext& context) override;
    void OnNodeInvalidated(
        UIScene& scene, const UIFocusInvalidation& invalidation,
        UIFocusDispatchContext& context) override;

    [[nodiscard]] const UIFocusState& State() const noexcept { return m_State; }
    [[nodiscard]] const UIFocusDebugStats& DebugStats() const noexcept { return m_Stats; }
    [[nodiscard]] const std::string& LastDebugLog() const noexcept { return m_DebugLog; }
    void SetDebugLoggingEnabled(bool enabled) { m_DebugLoggingEnabled = enabled; }
    void DiscardRestoreCandidate() noexcept { m_State.restoreCandidate = {}; }
    [[nodiscard]] std::span<const UINodeHandle> FocusCandidates(UIScene& scene);

private:
    void EnsureFocusRoot(UIScene& scene);
    bool BuildRoute(const UIScene& scene, UINodeHandle target, std::vector<UINodeHandle>& route) const;
    void DispatchFocusEvent(
        UIScene& scene, UIEventType type, UINodeHandle target, UINodeHandle related,
        UIFocusReason reason, std::span<const UINodeHandle> route,
        UIFocusDispatchContext& context, bool* prevented = nullptr);
    UIFocusRequestResult ApplyFocus(
        UIScene& scene, UINodeHandle target, UIFocusReason reason,
        UIFocusRequestPriority priority, UIFocusDispatchContext& context);
    void MarkRouteChange(
        UIScene& scene, std::span<const UINodeHandle> oldRoute,
        std::span<const UINodeHandle> newRoute);
    void RebuildCandidates(UIScene& scene);
    void AppendCandidates(UIScene& scene, UINodeHandle node);
    void AppendDebug(std::string line);

    UIFocusState m_State{};
    std::uint64_t m_CachedFocusRevision = 0;
    UINodeHandle m_CachedFocusRoot{};
    std::vector<UINodeHandle> m_FocusCandidates;
    UIFocusDebugStats m_Stats{};
    bool m_DebugLoggingEnabled = false;
    std::string m_DebugLog;
};

} // namespace Engine::UI2D
