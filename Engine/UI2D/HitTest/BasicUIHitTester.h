#pragma once

#include "UI2D/Input/IUIHitTester.h"

#include <string>

namespace Engine::UI2D {

struct UINodeRecord;

class BasicUIHitTester final : public IUIHitTester {
public:
    UIHitTestResult HitTest(
        const UIScene& scene,
        const UIHitTestContext& context,
        Engine::Vec2F scenePosition) const override;

    [[nodiscard]] const UIHitTestStats& LastStats() const noexcept { return m_LastStats; }
    [[nodiscard]] const std::string& LastDebugDump() const noexcept { return m_LastDebugDump; }

private:
    bool IsNodeEligible(const UINodeRecord& node, bool includeDisabled, UIHitTestStats& stats) const;
    bool PassesAncestorClips(
        const UIScene& scene,
        UINodeHandle node,
        UINodeHandle searchRoot,
        Engine::Vec2F scenePosition,
        UIHitTestStats& stats) const;
    bool HitNodeGeometry(
        const UINodeRecord& node,
        Engine::Vec2F scenePosition,
        Engine::Vec2F& outLocalPosition) const;
    bool BuildRoute(
        const UIScene& scene,
        UINodeHandle searchRoot,
        UINodeHandle target,
        std::vector<UINodeHandle>& output) const;

    mutable UIHitTestStats m_LastStats{};
    mutable std::string m_LastDebugDump;
};

} // namespace Engine::UI2D
