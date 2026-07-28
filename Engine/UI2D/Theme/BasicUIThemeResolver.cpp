#include "UI2D/Theme/BasicUIThemeResolver.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <unordered_set>

namespace Engine::UI2D {
namespace {
std::atomic<std::uint32_t> g_NextThemeGeneration{1};
std::atomic<std::uint64_t> g_NextThemeRevision{1};
constexpr UIInteractionStateMask kValidStates = (1u << 9) - 1u;

bool Finite(Engine::Vec2F value) { return std::isfinite(value.x) && std::isfinite(value.y); }
bool Finite(Engine::Color4f value) {
    return std::isfinite(value.r) && std::isfinite(value.g) &&
        std::isfinite(value.b) && std::isfinite(value.a);
}
void ValidateProperties(const UIStyleProperties& value, std::string_view where,
    std::vector<std::string>& errors) {
    if (value.positionOffset && !Finite(*value.positionOffset)) errors.emplace_back(std::string(where) + ": position must be finite");
    if (value.scaleMultiplier && !Finite(*value.scaleMultiplier)) errors.emplace_back(std::string(where) + ": scale must be finite");
    if (value.rotationOffsetRadians && !std::isfinite(*value.rotationOffsetRadians)) errors.emplace_back(std::string(where) + ": rotation must be finite");
    if (value.opacityMultiplier && (!std::isfinite(*value.opacityMultiplier) || *value.opacityMultiplier < 0.0f || *value.opacityMultiplier > 1.0f))
        errors.emplace_back(std::string(where) + ": opacity must be in [0,1]");
    if (value.colorMultiplier && !Finite(*value.colorMultiplier)) errors.emplace_back(std::string(where) + ": color must be finite");
}
void ValidateTransitions(const UIStyleTransitions& value, std::string_view where,
    std::vector<std::string>& errors) {
    const UIStyleTransition* transitions[] = {&value.position, &value.scale, &value.rotation, &value.opacity, &value.color};
    for (const UIStyleTransition* transition : transitions) {
        if (!std::isfinite(transition->durationSeconds) || transition->durationSeconds < 0.0) {
            errors.emplace_back(std::string(where) + ": transition duration must be finite and non-negative");
            return;
        }
    }
}
bool ValidColor(Engine::Color4f value) {
    return Finite(value) && value.r >= 0.0f && value.r <= 1.0f &&
        value.g >= 0.0f && value.g <= 1.0f && value.b >= 0.0f && value.b <= 1.0f &&
        value.a >= 0.0f && value.a <= 1.0f;
}
} // namespace

UIThemeResolveResult BasicUIThemeResolver::Resolve(const UIThemeDefinition& definition) {
    UIThemeResolveResult result;
    if (definition.name.empty()) result.errors.emplace_back("theme name must not be empty");
    const auto& colors = definition.tokens.colors;
    const Engine::Color4f allColors[] = {colors.surface, colors.surfaceElevated, colors.foreground,
        colors.foregroundMuted, colors.accent, colors.accentBright, colors.border, colors.focus, colors.disabled};
    for (Engine::Color4f color : allColors) if (!ValidColor(color)) {
        result.errors.emplace_back("theme color tokens must be finite and in [0,1]"); break;
    }
    const auto& animationTokens = definition.tokens.animations;
    const double tokenDurations[] = {animationTokens.hoverDuration, animationTokens.pressDuration,
        animationTokens.focusDuration, animationTokens.disabledDuration};
    for (double duration : tokenDurations) if (!std::isfinite(duration) || duration < 0.0) {
        result.errors.emplace_back("theme animation token durations must be finite and non-negative"); break;
    }
    std::unordered_set<std::string> names;
    for (const UIStyleClassDefinition& style : definition.styleClasses) {
        if (style.name.empty()) result.errors.emplace_back("style class name must not be empty");
        else if (!names.insert(style.name).second) result.errors.emplace_back("duplicate style class: " + style.name);
        ValidateProperties(style.baseStyle, style.name, result.errors);
        ValidateTransitions(style.baseTransitions, style.name, result.errors);
        for (const UIStyleRuleDefinition& rule : style.rules) {
            if (((rule.requiredStates | rule.forbiddenStates) & ~kValidStates) != 0)
                result.errors.emplace_back(style.name + ": selector contains invalid interaction states");
            if ((rule.requiredStates & rule.forbiddenStates) != 0)
                result.errors.emplace_back(style.name + ": selector requires and forbids the same state");
            ValidateProperties(rule.properties, style.name, result.errors);
            ValidateTransitions(rule.transitions, style.name, result.errors);
        }
    }
    if (!result.errors.empty()) return result;

    std::uint32_t generation = g_NextThemeGeneration.fetch_add(1, std::memory_order_relaxed);
    if (generation == 0) generation = g_NextThemeGeneration.fetch_add(1, std::memory_order_relaxed);
    const UIThemeHandle handle{1, generation};
    const std::uint64_t revision = g_NextThemeRevision.fetch_add(1, std::memory_order_relaxed);
    std::vector<std::string> classNames;
    std::vector<UIResolvedStyleClass> classes;
    classNames.reserve(definition.styleClasses.size());
    classes.reserve(definition.styleClasses.size());
    for (std::size_t i = 0; i < definition.styleClasses.size(); ++i) {
        const UIStyleClassDefinition& input = definition.styleClasses[i];
        UIResolvedStyleClass output{input.baseStyle, input.baseTransitions, {}};
        const UIStyleClassId id{static_cast<std::uint32_t>(i + 1), generation};
        output.rules.reserve(input.rules.size());
        for (std::size_t j = 0; j < input.rules.size(); ++j) {
            const auto& rule = input.rules[j];
            output.rules.push_back({{id, rule.requiredStates, rule.forbiddenStates,
                static_cast<std::uint32_t>(j)}, rule.properties, rule.transitions});
        }
        std::stable_sort(output.rules.begin(), output.rules.end(), [](const UIStyleRule& lhs, const UIStyleRule& rhs) {
            const auto lhsRequired = std::popcount(lhs.selector.requiredStates);
            const auto rhsRequired = std::popcount(rhs.selector.requiredStates);
            if (lhsRequired != rhsRequired) return lhsRequired < rhsRequired;
            const auto lhsForbidden = std::popcount(lhs.selector.forbiddenStates);
            const auto rhsForbidden = std::popcount(rhs.selector.forbiddenStates);
            if (lhsForbidden != rhsForbidden) return lhsForbidden < rhsForbidden;
            return lhs.selector.declarationOrder < rhs.selector.declarationOrder;
        });
        classNames.push_back(input.name);
        classes.push_back(std::move(output));
    }
    result.theme = std::make_shared<const ResolvedUITheme>(handle, revision, definition.name,
        definition.tokens, std::move(classNames), std::move(classes), definition);
    result.success = true;
    return result;
}

} // namespace Engine::UI2D
