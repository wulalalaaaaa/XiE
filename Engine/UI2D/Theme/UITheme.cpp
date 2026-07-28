#include "UI2D/Theme/ResolvedUITheme.h"

#include "UI2D/Theme/BasicUIThemeResolver.h"

namespace Engine::UI2D {

ResolvedUITheme::ResolvedUITheme(UIThemeHandle handle, std::uint64_t revision, std::string name,
    UIThemeTokens tokens, std::vector<std::string> classNames,
    std::vector<UIResolvedStyleClass> classes, UIThemeDefinition source)
    : m_Handle(handle), m_Revision(revision), m_Name(std::move(name)), m_Tokens(std::move(tokens)),
      m_StyleGeneration(handle.generation) {
    m_StyleClasses.reserve(classes.size() + 1);
    m_StyleClassNames.reserve(classNames.size() + 1);
    m_StyleClasses.emplace_back();
    m_StyleClassNames.emplace_back();
    for (std::size_t i = 0; i < classes.size(); ++i) {
        const UIStyleClassId id{static_cast<std::uint32_t>(i + 1), m_StyleGeneration};
        m_StyleNames.emplace(classNames[i], id);
        m_StyleClassNames.push_back(std::move(classNames[i]));
        m_StyleClasses.push_back(std::move(classes[i]));
    }
    backgroundColor = m_Tokens.colors.surface;
    panelColor = m_Tokens.colors.surfaceElevated;
    textColor = m_Tokens.colors.foreground;
    defaultFont = source.defaultFont;
    fontSize = source.fontSize;
    margin = m_Tokens.metrics.spacingLarge;
    padding = m_Tokens.metrics.spacingMedium;
    spacing = m_Tokens.metrics.spacingMedium;
    cornerRadius = m_Tokens.metrics.cornerRadiusMedium;
    ringWidth = m_Tokens.metrics.focusRingWidth;
    hoverDuration = static_cast<float>(m_Tokens.animations.hoverDuration);
    pressDuration = static_cast<float>(m_Tokens.animations.pressDuration);
    backgroundTexture = source.backgroundTexture;
    nineSliceTexture = source.nineSliceTexture;
    effectLevel = source.effectLevel;
}
std::string_view ResolvedUITheme::StyleClassName(UIStyleClassId id) const {
    if (!id.IsValid() || id.generation != m_StyleGeneration || id.index >= m_StyleClassNames.size()) return {};
    return m_StyleClassNames[id.index];
}

UIStyleClassId ResolvedUITheme::FindStyleClass(std::string_view name) const {
    const auto it = m_StyleNames.find(std::string(name));
    return it == m_StyleNames.end() ? UIStyleClassId{} : it->second;
}
const UIResolvedStyleClass* ResolvedUITheme::TryGetStyleClass(UIStyleClassId id) const {
    if (!id.IsValid() || id.generation != m_StyleGeneration || id.index >= m_StyleClasses.size()) return nullptr;
    return &m_StyleClasses[id.index];
}

namespace {
UIThemeDefinition MakeDefaultDefinition(bool light) {
    UIThemeDefinition definition;
    definition.name = light ? "DefaultLight" : "DefaultDark";
    if (light) {
        definition.tokens.colors.surface = {0.94f, 0.95f, 0.97f, 1.0f};
        definition.tokens.colors.surfaceElevated = {1.0f, 1.0f, 1.0f, 1.0f};
        definition.tokens.colors.foreground = {0.10f, 0.11f, 0.14f, 1.0f};
        definition.tokens.colors.foregroundMuted = {0.42f, 0.44f, 0.48f, 1.0f};
    }
    const auto& colors = definition.tokens.colors;
    const auto& animation = definition.tokens.animations;
    UIStyleClassDefinition panel{"Panel"};
    panel.baseStyle.colorMultiplier = colors.surfaceElevated;
    UIStyleClassDefinition interactive{"Interactive"};
    interactive.baseStyle.scaleMultiplier = Engine::Vec2F{1.0f, 1.0f};
    interactive.baseStyle.opacityMultiplier = 1.0f;
    interactive.baseStyle.colorMultiplier = Engine::Color4f{};
    interactive.baseTransitions.scale = {animation.hoverDuration, UIEasing::EaseOutCubic};
    interactive.baseTransitions.opacity = {animation.pressDuration, UIEasing::EaseOutQuad};
    interactive.baseTransitions.color = {animation.hoverDuration, UIEasing::EaseOutQuad};
    UIStyleRuleDefinition hover;
    hover.requiredStates = ToMask(UIInteractionState::Hovered);
    hover.properties.scaleMultiplier = Engine::Vec2F{1.04f, 1.04f};
    hover.properties.colorMultiplier = {1.08f, 1.08f, 1.08f, 1.0f};
    hover.transitions = interactive.baseTransitions;
    UIStyleRuleDefinition pressed;
    pressed.requiredStates = ToMask(UIInteractionState::Pressed);
    pressed.properties.scaleMultiplier = Engine::Vec2F{0.96f, 0.96f};
    pressed.properties.opacityMultiplier = 0.85f;
    pressed.transitions = interactive.baseTransitions;
    UIStyleRuleDefinition focused;
    focused.requiredStates = ToMask(UIInteractionState::Focused);
    focused.properties.colorMultiplier = colors.accent;
    focused.transitions = interactive.baseTransitions;
    UIStyleRuleDefinition disabled;
    disabled.requiredStates = ToMask(UIInteractionState::Disabled);
    disabled.properties.opacityMultiplier = 0.45f;
    disabled.properties.colorMultiplier = colors.disabled;
    disabled.transitions = interactive.baseTransitions;
    UIStyleRuleDefinition hoverFocused;
    hoverFocused.requiredStates = UIInteractionState::Hovered | UIInteractionState::Focused;
    hoverFocused.properties.scaleMultiplier = Engine::Vec2F{1.04f, 1.04f};
    hoverFocused.properties.colorMultiplier = colors.accentBright;
    hoverFocused.transitions = interactive.baseTransitions;
    interactive.rules = {hover, pressed, focused, disabled, hoverFocused};
    UIStyleClassDefinition image{"ImageItem"};
    image.baseStyle.colorMultiplier = Engine::Color4f{};
    image.rules.push_back(hover);
    UIStyleClassDefinition ring{"FocusRing"};
    ring.baseStyle.opacityMultiplier = 0.0f;
    UIStyleRuleDefinition focusWithin;
    focusWithin.requiredStates = ToMask(UIInteractionState::FocusWithin);
    focusWithin.properties.opacityMultiplier = 1.0f;
    focusWithin.properties.colorMultiplier = colors.focus;
    focusWithin.transitions.opacity = {animation.focusDuration, UIEasing::EaseOutQuad};
    ring.rules.push_back(focusWithin);
    definition.styleClasses = {panel, interactive, image, ring};
    return definition;
}
} // namespace

std::shared_ptr<const ResolvedUITheme> MakeDefaultLightResolvedUITheme() {
    return BasicUIThemeResolver{}.Resolve(MakeDefaultDefinition(true)).theme;
}
std::shared_ptr<const ResolvedUITheme> MakeDefaultDarkResolvedUITheme() {
    return BasicUIThemeResolver{}.Resolve(MakeDefaultDefinition(false)).theme;
}
std::shared_ptr<const ResolvedUITheme> MakeDefaultResolvedUITheme() {
    static const auto theme = MakeDefaultDarkResolvedUITheme();
    return theme;
}

} // namespace Engine::UI2D
