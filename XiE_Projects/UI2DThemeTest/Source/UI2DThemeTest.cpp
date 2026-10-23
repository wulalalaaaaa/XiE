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
    UILayoutParams value; value.sizeRule.width = {UISizeMode::Fixed, width};
    value.sizeRule.height = {UISizeMode::Fixed, height}; return value;
}
void Place(UIScene& scene, UINodeHandle node, float x, float y, float width, float height) {
    scene.SetLayout(node, Fixed(width, height));
    UITransform transform; transform.position = {x, y}; transform.size = {width, height};
    transform.pivot = {0.5f, 0.5f}; scene.SetTransform(node, transform);
}
struct ThemeWindow {
    BasicLayoutEngine layout; BasicUIHitTester hit; BasicUIInputRouter input; BasicUIRenderBuilder render;
    std::shared_ptr<const ResolvedUITheme> dark = MakeDefaultDarkResolvedUITheme();
    std::shared_ptr<const ResolvedUITheme> light = MakeDefaultLightResolvedUITheme();
    UIWindowRuntime runtime;
    UINodeHandle panel{}, itemA{}, itemB{}, image{}, ring{}, status{};
    explicit ThemeWindow(WindowHandle window) : runtime(window, {layout, hit, input, render}, dark) {
        UIScene& scene = runtime.Scene();
        panel = scene.CreateNode("Panel"); Place(scene, panel, 30, 30, 660, 380);
        scene.SetVisual(panel, UIPanelVisual{{0.12f, 0.14f, 0.20f, 1}, 16, true});
        itemA = scene.CreateNode("ItemA", panel); Place(scene, itemA, 50, 60, 150, 110);
        itemB = scene.CreateNode("ItemB", panel); Place(scene, itemB, 250, 60, 150, 110);
        image = scene.CreateNode("ItemC", panel); Place(scene, image, 450, 60, 130, 110);
        ring = scene.CreateNode("FocusRing", panel); Place(scene, ring, 70, 235, 110, 80);
        status = scene.CreateNode("StatusText", panel); Place(scene, status, 250, 235, 330, 80);
        scene.SetVisual(itemA, UIPanelVisual{{0.20f, 0.55f, 0.95f, 1}, 12, true});
        scene.SetVisual(itemB, UIPanelVisual{{0.55f, 0.34f, 0.92f, 1}, 12, true});
        scene.SetVisual(image, UIPanelVisual{{0.18f, 0.78f, 0.55f, 1}, 12, true});
        scene.SetVisual(ring, UIPanelVisual{{0.95f, 0.65f, 0.16f, 1}, 10, true});
        scene.SetVisual(status, UIPanelVisual{{0.16f, 0.19f, 0.28f, 1}, 10, true});
        scene.SetFocusProperties(itemA, {true, 0}); scene.SetFocusProperties(itemB, {true, 1});
        runtime.SetStyleClass(panel, dark->FindStyleClass("Panel"));
        runtime.SetStyleClass(itemA, dark->FindStyleClass("Interactive"));
        runtime.SetStyleClass(itemB, dark->FindStyleClass("Interactive"));
        runtime.SetStyleClass(image, dark->FindStyleClass("ImageItem"));
        runtime.SetStyleClass(ring, dark->FindStyleClass("FocusRing"));
        runtime.Update(0, {720, 440}, 1);
    }
};
bool Exercise(ThemeWindow& ui, WindowHandle window) {
    PointerMoveEvent move; move.window = window; move.logicalPosition = {120, 120};
    ui.runtime.EnqueueInput(move); ui.runtime.RequestFocus(ui.itemA); ui.runtime.Update(0.08, {720,440}, 1);
    UIAnimationDesc app; app.node = ui.itemA; app.property = UIAnimatedProperty::Scale;
    app.from = Vec2F{1,1}; app.to = Vec2F{1.25f,1.25f}; app.durationSeconds = 0.2;
    app.fillMode = UIAnimationFillMode::Forwards; ui.runtime.StartAnimation(app);
    ui.runtime.Update(0.2, {720,440}, 1);
    ui.runtime.SetTheme(ui.light, UIThemeSwitchMode::Animated);
    ui.runtime.Update(0.06, {720,440}, 1);
    ui.runtime.SetTheme(ui.dark, UIThemeSwitchMode::Immediate);
    ui.runtime.Update(0.2, {720,440}, 1);
    const UIResolvedAnimatedProperties resolved = ResolveAnimatedProperties(*ui.runtime.Scene().TryGet(ui.itemA));
    return resolved.scale.x > 1.25f && ui.runtime.Interaction().IsHovered(ui.itemA) &&
        ui.runtime.GetStyleClass(ui.itemA) == ui.dark->FindStyleClass("Interactive") &&
        ui.runtime.HitTest({120,120}).target == ui.itemA && !ui.runtime.DrawList().Empty();
}
} // namespace

int main(int argc, char** argv) {
    const bool selfTest = argc > 1 && std::string(argv[1]) == "--self-test";
    GlfwApplicationHost host; if (!host.IsInitialized()) return 1;
    GlfwWindowSystem windows; WindowDesc desc;
    desc.title = "XiE UI2D Theme OpenGL"; desc.width = 720; desc.height = 440; desc.visible = !selfTest;
    const WindowHandle window = windows.CreateWindow(desc);
    ThemeWindow gl(window); windows.SetInputEventQueue(window, &gl.runtime.InputQueue());
    if (!Exercise(gl, window)) return 2;
    GlfwOpenGLSurface surface(windows, window); surface.MakeCurrent();
    OpenGLBackend backend; if (!backend.Init()) return 3;
    const std::array<float,16> projection = {2.0f/720,0,0,0, 0,-2.0f/440,0,0, 0,0,-1,0, -1,1,0,1};
    backend.SetViewProjection(projection.data());
    OpenGL2DRenderer renderer; OpenGLWindowRenderPipeline pipeline(surface, backend, renderer);
    const auto glStats = pipeline.Render(gl.runtime.DrawList(), {720,440,1}); pipeline.Present();
    const auto glValue = ResolveAnimatedProperties(*gl.runtime.Scene().TryGet(gl.itemA));
    std::cout << "[OpenGL Theme] theme=" << gl.runtime.Theme()->Name() << " scale=" << glValue.scale.x
              << " style=" << gl.runtime.TryGetRuntimeStyle(gl.itemA)->scaleMultiplier.x
              << " draws=" << glStats.drawCallCount << '\n';
    backend.Shutdown(); windows.SetInputEventQueue(window, nullptr); windows.DestroyWindow(window);
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Win32ApplicationHost winHost; Win32WindowSystem winWindows; WindowDesc winDesc;
    winDesc.title = "XiE UI2D Theme Layered"; winDesc.width = 720; winDesc.height = 440;
    winDesc.visible = !selfTest; winDesc.transparent = true; winDesc.decorated = false;
    const WindowHandle winWindow = winWindows.CreateWindow(winDesc);
    ThemeWindow softwareUI(winWindow); winWindows.SetInputEventQueue(winWindow, &softwareUI.runtime.InputQueue());
    if (!Exercise(softwareUI, winWindow)) return 4;
    Software2DRenderer software; LayeredWindowPresenter presenter(winWindows.GetNativeHandle(winWindow));
    LayeredWindowRenderPipeline layered(software, presenter); layered.Resize(720,440,1);
    const auto swStats = layered.Render(softwareUI.runtime.DrawList(), {720,440,1}); layered.Present();
    const auto swValue = ResolveAnimatedProperties(*softwareUI.runtime.Scene().TryGet(softwareUI.itemA));
    std::cout << "[Layered Theme] theme=" << softwareUI.runtime.Theme()->Name() << " scale=" << swValue.scale.x
              << " style=" << softwareUI.runtime.TryGetRuntimeStyle(softwareUI.itemA)->scaleMultiplier.x
              << " draws=" << swStats.drawCallCount << '\n';
    winWindows.SetInputEventQueue(winWindow, nullptr); winWindows.DestroyWindow(winWindow); CoUninitialize();
    if (std::abs(glValue.scale.x - swValue.scale.x) > 0.0001f ||
        glValue.colorMultiplier != swValue.colorMultiplier) return 5;
#endif
    return glStats.drawCallCount > 0 ? 0 : 6;
}
