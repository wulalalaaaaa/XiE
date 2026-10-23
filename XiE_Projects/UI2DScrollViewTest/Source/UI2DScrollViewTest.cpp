#include "Platform/GLFW/GlfwApplicationHost.h"
#include "Platform/GLFW/GlfwOpenGLSurface.h"
#include "Platform/GLFW/GlfwWindowSystem.h"
#include "Renderer/OpenGLBackend.h"
#include "Renderer2D/Backends/OpenGL/OpenGL2DRenderer.h"
#include "Renderer2D/Backends/OpenGL/OpenGLWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/LayeredWindowPresenter.h"
#include "Renderer2D/Backends/Software/LayeredWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/Software2DRenderer.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Theme/BasicUIThemeResolver.h"
#include "UI2D/Widgets/Basic/UIButtonWidget.h"
#include "UI2D/Widgets/Scrolling/UIScrollViewWidget.h"

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
    UILayoutParams result;
    result.sizeRule.width = {UISizeMode::Fixed, width};
    result.sizeRule.height = {UISizeMode::Fixed, height};
    return result;
}
UITransform At(float x, float y, float width, float height) {
    UITransform result; result.position = {x,y}; result.size = {width,height}; return result;
}

std::shared_ptr<const ResolvedUITheme> MakeTheme() {
    UIThemeDefinition definition; definition.name = "ScrollViewBackendTest";
    UIStyleClassDefinition view; view.name = "Scroll.View";
    UIStyleRuleDefinition scrolling;
    scrolling.requiredStates = ToMask(UIInteractionState::Scrolling);
    scrolling.properties.colorMultiplier = Color4f{1.08f,1.08f,1.08f,1};
    view.rules.push_back(scrolling); definition.styleClasses.push_back(view);
    UIStyleClassDefinition track; track.name = "Scroll.Track";
    definition.styleClasses.push_back(track);
    UIStyleClassDefinition thumb; thumb.name = "Scroll.Thumb";
    UIStyleRuleDefinition captured;
    captured.requiredStates = ToMask(UIInteractionState::Captured);
    captured.properties.colorMultiplier = Color4f{1.2f,1.2f,1.2f,1};
    thumb.rules.push_back(captured); definition.styleClasses.push_back(thumb);
    return BasicUIThemeResolver{}.Resolve(definition).theme;
}

struct Snapshot {
    Vec2F offset{};
    Vec2F maximum{};
    float thumbLength = 0;
    float thumbPosition = 0;
    std::size_t drawItems = 0;
};

struct ScrollFixture {
    WindowHandle window{};
    BasicLayoutEngine layout;
    BasicUIHitTester hit;
    BasicUIInputRouter input;
    BasicUIRenderBuilder render;
    std::shared_ptr<const ResolvedUITheme> theme = MakeTheme();
    UIWindowRuntime runtime;
    UIWidgetHandle view{};

    explicit ScrollFixture(WindowHandle handle)
        : window(handle), runtime(handle, {layout,hit,input,render}, theme) {
        UIScrollViewWidgetDesc desc;
        desc.common.layout = Fixed(240,180);
        desc.common.transform = At(20,20,240,180);
        desc.common.styleClass = theme->FindStyleClass("Scroll.View");
        desc.contentLayout.mode = UILayoutMode::VerticalStack;
        desc.policy.vertical = UIScrollMode::Auto;
        desc.policy.smoothWheel = false;
        desc.policy.enableInertia = false;
        desc.scrollbarThickness = 12;
        desc.verticalScrollbarTrackStyle = theme->FindStyleClass("Scroll.Track");
        desc.verticalScrollbarThumbStyle = theme->FindStyleClass("Scroll.Thumb");
        desc.createHorizontalScrollbar = false;
        view = runtime.Widgets().CreateScrollView(desc);
        for (int index = 0; index < 8; ++index) {
            UIButtonWidgetDesc button;
            button.panel.common.layout.sizeRule.width.mode = UISizeMode::Stretch;
            button.panel.common.layout.sizeRule.height = {UISizeMode::Fixed,50};
            button.panel.common.tabIndex = index;
            button.panel.backgroundColor = index % 2 == 0
                ? Color4f{0.20f,0.42f,0.78f,0.96f}
                : Color4f{0.28f,0.34f,0.56f,0.96f};
            const UIWidgetHandle item = runtime.Widgets().CreateButton(button);
            runtime.Widgets().AddContent(view, item);
        }
        runtime.Update(0.0, {300,240}, 1.0f);
    }

    bool Send(InputEvent event, double seconds, float dpi) {
        if (!runtime.EnqueueInput(std::move(event))) return false;
        runtime.Update(seconds, {300,240}, dpi);
        return true;
    }
    Snapshot GetSnapshot() const {
        Snapshot result;
        const UIScrollSnapshot scroll = runtime.Widgets().GetScrollSnapshot(view);
        result.offset = scroll.offset; result.maximum = scroll.maximumOffset;
        const UIWidgetRecord* record = runtime.Widgets().Registry().TryGet(view);
        const UIWidgetRecord* bar = record
            ? runtime.Widgets().Registry().TryGet(record->scrollView->verticalScrollbar) : nullptr;
        if (bar && bar->scrollbar) {
            result.thumbLength = bar->scrollbar->thumbLength;
            result.thumbPosition = bar->scrollbar->thumbPosition;
        }
        result.drawItems = runtime.DrawList().Commands().size();
        return result;
    }
};

bool Exercise(ScrollFixture& ui, float dpi) {
    PointerWheelEvent wheel; wheel.window = ui.window; wheel.logicalPosition = {80,80};
    wheel.logicalDelta = {0,-1};
    if (!ui.Send(wheel, 0.016, dpi)) return false;
    PointerButtonEvent down; down.window = ui.window; down.logicalPosition = {80,100};
    down.button = PointerButton::Primary; down.pressed = true; down.timestampSeconds = 1.0;
    if (!ui.Send(down, 0.016, dpi)) return false;
    PointerMoveEvent move; move.window = ui.window; move.logicalPosition = {80,70};
    move.timestampSeconds = 1.02;
    if (!ui.Send(move, 0.016, dpi)) return false;
    down.logicalPosition = {80,70}; down.pressed = false; down.timestampSeconds = 1.04;
    if (!ui.Send(down, 0.016, dpi)) return false;

    // Exercise the overlay track's page-click path after Wheel and content drag.
    PointerButtonEvent track; track.window = ui.window; track.logicalPosition = {254,180};
    track.button = PointerButton::Primary; track.pressed = true; track.timestampSeconds = 2.0;
    if (!ui.Send(track, 0.016, dpi)) return false;
    track.pressed = false; track.timestampSeconds = 2.02;
    if (!ui.Send(track, 0.016, dpi)) return false;
    return ui.GetSnapshot().offset.y > 78.0f && !ui.runtime.QueryFrameActivity().hasActiveInteraction;
}

bool Same(const Snapshot& a, const Snapshot& b) {
    return std::abs(a.offset.x-b.offset.x) < 0.01f &&
        std::abs(a.offset.y-b.offset.y) < 0.01f &&
        std::abs(a.maximum.x-b.maximum.x) < 0.01f &&
        std::abs(a.maximum.y-b.maximum.y) < 0.01f &&
        std::abs(a.thumbLength-b.thumbLength) < 0.01f &&
        std::abs(a.thumbPosition-b.thumbPosition) < 0.01f &&
        a.drawItems == b.drawItems;
}
}

int main(int argc, char** argv) {
    const bool selfTest = argc > 1 && std::string(argv[1]) == "--self-test";
    GlfwApplicationHost host;
    if (!host.IsInitialized()) return 1;
    GlfwWindowSystem windows;
    WindowDesc desc; desc.title = "XiE ScrollView OpenGL"; desc.width = 300;
    desc.height = 240; desc.visible = !selfTest;
    const WindowHandle window = windows.CreateWindow(desc);
    ScrollFixture gl(window); windows.SetInputEventQueue(window, &gl.runtime.InputQueue());
    if (!Exercise(gl, 1.0f)) return 2;
    GlfwOpenGLSurface surface(windows, window); surface.MakeCurrent();
    OpenGLBackend backend; if (!backend.Init()) return 3;
    const std::array<float,16> projection = {2.0f/300,0,0,0, 0,-2.0f/240,0,0,
        0,0,-1,0, -1,1,0,1};
    backend.SetViewProjection(projection.data());
    OpenGL2DRenderer renderer; OpenGLWindowRenderPipeline pipeline(surface,backend,renderer);
    const auto glStats = pipeline.Render(gl.runtime.DrawList(), {300,240,1}); pipeline.Present();
    const Snapshot glSnapshot = gl.GetSnapshot();
    std::cout << "[OpenGL ScrollView] offset=" << glSnapshot.offset.y
              << " max=" << glSnapshot.maximum.y << " thumb=" << glSnapshot.thumbLength
              << " draws=" << glStats.drawCallCount << '\n';
    backend.Shutdown(); windows.SetInputEventQueue(window,nullptr); windows.DestroyWindow(window);
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Win32ApplicationHost winHost; Win32WindowSystem winWindows;
    WindowDesc winDesc; winDesc.title = "XiE ScrollView Layered"; winDesc.width = 300;
    winDesc.height = 240; winDesc.visible = !selfTest; winDesc.transparent = true;
    winDesc.decorated = false;
    const WindowHandle winWindow = winWindows.CreateWindow(winDesc);
    ScrollFixture softwareUI(winWindow);
    winWindows.SetInputEventQueue(winWindow, &softwareUI.runtime.InputQueue());
    if (!Exercise(softwareUI, 1.5f)) return 4;
    Software2DRenderer software;
    LayeredWindowPresenter presenter(winWindows.GetNativeHandle(winWindow));
    LayeredWindowRenderPipeline layered(software,presenter); layered.Resize(300,240,1.5f);
    const auto swStats = layered.Render(softwareUI.runtime.DrawList(), {300,240,1.5f});
    layered.Present();
    const Snapshot swSnapshot = softwareUI.GetSnapshot();
    std::cout << "[Layered ScrollView] offset=" << swSnapshot.offset.y
              << " max=" << swSnapshot.maximum.y << " thumb=" << swSnapshot.thumbLength
              << " draws=" << swStats.drawCallCount << '\n';
    winWindows.SetInputEventQueue(winWindow,nullptr); winWindows.DestroyWindow(winWindow);
    CoUninitialize();
    if (!Same(glSnapshot,swSnapshot)) return 5;
#endif
    return glStats.drawCallCount > 0 ? 0 : 6;
}
