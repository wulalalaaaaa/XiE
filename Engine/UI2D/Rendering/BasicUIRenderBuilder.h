#pragma once

#include "UI2D/Rendering/IUIRenderBuilder.h"
#include "UI2D/Core/UIVisual.h"

namespace Engine {
class ITextLayoutService;
class ITextureInfoProvider2D;
}

namespace Engine::UI2D {

struct UINodeRecord;

class BasicUIRenderBuilder final : public IUIRenderBuilder {
public:
    BasicUIRenderBuilder(
        const Engine::ITextureInfoProvider2D* textures = nullptr,
        const Engine::ITextLayoutService* textLayouts = nullptr);

    UIRenderResult Build(
        const UIScene& scene,
        const ResolvedUITheme& theme,
        Engine::DrawList2D& output,
        const UIRenderContext& context) override;

    [[nodiscard]] std::uint64_t ResourceRevision() const override;

private:
    bool EmitNode(
        const UINodeRecord& node,
        const UIRenderContext& context,
        Engine::DrawList2D& output,
        UIRenderResult& result) const;
    bool EmitPanel(const UINodeRecord& node, const UIPanelVisual& visual,
        Engine::DrawList2D& output, UIRenderResult& result) const;
    bool EmitImage(const UINodeRecord& node, const UIImageVisual& visual,
        Engine::DrawList2D& output, UIRenderResult& result) const;
    bool EmitText(const UINodeRecord& node, const UITextVisual& visual,
        Engine::DrawList2D& output, UIRenderResult& result) const;
    bool EmitNineSlice(const UINodeRecord& node, const UINineSliceVisual& visual,
        Engine::DrawList2D& output, UIRenderResult& result) const;
    bool EmitShape(const UINodeRecord& node, const UIShapeVisual& visual,
        Engine::DrawList2D& output, UIRenderResult& result) const;
    bool EmitTexturePlaceholder(const UINodeRecord& node, Engine::TextureState state,
        Engine::DrawList2D& output, UIRenderResult& result) const;

    const Engine::ITextureInfoProvider2D* m_Textures = nullptr;
    const Engine::ITextLayoutService* m_TextLayouts = nullptr;
};

} // namespace Engine::UI2D
