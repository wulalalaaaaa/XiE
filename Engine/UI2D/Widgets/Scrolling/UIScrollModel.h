#pragma once

#include "UI2D/Widgets/Scrolling/UIScrollTypes.h"

namespace Engine::UI2D {

class UIScrollModel {
public:
    void SetViewportSize(Engine::Vec2F size);
    void SetContentSize(Engine::Vec2F size);
    void SetPolicy(const UIScrollPolicy& policy);

    [[nodiscard]] Engine::Vec2F ViewportSize() const noexcept { return m_ViewportSize; }
    [[nodiscard]] Engine::Vec2F ContentSize() const noexcept { return m_ContentSize; }
    [[nodiscard]] Engine::Vec2F Offset() const noexcept { return m_Offset; }
    [[nodiscard]] Engine::Vec2F MaximumOffset() const noexcept;
    [[nodiscard]] Engine::Vec2F NormalizedOffset() const noexcept;
    [[nodiscard]] const UIScrollPolicy& Policy() const noexcept { return m_Policy; }

    bool SetOffset(Engine::Vec2F offset, UIScrollInputMode source);
    bool ScrollBy(Engine::Vec2F delta, UIScrollInputMode source);
    bool SetNormalizedOffset(Engine::Vec2F normalized);

    [[nodiscard]] bool CanScroll(UIScrollAxis axis) const noexcept;
    [[nodiscard]] bool IsAtStart(UIScrollAxis axis) const noexcept;
    [[nodiscard]] bool IsAtEnd(UIScrollAxis axis) const noexcept;
    [[nodiscard]] float PageStep(UIScrollAxis axis) const noexcept;
    [[nodiscard]] std::uint64_t Revision() const noexcept { return m_Revision; }
    [[nodiscard]] UIScrollInputMode LastSource() const noexcept { return m_LastSource; }

private:
    static Engine::Vec2F NormalizeSize(Engine::Vec2F value) noexcept;
    static UIScrollPolicy NormalizePolicy(UIScrollPolicy value) noexcept;
    Engine::Vec2F ClampOffset(Engine::Vec2F value) const noexcept;
    bool RecomputeAfterBoundsChange();

    Engine::Vec2F m_ViewportSize{};
    Engine::Vec2F m_ContentSize{};
    Engine::Vec2F m_Offset{};
    UIScrollPolicy m_Policy{};
    UIScrollInputMode m_LastSource = UIScrollInputMode::Programmatic;
    std::uint64_t m_Revision = 1;
};

} // namespace Engine::UI2D
