#pragma once

#include "UI2D/Layout/IUILayoutEngine.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Engine {
class ITextLayoutService;
class ITextureInfoProvider2D;
}

namespace Engine::UI2D {

struct UINodeRecord;

struct UILayoutDebugStats {
    std::uint32_t totalNodes = 0;
    std::uint32_t visibleNodes = 0;
    std::uint32_t measuredNodes = 0;
    std::uint32_t arrangedNodes = 0;
    std::uint32_t maxDepth = 0;
};

// Phase 3B layout implementation. It owns no scene or renderer resources.
class BasicLayoutEngine final : public IUILayoutEngine {
public:
    explicit BasicLayoutEngine(
        const Engine::ITextureInfoProvider2D* textures = nullptr,
        const Engine::ITextLayoutService* textLayouts = nullptr,
        Engine::Vec2F unavailableImagePlaceholder = {});

    UILayoutResult UpdateLayout(UIScene& scene, const UILayoutContext& context) override;

    [[nodiscard]] const UILayoutDebugStats& LastDebugStats() const noexcept { return m_DebugStats; }
    [[nodiscard]] std::string DumpScene(const UIScene& scene) const;

private:
    Engine::Vec2F MeasureNode(
        UIScene& scene,
        UINodeHandle node,
        Engine::Vec2F availableSize,
        const UILayoutContext& context,
        std::uint32_t depth);
    void ArrangeNode(
        UIScene& scene,
        UINodeHandle node,
        Engine::RectF finalRect,
        const UILayoutContext& context,
        std::uint32_t depth);

    Engine::Vec2F MeasureIntrinsic(const UINodeRecord& node) const;
    void ArrangeChildren(UIScene& scene, UINodeRecord& node, const UILayoutContext& context, std::uint32_t depth);
    void SetError(UILayoutError error);

    const Engine::ITextureInfoProvider2D* m_Textures = nullptr;
    const Engine::ITextLayoutService* m_TextLayouts = nullptr;
    Engine::Vec2F m_UnavailableImagePlaceholder{};
    std::vector<std::uint8_t> m_VisitStates;
    UILayoutDebugStats m_DebugStats{};
    UILayoutError m_Error = UILayoutError::None;
    bool m_LayoutChanged = false;
};

} // namespace Engine::UI2D
