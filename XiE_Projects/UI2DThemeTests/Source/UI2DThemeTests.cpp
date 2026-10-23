#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Style/BasicUIInteractionStyleResolver.h"
#include "UI2D/Theme/BasicUIThemeResolver.h"
#include "UI2D/Theme/UIThemeRepository.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {
using namespace Engine;
using namespace Engine::UI2D;
int failures = 0;
void Expect(bool value, const char* message) {
    std::cout << (value ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!value) ++failures;
}
bool Near(float lhs, float rhs, float epsilon = 0.002f) { return std::abs(lhs - rhs) <= epsilon; }
UILayoutParams Fixed(float width, float height) {
    UILayoutParams value;
    value.sizeRule.width = {UISizeMode::Fixed, width};
    value.sizeRule.height = {UISizeMode::Fixed, height};
    return value;
}
UIThemeDefinition ThemeDefinition(std::string name, float baseScale = 1.0f, double duration = 0.2) {
    UIThemeDefinition theme;
    theme.name = std::move(name);
    UIStyleClassDefinition style{"Interactive"};
    style.baseStyle.scaleMultiplier = Vec2F{baseScale, baseScale};
    style.baseStyle.opacityMultiplier = 1.0f;
    style.baseStyle.colorMultiplier = Color4f{};
    style.baseTransitions.scale = {duration, UIEasing::Linear};
    style.baseTransitions.opacity = {duration, UIEasing::Linear};
    style.baseTransitions.color = {duration, UIEasing::Linear};
    UIStyleRuleDefinition hover;
    hover.requiredStates = ToMask(UIInteractionState::Hovered);
    hover.properties.scaleMultiplier = Vec2F{1.1f, 1.1f};
    hover.properties.colorMultiplier = Color4f{0.8f, 0.9f, 1.0f, 1.0f};
    hover.transitions = style.baseTransitions;
    UIStyleRuleDefinition pressed;
    pressed.requiredStates = ToMask(UIInteractionState::Pressed);
    pressed.properties.scaleMultiplier = Vec2F{0.9f, 0.9f};
    pressed.properties.opacityMultiplier = 0.7f;
    pressed.transitions = style.baseTransitions;
    UIStyleRuleDefinition combined;
    combined.requiredStates = UIInteractionState::Hovered | UIInteractionState::Focused;
    combined.properties.scaleMultiplier = Vec2F{1.2f, 1.2f};
    combined.transitions = style.baseTransitions;
    UIStyleRuleDefinition forbidden;
    forbidden.requiredStates = ToMask(UIInteractionState::Hovered);
    forbidden.forbiddenStates = ToMask(UIInteractionState::Pressed);
    forbidden.properties.opacityMultiplier = 0.8f;
    forbidden.transitions = style.baseTransitions;
    UIStyleRuleDefinition pressedHover;
    pressedHover.requiredStates = UIInteractionState::Hovered | UIInteractionState::Pressed;
    pressedHover.properties.scaleMultiplier = Vec2F{0.9f, 0.9f};
    pressedHover.properties.opacityMultiplier = 0.7f;
    pressedHover.transitions = style.baseTransitions;
    style.rules = {hover, pressed, combined, forbidden, pressedHover};
    theme.styleClasses.push_back(std::move(style));
    return theme;
}

void TestThemeResolveAndRepository() {
    BasicUIThemeResolver resolver;
    const UIThemeResolveResult one = resolver.Resolve(ThemeDefinition("One"));
    const UIThemeResolveResult two = resolver.Resolve(ThemeDefinition("Two"));
    Expect(one.Succeeded() && two.Succeeded(), "valid code-defined themes resolve");
    Expect(one.theme->Handle() != two.theme->Handle() && one.theme->Revision() != two.theme->Revision(),
        "theme handles and revisions are generational and unique");
    const UIStyleClassId oneClass = one.theme->FindStyleClass("Interactive");
    const UIStyleClassId twoClass = two.theme->FindStyleClass("Interactive");
    Expect(oneClass.IsValid() && oneClass != twoClass && !two.theme->TryGetStyleClass(oneClass),
        "style class IDs are resolved and isolated by theme generation");
    Expect(one.theme->StyleClassName(oneClass) == "Interactive", "resolved theme supports ID-to-name remapping");
    UIThemeRepository repository;
    const UIThemeHandle handle = repository.Add(one.theme);
    const bool removed = repository.Remove(handle);
    const UIThemeHandle reused = repository.Add(one.theme);
    Expect(removed && !repository.TryGet(handle) && repository.TryGet(reused) == one.theme &&
        reused.index == handle.index && reused.generation != handle.generation,
        "theme repository owns immutable shared themes by handle");

    UIThemeDefinition duplicate = ThemeDefinition("Duplicate");
    duplicate.styleClasses.push_back(duplicate.styleClasses.front());
    Expect(!resolver.Resolve(duplicate).Succeeded(), "duplicate style class names are rejected");
    UIThemeDefinition negative = ThemeDefinition("Negative");
    negative.styleClasses[0].baseTransitions.scale.durationSeconds = -1;
    Expect(!resolver.Resolve(negative).Succeeded(), "negative transition duration is rejected");
    UIThemeDefinition conflict = ThemeDefinition("Conflict");
    conflict.styleClasses[0].rules[0].forbiddenStates = conflict.styleClasses[0].rules[0].requiredStates;
    Expect(!resolver.Resolve(conflict).Succeeded(), "required and forbidden selector conflict is rejected");
    UIThemeDefinition badColor = ThemeDefinition("BadColor");
    badColor.tokens.colors.accent.r = std::numeric_limits<float>::quiet_NaN();
    Expect(!resolver.Resolve(badColor).Succeeded(), "non-finite theme colors are rejected");
}

void TestSelectorSpecificity() {
    const auto theme = BasicUIThemeResolver{}.Resolve(ThemeDefinition("Selectors")).theme;
    const UIStyleClassId id = theme->FindStyleClass("Interactive");
    const UIResolvedStyleClass& style = *theme->TryGetStyleClass(id);
    const UIResolvedStyle hover = ResolveNodeStyle(style, id, ToMask(UIInteractionState::Hovered));
    const UIResolvedStyle pressedHover = ResolveNodeStyle(style, id,
        UIInteractionState::Hovered | UIInteractionState::Pressed);
    const UIResolvedStyle focusedHover = ResolveNodeStyle(style, id,
        UIInteractionState::Hovered | UIInteractionState::Focused);
    Expect(Near(hover.opacityMultiplier, 0.8f), "matching forbidden selector applies when state is absent");
    Expect(Near(pressedHover.opacityMultiplier, 0.7f), "forbidden selector does not match when forbidden state exists");
    Expect(Near(focusedHover.scaleMultiplier.x, 1.2f), "higher state-count specificity overrides general hover");
    Expect(Near(focusedHover.opacityMultiplier, 0.8f), "unmentioned properties retain the previously merged value");
}

struct RuntimeFixture {
    BasicLayoutEngine layout;
    BasicUIHitTester hit;
    BasicUIInputRouter input;
    BasicUIRenderBuilder render;
    std::shared_ptr<const ResolvedUITheme> theme =
        BasicUIThemeResolver{}.Resolve(ThemeDefinition("Runtime")).theme;
    UIWindowRuntime runtime{{501, 1}, {layout, hit, input, render}, theme};
    UINodeHandle node = runtime.Scene().CreateNode("Interactive");
    RuntimeFixture() {
        runtime.Scene().SetLayout(node, Fixed(100, 80));
        UITransform transform;
        transform.position = {40, 30}; transform.size = {100, 80}; transform.pivot = {0.5f, 0.5f};
        runtime.Scene().SetTransform(node, transform);
        runtime.Scene().SetVisual(node, UIPanelVisual{{1, 1, 1, 1}});
        runtime.Scene().SetFocusProperties(node, {true, 0});
        runtime.SetStyleClass(node, theme->FindStyleClass("Interactive"));
        runtime.Update(0, {320, 240}, 1);
    }
    void Move(float x, float y, double delta = 0.0) {
        PointerMoveEvent event; event.window = {501, 1}; event.logicalPosition = {x, y};
        runtime.EnqueueInput(event); runtime.Update(delta, {320, 240}, 1);
    }
    void Button(bool down, double delta = 0.0) {
        PointerButtonEvent event; event.window = {501, 1}; event.logicalPosition = {70, 60};
        event.button = PointerButton::Primary; event.pressed = down;
        runtime.EnqueueInput(event); runtime.Update(delta, {320, 240}, 1);
    }
};

void TestRuntimeStateTransitionsAndComposition() {
    RuntimeFixture f;
    Expect(Near(f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x, 1.0f),
        "initial base style is applied immediately");
    const UI2DUpdateResult stable = f.runtime.Update(0, {320, 240}, 1);
    Expect(stable.style.checkedNodeCount == 0 && !stable.drawListRebuilt,
        "stable state takes the empty dirty-queue fast path and stays static");

    const std::uint64_t interactionRevision = f.runtime.Interaction().Revision();
    f.Move(70, 60, 0.1);
    const auto* halfway = f.runtime.TryGetRuntimeStyle(f.node);
    Expect(halfway->scaleMultiplier.x > 1.0f && halfway->scaleMultiplier.x < 1.1f &&
        f.runtime.Interaction().IsHovered(f.node) &&
        f.runtime.Interaction().Revision() != interactionRevision,
        "hover starts InteractionStyle animation and advances snapshot revision in the same frame");
    f.Move(250, 200, 0.05);
    Expect(f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x < 1.075f,
        "rapid hover leave replaces from current without a jump");

    f.Move(70, 60, 0.2);
    f.Button(true, 0.1);
    const UIInteractionStateMask pressedMask = ResolveInteractionState(
        f.runtime.Scene(), f.runtime.Interaction(), f.node);
    Expect((pressedMask & ToMask(UIInteractionState::Pressed)) != 0 &&
        (pressedMask & ToMask(UIInteractionState::Captured)) != 0 &&
        (pressedMask & ToMask(UIInteractionState::Focused)) != 0,
        "pressed, captured, and focused states derive from interaction services");
    Expect(f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x < 1.1f,
        "pressed style replaces hover only inside InteractionStyle channel");
    f.Button(false, 0.2);
    f.Move(250, 200, 0.2);

    UIAnimationDesc application;
    application.node = f.node; application.property = UIAnimatedProperty::Scale;
    application.from = Vec2F{1, 1}; application.to = Vec2F{1.5f, 1.5f};
    application.durationSeconds = 0.2; application.fillMode = UIAnimationFillMode::Forwards;
    const auto app = f.runtime.StartAnimation(application);
    f.Move(70, 60, 0.1);
    const UINodeRecord& record = *f.runtime.Scene().TryGet(f.node);
    const Vec2F appScale = std::get<Vec2F>(ResolveAnimationChannelProperty(
        record, UIAnimatedProperty::Scale, UIAnimationChannel::Application));
    const float styleScale = f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x;
    Expect(app.success && Near(ResolveAnimatedProperties(record).scale.x, appScale.x * styleScale),
        "Application and InteractionStyle scale channels coexist and multiply to final scale");
    const float styleBeforeCancel = f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x;
    const std::uint32_t appCanceled = f.runtime.CancelAllAnimations(UIAnimationCancelMode::KeepCurrent);
    Expect(appCanceled == 1 && f.runtime.QueryFrameActivity().hasActiveAnimation &&
        Near(f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x, styleBeforeCancel),
        "canceling Application animations leaves InteractionStyle animation active");
    f.runtime.Update(0.1, {320, 240}, 1);
    Expect(Near(f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x, 1.2f),
        "InteractionStyle channel completes after Application cancellation");

    f.runtime.Scene().SetEnabled(f.node, false);
    f.runtime.Update(0.2, {320, 240}, 1);
    Expect((ResolveInteractionState(f.runtime.Scene(), f.runtime.Interaction(), f.node) &
        ToMask(UIInteractionState::Disabled)) != 0, "disabled state uses effective enabled ancestry");

    WindowFocusEvent inactive; inactive.window = {501, 1}; inactive.focused = false;
    f.runtime.EnqueueInput(inactive); f.runtime.Update(0, {320, 240}, 1);
    Expect((ResolveInteractionState(f.runtime.Scene(), f.runtime.Interaction(), f.node) &
        ToMask(UIInteractionState::WindowInactive)) != 0,
        "window inactive state is isolated in the per-window focus manager");
}

void TestThemeSwitchAndGeometry() {
    RuntimeFixture f;
    const auto alternate = BasicUIThemeResolver{}.Resolve(ThemeDefinition("Alternate", 0.8f, 0.4)).theme;
    f.runtime.SetTheme(alternate, UIThemeSwitchMode::Immediate);
    f.runtime.Update(0, {320, 240}, 1);
    Expect(f.runtime.GetStyleClass(f.node) == alternate->FindStyleClass("Interactive") &&
        Near(f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x, 0.8f),
        "Immediate theme switch remaps class by name and writes target immediately");

    const auto animated = BasicUIThemeResolver{}.Resolve(ThemeDefinition("Animated", 1.0f, 0.4)).theme;
    f.runtime.SetTheme(animated, UIThemeSwitchMode::Animated);
    f.runtime.Update(0.2, {320, 240}, 1);
    Expect(f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x > 0.8f &&
        f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x < 1.0f,
        "Animated theme switch transitions from current runtime style");
    f.runtime.Update(0.2, {320, 240}, 1);
    Expect(Near(f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x, 1.0f) &&
        !f.runtime.QueryFrameActivity().hasActiveAnimation,
        "style animation completes, retains target, and returns scheduler to static");

    f.runtime.SetVisible(false);
    f.runtime.SetTheme(alternate, UIThemeSwitchMode::Animated);
    const UI2DUpdateResult hidden = f.runtime.Update(0.2, {320, 240}, 1);
    Expect(hidden.style.checkedNodeCount == 0 &&
        Near(f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x, 1.0f),
        "hidden window neither resolves styles nor advances theme-switch animation");
    f.runtime.SetVisible(true);
    f.runtime.Update(0, {320, 240}, 1);
    Expect(Near(f.runtime.TryGetRuntimeStyle(f.node)->scaleMultiplier.x, 0.8f),
        "showing a window resolves the current theme target before rendering");

    UINodeRecord* node = f.runtime.Scene().TryGet(f.node);
    node->style.runtimeProperties.positionOffset = {60, 0};
    node->style.runtimeProperties.scaleMultiplier = {1.5f, 1.5f};
    f.runtime.Scene().MarkDirty(f.node,
        UIDirtyFlags::Transform | UIDirtyFlags::Visual | UIDirtyFlags::HitTest);
    f.runtime.Update(0, {320, 240}, 1);
    Expect(f.runtime.HitTest({170, 60}).target == f.node && !f.runtime.DrawList().Empty(),
        "style position/scale feed the same final transform used by HitTest and RenderBuilder");
}
} // namespace

int main() {
    TestThemeResolveAndRepository();
    TestSelectorSpecificity();
    TestRuntimeStateTransitionsAndComposition();
    TestThemeSwitchAndGeometry();
    if (failures) { std::cerr << failures << " theme tests failed\n"; return 1; }
    std::cout << "UI2D theme tests passed\n";
    return 0;
}
