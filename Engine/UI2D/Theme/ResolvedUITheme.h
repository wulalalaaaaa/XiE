#pragma once

#include "Foundation/Handles/GenerationalHandle.h"
#include "Renderer2D/DrawTypes2D.h"
#include "UI2D/Theme/UIThemeDefinition.h"

#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Engine::UI2D {

struct UIThemeHandleTag;
using UIThemeHandle = Engine::GenerationalHandle<UIThemeHandleTag>;

struct UIStyleSelector {
    UIStyleClassId styleClass{};
    UIInteractionStateMask requiredStates = 0;
    UIInteractionStateMask forbiddenStates = 0;
    std::uint32_t declarationOrder = 0;
};
struct UIStyleRule {
    UIStyleSelector selector;
    UIStyleProperties properties;
    UIStyleTransitions transitions;
};
struct UIResolvedStyleClass {
    UIStyleProperties baseStyle;
    UIStyleTransitions baseTransitions;
    std::vector<UIStyleRule> rules;
};

class ResolvedUITheme {
public:
    ResolvedUITheme() = default;
    ResolvedUITheme(UIThemeHandle handle, std::uint64_t revision, std::string name,
        UIThemeTokens tokens, std::vector<std::string> classNames,
        std::vector<UIResolvedStyleClass> classes, UIThemeDefinition source);

    [[nodiscard]] UIThemeHandle Handle() const noexcept { return m_Handle; }
    [[nodiscard]] std::uint64_t Revision() const noexcept { return m_Revision; }
    [[nodiscard]] std::string_view Name() const noexcept { return m_Name; }
    [[nodiscard]] UIStyleClassId FindStyleClass(std::string_view name) const;
    [[nodiscard]] std::string_view StyleClassName(UIStyleClassId id) const;
    [[nodiscard]] const UIResolvedStyleClass* TryGetStyleClass(UIStyleClassId id) const;
    [[nodiscard]] const UIThemeTokens& Tokens() const noexcept { return m_Tokens; }

    // Legacy layout/render tokens retained until those consumers migrate to Tokens().
    Engine::Color4f backgroundColor{0.08f, 0.09f, 0.11f, 0.75f};
    Engine::Color4f panelColor{0.22f, 0.24f, 0.28f, 0.80f};
    Engine::Color4f textColor{1.0f, 1.0f, 1.0f, 0.95f};
    Engine::FontHandle defaultFont{};
    float fontSize = 16.0f;
    float margin = 12.0f;
    float padding = 10.0f;
    float spacing = 8.0f;
    float cornerRadius = 10.0f;
    float ringWidth = 2.0f;
    float hoverDuration = 0.12f;
    float pressDuration = 0.08f;
    Engine::TextureHandle backgroundTexture{};
    Engine::TextureHandle nineSliceTexture{};
    int effectLevel = 0;

private:
    UIThemeHandle m_Handle{};
    std::uint64_t m_Revision = 1;
    std::string m_Name{"Default"};
    UIThemeTokens m_Tokens{};
    std::uint32_t m_StyleGeneration = 0;
    std::unordered_map<std::string, UIStyleClassId> m_StyleNames;
    std::vector<std::string> m_StyleClassNames;
    std::vector<UIResolvedStyleClass> m_StyleClasses;
};

std::shared_ptr<const ResolvedUITheme> MakeDefaultResolvedUITheme();
std::shared_ptr<const ResolvedUITheme> MakeDefaultLightResolvedUITheme();
std::shared_ptr<const ResolvedUITheme> MakeDefaultDarkResolvedUITheme();

} // namespace Engine::UI2D
