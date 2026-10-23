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
#include <string>

namespace {
using namespace Engine;
using namespace Engine::UI2D;

UILayoutParams Fixed(float width, float height) {
    UILayoutParams layout;
    layout.sizeRule.width = {UISizeMode::Fixed, width};
    layout.sizeRule.height = {UISizeMode::Fixed, height};
    return layout;
}
void Place(UIScene& scene, UINodeHandle node, float x, float y, float w, float h) {
    scene.SetLayout(node, Fixed(w, h));
    UITransform transform = scene.TryGet(node)->transform;
    transform.position = {x, y}; transform.size = {w, h}; transform.pivot = {0.5f, 0.5f};
    scene.SetTransform(node, transform);
}
UIAnimationDesc VecAnim(UINodeHandle node, UIAnimatedProperty property, Vec2F to, double duration = 0.2) {
    UIAnimationDesc desc; desc.node = node; desc.property = property;
    desc.from = Vec2F{}; desc.to = to; desc.durationSeconds = duration;
    desc.fromMode = UIAnimationFromMode::Current; desc.fillMode = UIAnimationFillMode::Forwards;
    return desc;
}
UIAnimationDesc ScalarAnim(UINodeHandle node, UIAnimatedProperty property, float to, double duration = 0.2) {
    UIAnimationDesc desc; desc.node = node; desc.property = property;
    desc.from = 0.0f; desc.to = to; desc.durationSeconds = duration;
    desc.fromMode = UIAnimationFromMode::Current; desc.fillMode = UIAnimationFillMode::Forwards;
    return desc;
}
UIAnimationDesc ColorAnim(UINodeHandle node, Color4f to) {
    UIAnimationDesc desc; desc.node = node; desc.property = UIAnimatedProperty::ColorMultiplier;
    desc.from = Color4f{}; desc.to = to; desc.durationSeconds = 0.2;
    desc.fromMode = UIAnimationFromMode::Current; desc.fillMode = UIAnimationFillMode::Forwards;
    return desc;
}

struct AnimationWindow {
    BasicLayoutEngine layout;
    BasicUIHitTester hit;
    BasicUIInputRouter router;
    BasicUIRenderBuilder builder;
    UIWindowRuntime runtime;
    UINodeHandle panel{}, image{}, circle{}, ring{}, status{};

    explicit AnimationWindow(WindowHandle window)
        : runtime(window, {layout, hit, router, builder}, nullptr) {
        UIScene& scene = runtime.Scene();
        const UINodeHandle background = scene.CreateNode("Background");
        Place(scene, background, 0, 0, 720, 440);
        scene.SetVisual(background, UIPanelVisual{{0.035f,0.045f,0.075f,1}});
        panel = scene.CreateNode("AnimatedPanel"); Place(scene, panel, 70, 80, 180, 130);
        scene.SetVisual(panel, UIPanelVisual{{0.14f,0.48f,0.92f,1}, 16, true});
        scene.SetFocusProperties(panel, {true, 0});
        image = scene.CreateNode("AnimatedImage"); Place(scene, image, 300, 80, 130, 130);
        scene.SetVisual(image, UIPanelVisual{{0.58f,0.32f,0.9f,1}, 14, true});
        circle = scene.CreateNode("AnimatedCircle"); Place(scene, circle, 485, 82, 120, 120);
        UIShapeVisual circleVisual; circleVisual.type = UIShapeType::Circle;
        circleVisual.color = {0.12f,0.85f,0.52f,1}; scene.SetVisual(circle, circleVisual);
        ring = scene.CreateNode("AnimatedRing"); Place(scene, ring, 120, 255, 110, 110);
        UIShapeVisual ringVisual; ringVisual.type = UIShapeType::Ring;
        ringVisual.color = {0.98f,0.68f,0.12f,1}; ringVisual.innerRadius = 32; ringVisual.outerRadius = 50;
        scene.SetVisual(ring, ringVisual);
        status = scene.CreateNode("StatusText"); Place(scene, status, 300, 270, 330, 78);
        scene.SetVisual(status, UIPanelVisual{{0.12f,0.16f,0.25f,1}, 10, true});

        runtime.AddEventListener(panel, UIEventType::PointerEnter, UIEventPhaseMask::Target,
            [this](UIEventContext&) { runtime.StartAnimation(VecAnim(panel, UIAnimatedProperty::Scale, {1.1f,1.1f})); });
        runtime.AddEventListener(panel, UIEventType::PointerLeave, UIEventPhaseMask::Target,
            [this](UIEventContext&) { runtime.StartAnimation(VecAnim(panel, UIAnimatedProperty::Scale, {1,1})); });
        runtime.AddEventListener(panel, UIEventType::PointerDown, UIEventPhaseMask::Target,
            [this](UIEventContext&) { runtime.StartAnimation(ScalarAnim(panel, UIAnimatedProperty::Opacity, 0.65f)); });
        runtime.AddEventListener(panel, UIEventType::PointerUp, UIEventPhaseMask::Target,
            [this](UIEventContext&) { runtime.StartAnimation(ScalarAnim(panel, UIAnimatedProperty::Opacity, 1.0f)); });
        runtime.AddEventListener(panel, UIEventType::FocusGained, UIEventPhaseMask::Target,
            [this](UIEventContext&) { runtime.StartAnimation(ColorAnim(panel, {1,0.78f,0.42f,1})); });
        runtime.AddEventListener(panel, UIEventType::FocusLost, UIEventPhaseMask::Target,
            [this](UIEventContext&) { runtime.StartAnimation(ColorAnim(panel, {})); });
        runtime.AddEventListener(panel, UIEventType::Click, UIEventPhaseMask::Target,
            [this](UIEventContext&) {
                const float current = ResolveAnimatedProperties(*runtime.Scene().TryGet(panel)).rotationRadians;
                runtime.StartAnimation(ScalarAnim(panel, UIAnimatedProperty::Rotation,
                    current + 6.283185307f, 0.6));
            });
        runtime.Update(0, {720,440}, 1);
    }
};

bool Exercise(AnimationWindow& ui, WindowHandle window) {
    auto update = [&](double delta) { ui.runtime.Update(delta, {720,440}, 1); };
    ui.runtime.StartAnimation(VecAnim(ui.image, UIAnimatedProperty::Position, {330,220}, 0.8));
    UIAnimationDesc pulse = VecAnim(ui.ring, UIAnimatedProperty::VisualSize, {145,145}, 0.35);
    pulse.direction = UIAnimationDirection::Alternate; pulse.iterationCount = 4;
    ui.runtime.StartAnimation(pulse);
    PointerMoveEvent move; move.window = window; move.logicalPosition = {140,130};
    ui.runtime.EnqueueInput(move); update(0.1);
    PointerButtonEvent down; down.window = window; down.logicalPosition = {140,130};
    down.button = PointerButton::Primary; down.pressed = true;
    ui.runtime.EnqueueInput(down); update(0.1);
    down.pressed = false; ui.runtime.EnqueueInput(down); update(0.1);
    ui.runtime.RequestFocus(ui.panel); update(0.1);
    for (int i = 0; i < 8; ++i) update(0.1);
    const UIResolvedAnimatedProperties entered = ResolveAnimatedProperties(*ui.runtime.Scene().TryGet(ui.panel));
    const UIResolvedAnimatedProperties image = ResolveAnimatedProperties(*ui.runtime.Scene().TryGet(ui.image));
    const bool enterState = entered.scale.x > 1.09f && entered.opacity > 0.99f &&
        entered.rotationRadians > 5.0f && entered.colorMultiplier.g < 0.9f && image.position.y > 200.0f;

    move.logicalPosition = {680,400};
    ui.runtime.EnqueueInput(move);
    ui.runtime.ClearFocus();
    update(0.2);
    const UIResolvedAnimatedProperties left = ResolveAnimatedProperties(*ui.runtime.Scene().TryGet(ui.panel));
    const bool leaveState = left.scale.x < 1.01f && left.colorMultiplier == Color4f{};

    move.logicalPosition = {140,130};
    ui.runtime.EnqueueInput(move);
    ui.runtime.RequestFocus(ui.panel);
    update(0.2);
    return enterState && leaveState &&
        ResolveAnimatedProperties(*ui.runtime.Scene().TryGet(ui.panel)).scale.x > 1.09f &&
        !ui.runtime.DrawList().Empty();
}

} // namespace

int main(int argc, char** argv) {
    const bool selfTest = argc > 1 && std::string(argv[1]) == "--self-test";
    GlfwApplicationHost host; if (!host.IsInitialized()) return 1;
    GlfwWindowSystem windows;
    WindowDesc desc; desc.title = "XiE UI2D Animator OpenGL"; desc.width = 720; desc.height = 440;
    desc.visible = !selfTest;
    const WindowHandle window = windows.CreateWindow(desc);
    AnimationWindow gl(window); windows.SetInputEventQueue(window, &gl.runtime.InputQueue());
    if (!Exercise(gl, window)) return 2;
    GlfwOpenGLSurface surface(windows, window); surface.MakeCurrent();
    OpenGLBackend backend; if (!backend.Init()) return 3;
    const std::array<float,16> viewProjection = {2.0f/720,0,0,0, 0,-2.0f/440,0,0,
        0,0,-1,0, -1,1,0,1};
    backend.SetViewProjection(viewProjection.data());
    OpenGL2DRenderer renderer; OpenGLWindowRenderPipeline pipeline(surface, backend, renderer);
    const auto glStats = pipeline.Render(gl.runtime.DrawList(), {720,440,1}); pipeline.Present();
    const UIResolvedAnimatedProperties glPanel = ResolveAnimatedProperties(*gl.runtime.Scene().TryGet(gl.panel));
    std::cout << "[OpenGL Animator] active=" << gl.runtime.ActiveAnimationCount()
              << " scale=" << glPanel.scale.x << " opacity=" << glPanel.opacity
              << " rotation=" << glPanel.rotationRadians << " draws=" << glStats.drawCallCount << '\n';
    backend.Shutdown(); windows.SetInputEventQueue(window, nullptr); windows.DestroyWindow(window);

#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Win32ApplicationHost winHost; Win32WindowSystem winWindows;
    WindowDesc winDesc; winDesc.title = "XiE UI2D Animator Layered"; winDesc.width = 720; winDesc.height = 440;
    winDesc.visible = !selfTest; winDesc.transparent = true; winDesc.decorated = false;
    const WindowHandle winWindow = winWindows.CreateWindow(winDesc);
    AnimationWindow softwareUI(winWindow); winWindows.SetInputEventQueue(winWindow, &softwareUI.runtime.InputQueue());
    if (!Exercise(softwareUI, winWindow)) return 4;
    Software2DRenderer software; LayeredWindowPresenter presenter(winWindows.GetNativeHandle(winWindow));
    LayeredWindowRenderPipeline layered(software, presenter); layered.Resize(720,440,1);
    const auto swStats = layered.Render(softwareUI.runtime.DrawList(), {720,440,1}); layered.Present();
    const UIResolvedAnimatedProperties swPanel = ResolveAnimatedProperties(*softwareUI.runtime.Scene().TryGet(softwareUI.panel));
    std::cout << "[Layered Animator] active=" << softwareUI.runtime.ActiveAnimationCount()
              << " scale=" << swPanel.scale.x << " opacity=" << swPanel.opacity
              << " rotation=" << swPanel.rotationRadians << " draws=" << swStats.drawCallCount << '\n';
    winWindows.SetInputEventQueue(winWindow, nullptr); winWindows.DestroyWindow(winWindow); CoUninitialize();
    if (std::abs(glPanel.scale.x-swPanel.scale.x) > 0.0001f ||
        std::abs(glPanel.opacity-swPanel.opacity) > 0.0001f ||
        std::abs(glPanel.rotationRadians-swPanel.rotationRadians) > 0.0001f) return 5;
#endif
    return glStats.drawCallCount > 0 ? 0 : 6;
}
