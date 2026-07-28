#pragma once

#include "Renderer2D/DrawTypes2D.h"
#include "UI2D/Style/UIStyleTypes.h"

#include <string>
#include <vector>

namespace Engine::UI2D {

struct UIThemeColorTokens {
    Engine::Color4f surface{0.08f, 0.09f, 0.11f, 0.75f};
    Engine::Color4f surfaceElevated{0.22f, 0.24f, 0.28f, 0.80f};
    Engine::Color4f foreground{1.0f, 1.0f, 1.0f, 0.95f};
    Engine::Color4f foregroundMuted{0.68f, 0.70f, 0.74f, 1.0f};
    Engine::Color4f accent{0.30f, 0.58f, 1.0f, 1.0f};
    Engine::Color4f accentBright{0.48f, 0.72f, 1.0f, 1.0f};
    Engine::Color4f border{0.36f, 0.38f, 0.42f, 1.0f};
    Engine::Color4f focus{0.35f, 0.65f, 1.0f, 1.0f};
    Engine::Color4f disabled{0.50f, 0.50f, 0.52f, 1.0f};
};
struct UIThemeMetricTokens {
    float cornerRadiusSmall = 4.0f;
    float cornerRadiusMedium = 10.0f;
    float cornerRadiusLarge = 16.0f;
    float borderWidth = 1.0f;
    float focusRingWidth = 2.0f;
    float spacingSmall = 4.0f;
    float spacingMedium = 8.0f;
    float spacingLarge = 12.0f;
};
struct UIThemeAnimationTokens {
    double hoverDuration = 0.12;
    double pressDuration = 0.08;
    double focusDuration = 0.14;
    double disabledDuration = 0.10;
};
struct UIThemeTokens {
    UIThemeColorTokens colors;
    UIThemeMetricTokens metrics;
    UIThemeAnimationTokens animations;
};

struct UIStyleRuleDefinition {
    UIInteractionStateMask requiredStates = 0;
    UIInteractionStateMask forbiddenStates = 0;
    UIStyleProperties properties;
    UIStyleTransitions transitions;
};
struct UIStyleClassDefinition {
    std::string name;
    UIStyleProperties baseStyle;
    UIStyleTransitions baseTransitions;
    std::vector<UIStyleRuleDefinition> rules;
};
struct UIThemeDefinition {
    std::string name;
    UIThemeTokens tokens;
    std::vector<UIStyleClassDefinition> styleClasses;
    Engine::FontHandle defaultFont{};
    float fontSize = 16.0f;
    Engine::TextureHandle backgroundTexture{};
    Engine::TextureHandle nineSliceTexture{};
    int effectLevel = 0;
};

} // namespace Engine::UI2D
