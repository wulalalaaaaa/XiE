#include "Platform/GLFW/GlfwApplicationHost.h"
#include "Platform/GLFW/GlfwOpenGLSurface.h"
#include "Platform/GLFW/GlfwWindowSystem.h"
#include "Renderer/OpenGLBackend.h"
#include "Renderer2D/Backends/OpenGL/OpenGL2DRenderer.h"
#include "Renderer2D/Backends/OpenGL/OpenGLWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/LayeredWindowPresenter.h"
#include "Renderer2D/Backends/Software/LayeredWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/Software2DRenderer.h"
#include "UI2D/Animation/UIAnimatedProperties.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Theme/BasicUIThemeResolver.h"
#include "UI2D/Widgets/Basic/UIButtonWidget.h"
#include "UI2D/Widgets/Basic/UIImageWidget.h"
#include "UI2D/Widgets/Basic/UIPanelWidget.h"
#include "UI2D/Widgets/Basic/UIProgressBarWidget.h"
#include "UI2D/Widgets/Basic/UITextWidget.h"
#include "UI2D/Widgets/Basic/UIToggleButtonWidget.h"

#ifdef _WIN32
#include "Platform/Win32/Win32ApplicationHost.h"
#include "Platform/Win32/Win32WindowSystem.h"
#include <objbase.h>
#ifdef CreateWindow
#undef CreateWindow
#endif
#endif

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace {
using namespace Engine;
using namespace Engine::UI2D;

UILayoutParams Fixed(float width, float height) {
    UILayoutParams value;
    value.sizeRule.width = {UISizeMode::Fixed, width};
    value.sizeRule.height = {UISizeMode::Fixed, height};
    return value;
}
UITransform At(float x, float y, float width, float height) {
    UITransform value;
    value.position = {x, y};
    value.size = {width, height};
    return value;
}

std::shared_ptr<const ResolvedUITheme> MakeWidgetTheme(std::string name, float tone) {
    UIThemeDefinition definition;
    definition.name = std::move(name);
    auto addClass = [&](std::string className) -> UIStyleClassDefinition& {
        definition.styleClasses.push_back({});
        definition.styleClasses.back().name = std::move(className);
        definition.styleClasses.back().baseStyle.colorMultiplier = Color4f{tone, tone, tone, 1};
        return definition.styleClasses.back();
    };
    addClass("Widget.Panel");
    auto& button = addClass("Widget.Button");
    button.baseTransitions.scale = {0.08, UIEasing::EaseOutQuad};
    button.baseTransitions.color = {0.12, UIEasing::EaseOutQuad};
    UIStyleRuleDefinition hover;
    hover.requiredStates = ToMask(UIInteractionState::Hovered);
    hover.properties.colorMultiplier = Color4f{1.12f, 1.12f, 1.12f, 1};
    button.rules.push_back(hover);
    UIStyleRuleDefinition pressed;
    pressed.requiredStates = ToMask(UIInteractionState::Pressed);
    pressed.properties.scaleMultiplier = Vec2F{0.94f, 0.94f};
    button.rules.push_back(pressed);
    UIStyleRuleDefinition focused;
    focused.requiredStates = ToMask(UIInteractionState::Focused);
    focused.properties.scaleMultiplier = Vec2F{1.04f, 1.04f};
    button.rules.push_back(focused);
    auto& toggle = addClass("Widget.Toggle");
    toggle.baseTransitions.color = {0.12, UIEasing::EaseOutQuad};
    UIStyleRuleDefinition checked;
    checked.requiredStates = ToMask(UIInteractionState::Checked);
    checked.properties.colorMultiplier = Color4f{0.58f, 1.18f, 0.78f, 1};
    toggle.rules.push_back(checked);
    addClass("Widget.Progress.Background");
    addClass("Widget.Progress.Fill");
    return BasicUIThemeResolver{}.Resolve(definition).theme;
}

struct WidgetSnapshot {
    int buttonActivations = 0;
    int toggleActivations = 0;
    bool checked = false;
    float progressAnchor = 0.0f;
    std::uint32_t widgetCount = 0;
};

struct WidgetWindow {
    BasicLayoutEngine layout;
    BasicUIHitTester hit;
    BasicUIInputRouter input;
    BasicUIRenderBuilder render;
    std::shared_ptr<const ResolvedUITheme> dark = MakeWidgetTheme("WidgetDark", 1.0f);
    std::shared_ptr<const ResolvedUITheme> light = MakeWidgetTheme("WidgetLight", 0.92f);
    UIWindowRuntime runtime;
    UIWidgetHandle root{}, header{}, image{}, button{}, buttonText{}, toggle{}, toggleText{}, progress{}, status{};
    UINodeHandle progressFill{};
    int buttonActivations = 0;
    int toggleActivations = 0;

    explicit WidgetWindow(WindowHandle window)
        : runtime(window, {layout, hit, input, render}, dark) {
        auto& widgets = runtime.Widgets();
        UIPanelWidgetDesc panel;
        panel.common.layout = Fixed(700, 420);
        panel.common.transform = At(20, 20, 700, 420);
        panel.common.styleClass = dark->FindStyleClass("Widget.Panel");
        panel.backgroundColor = {0.10f, 0.12f, 0.18f, 1};
        panel.cornerRadius = 18;
        panel.clipChildren = true;
        UIWidgetContext context = runtime.Widgets().Context();
        root = UIPanelWidget::Create(context, panel);

        UITextWidgetDesc text;
        text.common.layout = Fixed(620, 34);
        text.common.transform = At(30, 24, 620, 34);
        text.textLayout = {101, 1};
        text.color = {0.95f, 0.97f, 1, 1};
        header = widgets.CreateText(text, root);

        UIImageWidgetDesc imageDesc;
        imageDesc.common.layout = Fixed(96, 96);
        imageDesc.common.transform = At(30, 82, 96, 96);
        imageDesc.texture = {101, 1};
        imageDesc.fit = UIImageFit::Contain;
        imageDesc.tint = {0.55f, 0.75f, 1, 1};
        image = widgets.CreateImage(imageDesc, root);

        UIButtonWidgetDesc buttonDesc;
        buttonDesc.panel.common.layout = Fixed(190, 58);
        buttonDesc.panel.common.transform = At(160, 82, 190, 58);
        buttonDesc.panel.common.styleClass = dark->FindStyleClass("Widget.Button");
        buttonDesc.panel.backgroundColor = {0.22f, 0.48f, 0.92f, 1};
        buttonDesc.panel.cornerRadius = 12;
        button = widgets.CreateButton(buttonDesc, root);
        text.common.layout = Fixed(150, 26);
        text.common.transform = At(20, 16, 150, 26);
        text.textLayout = {102, 1};
        buttonText = widgets.CreateText(text, button);

        UIToggleButtonWidgetDesc toggleDesc;
        toggleDesc.button.panel.common.layout = Fixed(190, 58);
        toggleDesc.button.panel.common.transform = At(380, 82, 190, 58);
        toggleDesc.button.panel.common.styleClass = dark->FindStyleClass("Widget.Toggle");
        toggleDesc.button.panel.backgroundColor = {0.42f, 0.30f, 0.76f, 1};
        toggleDesc.button.panel.cornerRadius = 12;
        toggle = widgets.CreateToggleButton(toggleDesc, root);
        text.common.transform = At(20, 16, 150, 26);
        text.textLayout = {103, 1};
        toggleText = widgets.CreateText(text, toggle);

        UIProgressBarWidgetDesc progressDesc;
        progressDesc.common.layout = Fixed(540, 28);
        progressDesc.common.transform = At(30, 220, 540, 28);
        progressDesc.value = 0.25f;
        progressDesc.backgroundStyle = dark->FindStyleClass("Widget.Progress.Background");
        progressDesc.fillStyle = dark->FindStyleClass("Widget.Progress.Fill");
        progressDesc.fillColor = {0.22f, 0.78f, 0.52f, 1};
        progress = widgets.CreateProgressBar(progressDesc, root);
        progressFill = widgets.Registry().TryGet(progress)->progressFill;

        text.common.layout = Fixed(620, 32);
        text.common.transform = At(30, 286, 620, 32);
        text.textLayout = {104, 1};
        status = widgets.CreateText(text, root);

        widgets.OnActivated(button, [this](const UIButtonActivatedEvent&) {
            ++buttonActivations;
            runtime.Widgets().SetValue(progress, 0.25f + 0.15f * static_cast<float>(buttonActivations));
        });
        widgets.OnActivated(toggle, [this](const UIButtonActivatedEvent&) { ++toggleActivations; });
        runtime.Update(0.0, {740, 460}, 1.0f);
    }

    WidgetSnapshot Snapshot() const {
        const UINodeRecord* fill = runtime.Scene().TryGet(progressFill);
        return {buttonActivations, toggleActivations, runtime.Widgets().IsChecked(toggle),
            fill ? fill->layout.anchor.maxX : -1.0f,
            static_cast<std::uint32_t>(runtime.Widgets().Registry().Size())};
    }
};

PointerMoveEvent Move(WindowHandle window, Vec2F point) {
    PointerMoveEvent event; event.window = window; event.logicalPosition = point; return event;
}
PointerButtonEvent Pointer(WindowHandle window, Vec2F point, bool down) {
    PointerButtonEvent event; event.window = window; event.logicalPosition = point;
    event.button = PointerButton::Primary; event.pressed = down; return event;
}
KeyEvent Key(WindowHandle window, InputKeyCode key, bool down, bool shift = false) {
    KeyEvent event; event.window = window; event.key = key; event.pressed = down;
    event.modifiers = shift ? InputModifiers::Shift : InputModifiers::None; return event;
}

bool Exercise(WidgetWindow& ui, WindowHandle window, float dpiScale) {
    auto send = [&](InputEvent event, double delta = 0.02) {
        if (!ui.runtime.EnqueueInput(std::move(event))) return false;
        ui.runtime.Update(delta, {740, 460}, dpiScale);
        return true;
    };
    if (!send(Move(window, {220, 125})) || !send(Pointer(window, {220, 125}, true)) ||
        !send(Pointer(window, {220, 125}, false))) return false;
    if (ui.buttonActivations != 1) return false;
    ui.runtime.RequestFocus(ui.runtime.Widgets().Registry().TryGet(ui.button)->rootNode);
    ui.runtime.Update(0.02, {740, 460}, dpiScale);
    if (!send(Key(window, InputKeyCode::Enter, true))) return false;
    if (!send(Key(window, InputKeyCode::Space, true)) || !send(Key(window, InputKeyCode::Space, false))) return false;
    if (ui.buttonActivations != 3) return false;
    if (!send(Key(window, InputKeyCode::Tab, true))) return false;
    if (ui.runtime.FocusedNode() != ui.runtime.Widgets().Registry().TryGet(ui.toggle)->rootNode) return false;
    if (!send(Key(window, InputKeyCode::Space, true)) || !send(Key(window, InputKeyCode::Space, false))) return false;
    if (!ui.runtime.Widgets().IsChecked(ui.toggle) || ui.toggleActivations != 1) return false;

    UIAnimationDesc animation;
    animation.node = ui.runtime.Widgets().Registry().TryGet(ui.button)->rootNode;
    animation.property = UIAnimatedProperty::Position;
    animation.from = Vec2F{}; animation.to = Vec2F{3, 0};
    animation.durationSeconds = 0.08; animation.fillMode = UIAnimationFillMode::Forwards;
    ui.runtime.StartAnimation(animation);
    ui.runtime.SetTheme(ui.light, UIThemeSwitchMode::Animated);
    ui.runtime.Update(0.04, {740, 460}, dpiScale);
    ui.runtime.SetTheme(ui.dark, UIThemeSwitchMode::Immediate);
    ui.runtime.Update(0.10, {740, 460}, dpiScale);

    UIPanelWidgetDesc transient;
    transient.common.layout = Fixed(28, 28);
    transient.common.transform = At(630, 350, 28, 28);
    transient.backgroundColor = {0.95f, 0.56f, 0.18f, 1};
    const UIWidgetHandle dynamic = ui.runtime.Widgets().CreatePanel(transient, ui.root);
    ui.runtime.Update(0.0, {740, 460}, dpiScale);
    if (!dynamic.IsValid() || !ui.runtime.Widgets().Destroy(dynamic)) return false;
    ui.runtime.Update(0.0, {740, 460}, dpiScale);
    return !ui.runtime.Widgets().Registry().TryGet(dynamic) && !ui.runtime.DrawList().Empty();
}
} // namespace

int main(int argc, char** argv) {
    using namespace Engine;
    const bool selfTest = argc > 1 && std::string(argv[1]) == "--self-test";
    GlfwApplicationHost host;
    if (!host.IsInitialized()) {
        std::cerr << "GLFW initialization failed\n";
        return 1;
    }
    GlfwWindowSystem windows;
    WindowDesc desc; desc.title = "XiE UI2D Widgets OpenGL"; desc.width = 740; desc.height = 460;
    desc.visible = !selfTest;
    const WindowHandle window = windows.CreateWindow(desc);
    WidgetWindow gl(window); windows.SetInputEventQueue(window, &gl.runtime.InputQueue());
    if (!Exercise(gl, window, 1.0f)) return 2;
    GlfwOpenGLSurface surface(windows, window); surface.MakeCurrent();
    OpenGLBackend backend; if (!backend.Init()) return 3;
    const std::array<float, 16> projection = {2.0f/740,0,0,0, 0,-2.0f/460,0,0,
        0,0,-1,0, -1,1,0,1};
    backend.SetViewProjection(projection.data());
    OpenGL2DRenderer renderer; OpenGLWindowRenderPipeline pipeline(surface, backend, renderer);
    const auto glStats = pipeline.Render(gl.runtime.DrawList(), {740, 460, 1}); pipeline.Present();
    const WidgetSnapshot glSnapshot = gl.Snapshot();
    std::cout << "[OpenGL Widgets] activated=" << glSnapshot.buttonActivations
              << " toggle=" << glSnapshot.checked << " progress=" << glSnapshot.progressAnchor
              << " widgets=" << glSnapshot.widgetCount << " draws=" << glStats.drawCallCount << '\n';
    backend.Shutdown(); windows.SetInputEventQueue(window, nullptr); windows.DestroyWindow(window);
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Win32ApplicationHost winHost; Win32WindowSystem winWindows;
    WindowDesc winDesc; winDesc.title = "XiE UI2D Widgets Layered"; winDesc.width = 740;
    winDesc.height = 460; winDesc.visible = !selfTest; winDesc.transparent = true; winDesc.decorated = false;
    const WindowHandle winWindow = winWindows.CreateWindow(winDesc);
    WidgetWindow softwareUI(winWindow); winWindows.SetInputEventQueue(winWindow, &softwareUI.runtime.InputQueue());
    if (!Exercise(softwareUI, winWindow, 1.5f)) return 4;
    Software2DRenderer software; LayeredWindowPresenter presenter(winWindows.GetNativeHandle(winWindow));
    LayeredWindowRenderPipeline layered(software, presenter); layered.Resize(740, 460, 1.5f);
    const auto swStats = layered.Render(softwareUI.runtime.DrawList(), {740, 460, 1.5f}); layered.Present();
    const WidgetSnapshot swSnapshot = softwareUI.Snapshot();
    std::cout << "[Layered Widgets] activated=" << swSnapshot.buttonActivations
              << " toggle=" << swSnapshot.checked << " progress=" << swSnapshot.progressAnchor
              << " widgets=" << swSnapshot.widgetCount << " draws=" << swStats.drawCallCount << '\n';
    winWindows.SetInputEventQueue(winWindow, nullptr); winWindows.DestroyWindow(winWindow); CoUninitialize();
    if (glSnapshot.buttonActivations != swSnapshot.buttonActivations ||
        glSnapshot.toggleActivations != swSnapshot.toggleActivations ||
        glSnapshot.checked != swSnapshot.checked ||
        std::abs(glSnapshot.progressAnchor - swSnapshot.progressAnchor) > 0.0001f ||
        glSnapshot.widgetCount != swSnapshot.widgetCount) return 5;
#endif
    return glStats.drawCallCount > 0 ? 0 : 6;
}
