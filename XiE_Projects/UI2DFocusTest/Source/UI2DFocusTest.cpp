#include "Platform/GLFW/GlfwApplicationHost.h"
#include "Platform/GLFW/GlfwOpenGLSurface.h"
#include "Platform/GLFW/GlfwWindowSystem.h"
#include "Renderer/OpenGLBackend.h"
#include "Renderer2D/Backends/OpenGL/OpenGL2DRenderer.h"
#include "Renderer2D/Backends/OpenGL/OpenGLWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/LayeredWindowPresenter.h"
#include "Renderer2D/Backends/Software/LayeredWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/Software2DRenderer.h"
#include "UI2D/Animation/IUIAnimator.h"
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
    transform.position = {x, y};
    scene.SetTransform(node, transform);
}

struct FocusWindow {
    BasicLayoutEngine layout;
    BasicUIHitTester hit;
    BasicUIInputRouter router;
    NullUIAnimator animator;
    BasicUIRenderBuilder builder;
    UIWindowRuntime runtime;
    UINodeHandle background{}, top{}, buttonA{}, buttonB{}, decorative{};
    UINodeHandle center{}, circle{}, ring{}, programmatic{}, status{};
    int gained = 0;
    int lost = 0;

    explicit FocusWindow(WindowHandle window)
        : runtime(window, {layout, hit, router, builder}, nullptr) {
        UIScene& scene = runtime.Scene();
        background = scene.CreateNode("Background");
        Place(scene, background, 0, 0, 640, 420);
        scene.SetVisual(background, UIPanelVisual{{0.035f, 0.045f, 0.075f, 1.0f}});

        top = scene.CreateNode("TopBar");
        Place(scene, top, 20, 18, 600, 74);
        scene.SetVisual(top, UIPanelVisual{{0.09f, 0.11f, 0.18f, 1.0f}, 10, true});
        buttonA = scene.CreateNode("ButtonA", top); Place(scene, buttonA, 18, 15, 130, 44);
        buttonB = scene.CreateNode("ButtonB", top); Place(scene, buttonB, 166, 15, 130, 44);
        decorative = scene.CreateNode("DecorativeImage", top); Place(scene, decorative, 480, 15, 80, 44);
        scene.SetVisual(buttonA, UIPanelVisual{{0.16f, 0.48f, 0.9f, 1}, 8, true});
        scene.SetVisual(buttonB, UIPanelVisual{{0.52f, 0.28f, 0.82f, 1}, 8, true});
        scene.SetVisual(decorative, UIPanelVisual{{0.24f, 0.28f, 0.38f, 1}, 8, true});
        scene.SetFocusProperties(buttonA, {true, 0});
        scene.SetFocusProperties(buttonB, {true, 0});

        center = scene.CreateNode("CenterPanel"); Place(scene, center, 70, 112, 500, 220);
        scene.SetVisual(center, UIPanelVisual{{0.1f, 0.13f, 0.2f, 1}, 12, true});
        circle = scene.CreateNode("CircleNode", center); Place(scene, circle, 40, 55, 90, 90);
        ring = scene.CreateNode("RingNode", center); Place(scene, ring, 180, 55, 90, 90);
        programmatic = scene.CreateNode("ProgrammaticOnly", center); Place(scene, programmatic, 330, 55, 120, 90);
        UIShapeVisual circleVisual; circleVisual.type = UIShapeType::Circle;
        circleVisual.color = {0.12f, 0.82f, 0.5f, 1}; scene.SetVisual(circle, circleVisual);
        UIShapeVisual ringVisual; ringVisual.type = UIShapeType::Ring;
        ringVisual.color = {0.95f, 0.68f, 0.12f, 1}; ringVisual.innerRadius = 24;
        ringVisual.outerRadius = 42; scene.SetVisual(ring, ringVisual);
        scene.SetVisual(programmatic, UIPanelVisual{{0.82f, 0.22f, 0.24f, 1}, 10, true});
        scene.SetFocusProperties(circle, {true, 10});
        scene.SetFocusProperties(ring, {true, 20});
        scene.SetFocusProperties(programmatic, {true, -1});

        status = scene.CreateNode("StatusText"); Place(scene, status, 70, 354, 500, 44);
        scene.SetVisual(status, UIPanelVisual{{0.14f, 0.17f, 0.25f, 1}, 7, true});

        for (UINodeHandle node : {buttonA, buttonB, circle, ring, programmatic}) {
            runtime.AddEventListener(node, UIEventType::FocusGained, UIEventPhaseMask::Target,
                [this, node](UIEventContext& context) {
                    ++gained;
                    context.EnqueueMutation(SetVisualMutation{
                        status, UIPanelVisual{{0.16f, 0.52f, 0.34f, 1}, 7, true}});
                    context.EnqueueMutation(SetVisualMutation{
                        node, UIPanelVisual{{0.92f, 0.72f, 0.18f, 1}, 9, true}});
                });
            runtime.AddEventListener(node, UIEventType::FocusLost, UIEventPhaseMask::Target,
                [this](UIEventContext& context) {
                    ++lost;
                    context.EnqueueMutation(SetVisualMutation{
                        status, UIPanelVisual{{0.38f, 0.18f, 0.22f, 1}, 7, true}});
                });
        }
        runtime.Update(0.0, {640, 420}, 1.0f);
    }
};

PointerButtonEvent Pointer(WindowHandle window, Vec2F point, bool down) {
    PointerButtonEvent event;
    event.window = window; event.logicalPosition = point;
    event.button = PointerButton::Primary; event.pressed = down;
    return event;
}
KeyEvent TabKey(WindowHandle window, bool shift = false) {
    KeyEvent event;
    event.window = window; event.key = InputKeyCode::Tab; event.pressed = true;
    event.modifiers = shift ? InputModifiers::Shift : InputModifiers::None;
    return event;
}

bool Exercise(FocusWindow& ui, WindowHandle window) {
    auto send = [&](InputEvent event) {
        if (!ui.runtime.EnqueueInput(std::move(event))) return false;
        ui.runtime.Update(0.0, {640, 420}, 1.0f);
        return true;
    };
    if (!send(Pointer(window, {55, 55}, true)) || !send(Pointer(window, {55, 55}, false))) return false;
    if (ui.runtime.FocusedNode() != ui.buttonA) return false;
    if (!send(TabKey(window)) || ui.runtime.FocusedNode() != ui.buttonB) return false;
    if (!send(TabKey(window, true)) || ui.runtime.FocusedNode() != ui.buttonA) return false;

    ui.runtime.RequestFocus(ui.programmatic);
    ui.runtime.Update(0.0, {640, 420}, 1.0f);
    if (ui.runtime.FocusedNode() != ui.programmatic) return false;

    ui.runtime.RequestFocus(ui.buttonB); ui.runtime.Update(0.0, {640, 420}, 1.0f);
    ui.runtime.Scene().DestroyNode(ui.buttonB); ui.runtime.Update(0.0, {640, 420}, 1.0f);
    if (ui.runtime.FocusedNode() == ui.buttonB) return false;

    ui.runtime.RequestFocus(ui.circle); ui.runtime.Update(0.0, {640, 420}, 1.0f);
    ui.runtime.Scene().SetVisibility(ui.circle, false); ui.runtime.Update(0.0, {640, 420}, 1.0f);
    if (ui.runtime.FocusedNode() == ui.circle) return false;
    ui.runtime.Scene().SetVisibility(ui.circle, true);

    ui.runtime.RequestFocus(ui.ring); ui.runtime.Update(0.0, {640, 420}, 1.0f);
    ui.runtime.Scene().SetEnabled(ui.ring, false); ui.runtime.Update(0.0, {640, 420}, 1.0f);
    if (ui.runtime.FocusedNode() == ui.ring) return false;

    ui.runtime.RequestFocus(ui.buttonA); ui.runtime.Update(0.0, {640, 420}, 1.0f);
    ui.runtime.Scene().ReparentNode(ui.buttonA, ui.center); ui.runtime.Update(0.0, {640, 420}, 1.0f);
    if (ui.runtime.FocusedNode() != ui.buttonA || !ui.runtime.HasFocusWithin(ui.center)) return false;

    if (!send(WindowFocusEvent{window, false}) || ui.runtime.FocusedNode().IsValid()) return false;
    if (!send(WindowFocusEvent{window, true}) || ui.runtime.FocusedNode() != ui.buttonA) return false;
    ui.runtime.SetVisible(false);
    ui.runtime.SetVisible(true);
    if (!send(WindowFocusEvent{window, true}) || ui.runtime.FocusedNode().IsValid()) return false;
    ui.runtime.RequestFocus(ui.buttonA); ui.runtime.Update(0.0, {640, 420}, 1.0f);
    (void)ui.runtime.Interaction().IsFocused(ui.buttonA);
    return ui.gained >= 7 && ui.lost >= 6;
}
} // namespace

int main(int argc, char** argv) {
    const bool selfTest = argc > 1 && std::string(argv[1]) == "--self-test";
    GlfwApplicationHost host;
    if (!host.IsInitialized()) return 1;
    GlfwWindowSystem windows;
    WindowDesc desc; desc.title = "XiE UI2D Focus OpenGL"; desc.width = 640; desc.height = 420;
    desc.visible = !selfTest;
    const WindowHandle window = windows.CreateWindow(desc);
    FocusWindow gl(window);
    windows.SetInputEventQueue(window, &gl.runtime.InputQueue());
    if (!Exercise(gl, window)) return 2;
    GlfwOpenGLSurface surface(windows, window); surface.MakeCurrent();
    OpenGLBackend backend; if (!backend.Init()) return 3;
    const std::array<float, 16> viewProjection = {
        2.0f / 640, 0, 0, 0, 0, -2.0f / 420, 0, 0,
        0, 0, -1, 0, -1, 1, 0, 1};
    backend.SetViewProjection(viewProjection.data());
    OpenGL2DRenderer renderer;
    OpenGLWindowRenderPipeline pipeline(surface, backend, renderer);
    const auto glStats = pipeline.Render(gl.runtime.DrawList(), {640, 420, 1});
    pipeline.Present();
    std::cout << "[OpenGL Focus] gained=" << gl.gained << " lost=" << gl.lost
              << " draws=" << glStats.drawCallCount << '\n';
    backend.Shutdown(); windows.SetInputEventQueue(window, nullptr); windows.DestroyWindow(window);

#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Win32ApplicationHost winHost; Win32WindowSystem winWindows;
    WindowDesc winDesc; winDesc.title = "XiE UI2D Focus Layered";
    winDesc.width = 640; winDesc.height = 420; winDesc.visible = !selfTest;
    winDesc.transparent = true; winDesc.decorated = false;
    const WindowHandle winWindow = winWindows.CreateWindow(winDesc);
    FocusWindow softwareUI(winWindow);
    winWindows.SetInputEventQueue(winWindow, &softwareUI.runtime.InputQueue());
    if (!Exercise(softwareUI, winWindow)) return 4;
    Software2DRenderer software;
    LayeredWindowPresenter presenter(winWindows.GetNativeHandle(winWindow));
    LayeredWindowRenderPipeline layered(software, presenter);
    layered.Resize(640, 420, 1);
    const auto softwareStats = layered.Render(softwareUI.runtime.DrawList(), {640, 420, 1});
    layered.Present();
    std::cout << "[Layered Focus] gained=" << softwareUI.gained << " lost=" << softwareUI.lost
              << " draws=" << softwareStats.drawCallCount << '\n';
    winWindows.SetInputEventQueue(winWindow, nullptr); winWindows.DestroyWindow(winWindow);
    CoUninitialize();
    if (gl.gained != softwareUI.gained || gl.lost != softwareUI.lost) return 5;
#endif
    return glStats.drawCallCount > 0 ? 0 : 6;
}
