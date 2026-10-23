#include "Platform/GLFW/GlfwApplicationHost.h"
#include "Input/InputDiagnostics.h"
#include "Platform/GLFW/GlfwOpenGLSurface.h"
#include "Platform/GLFW/GlfwWindowSystem.h"
#include "Renderer/OpenGLBackend.h"
#include "Renderer2D/Backends/OpenGL/OpenGL2DRenderer.h"
#include "Renderer2D/Backends/OpenGL/OpenGLWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/LayeredWindowPresenter.h"
#include "Renderer2D/Backends/Software/LayeredWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/Software2DRenderer.h"
#include "Renderer2D/TextLayoutService.h"
#include "UI2D/Animation/IUIAnimator.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Input/IUIFocusManager.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Widgets/Text/UITextBoxWidget.h"

#ifdef _WIN32
#include "Platform/Win32/Win32ApplicationHost.h"
#include "Platform/Win32/Win32WindowSystem.h"
#include "Platform/Windows/TextInput/WindowsTextInputMethod.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <windowsx.h>
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
#include <vector>

namespace {
using namespace Engine;
using namespace Engine::UI2D;

UILayoutParams Fixed(float width,float height){UILayoutParams p;p.sizeRule.width={UISizeMode::Fixed,width};p.sizeRule.height={UISizeMode::Fixed,height};return p;}
void Place(UIScene& scene,UINodeHandle node,float x,float y,float w,float h){scene.SetLayout(node,Fixed(w,h));auto t=scene.TryGet(node)->transform;t.position={x,y};scene.SetTransform(node,t);}

struct Counters{int enters=0,moves=0,ups=0,clicks=0,wheels=0,keys=0;};

struct WindowUI{
    BasicTextLayoutService textLayouts;BasicLayoutEngine layout{nullptr,&textLayouts};
    BasicUIHitTester hit;BasicUIInputRouter router;
    NullUIAnimator animator;BasicUIRenderBuilder builder{nullptr,&textLayouts};UIWindowRuntime runtime;
    UINodeHandle buttonA{},buttonB{},rotated{};UIWidgetHandle textBox{};Counters counters;
    explicit WindowUI(WindowHandle window):runtime(window,{layout,hit,router,builder,&textLayouts},nullptr){
        auto& s=runtime.Scene();
        const auto background=s.CreateNode("Background");Place(s,background,0,0,640,420);s.SetVisual(background,UIPanelVisual{{0.04f,0.05f,0.08f,1}});
        const auto top=s.CreateNode("TopBar");Place(s,top,0,0,640,80);s.SetVisual(top,UIPanelVisual{{0.1f,0.12f,0.18f,1}});
        buttonA=s.CreateNode("ButtonA",top);Place(s,buttonA,24,18,120,44);s.SetVisual(buttonA,UIPanelVisual{{0.2f,0.5f,0.9f,1},8,true});s.SetFocusable(buttonA,true);
        buttonB=s.CreateNode("ButtonB",top);Place(s,buttonB,164,18,120,44);s.SetVisual(buttonB,UIPanelVisual{{0.5f,0.3f,0.8f,1},8,true});
        const auto center=s.CreateNode("CenterPanel");Place(s,center,70,110,500,230);s.SetClipChildren(center,true);s.SetVisual(center,UIPanelVisual{{0.12f,0.14f,0.2f,1},12,true});
        const auto circle=s.CreateNode("CircleNode",center);Place(s,circle,40,60,90,90);UIShapeVisual cs;cs.type=UIShapeType::Circle;cs.color={0.1f,0.9f,0.5f,0.9f};s.SetVisual(circle,cs);
        const auto ring=s.CreateNode("RingNode",center);Place(s,ring,160,60,90,90);UIShapeVisual rs;rs.type=UIShapeType::Ring;rs.color={0.9f,0.7f,0.1f,1};rs.innerRadius=25;rs.outerRadius=42;s.SetVisual(ring,rs);
        rotated=s.CreateNode("RotatedImage",center);Place(s,rotated,300,45,130,110);s.SetVisual(rotated,UIPanelVisual{{0.9f,0.25f,0.2f,0.9f},10,true});auto rt=s.TryGet(rotated)->transform;rt.pivot={0.5f,0.5f};rt.rotationRadians=0.35f;s.SetTransform(rotated,rt);
        const auto status=s.CreateNode("StatusPanel");Place(s,status,70,360,500,42);s.SetVisual(status,UIPanelVisual{{0.16f,0.18f,0.24f,1},6,true});
        UITextBoxWidgetDesc textBoxDesc;textBoxDesc.common.layout=Fixed(220,36);
        textBoxDesc.common.transform.position={24,86};textBoxDesc.common.transform.size={220,36};
        textBoxDesc.placeholder="Native input";textBox=runtime.Widgets().CreateTextBox(textBoxDesc);

        runtime.AddEventListener(buttonA,UIEventType::PointerEnter,UIEventPhaseMask::Target,[this](UIEventContext& c){++counters.enters;c.EnqueueMutation(SetVisualMutation{buttonA,UIPanelVisual{{0.3f,0.7f,1,1},8,true}});});
        runtime.AddEventListener(buttonA,UIEventType::PointerMove,UIEventPhaseMask::Target,[this](UIEventContext&){++counters.moves;});
        runtime.AddEventListener(buttonA,UIEventType::PointerUp,UIEventPhaseMask::Target,[this](UIEventContext&){++counters.ups;});
        runtime.AddEventListener(buttonA,UIEventType::Click,UIEventPhaseMask::Target,[this](UIEventContext& c){++counters.clicks;c.EnqueueMutation(SetVisualMutation{rotated,UIPanelVisual{{0.2f,0.8f,0.4f,0.9f},10,true}});});
        runtime.AddEventListener(buttonA,UIEventType::PointerWheel,UIEventPhaseMask::Target,[this](UIEventContext&){++counters.wheels;});
        runtime.AddEventListener(buttonA,UIEventType::KeyDown,UIEventPhaseMask::Target,[this](UIEventContext&){++counters.keys;});
        runtime.Update(0,{640,420},1);
    }
};

PointerMoveEvent Move(WindowHandle w,Vec2F p){PointerMoveEvent e;e.window=w;e.logicalPosition=p;return e;}
PointerButtonEvent Button(WindowHandle w,Vec2F p,bool down){PointerButtonEvent e;e.window=w;e.logicalPosition=p;e.button=PointerButton::Left;e.pressed=down;return e;}

bool Exercise(WindowUI& ui,WindowHandle window){
    auto send=[&](InputEvent e){if(!ui.runtime.EnqueueInput(std::move(e)))return false;ui.runtime.Update(0,{640,420},1);return true;};
    if(!send(WindowFocusEvent{window,true,0.0}))return false;
    if(!send(Move(window,{40,40}))||!send(Button(window,{40,40},true))||!send(Move(window,{620,400}))||!send(Button(window,{620,400},false)))return false;
    if(!send(Move(window,{40,40}))||!send(Button(window,{40,40},true))||!send(Button(window,{40,40},false)))return false;
    PointerWheelEvent wheel;wheel.window=window;wheel.logicalPosition={40,40};wheel.logicalDelta={0,1};if(!send(wheel))return false;
    KeyEvent key;key.window=window;key.key=InputKeyCode::Enter;key.pressed=true;if(!send(key))return false;
    return ui.counters.enters>=1&&ui.counters.moves>=2&&ui.counters.ups==2&&ui.counters.clicks==1&&ui.counters.wheels==1&&ui.counters.keys==1;
}

#ifdef _WIN32
LPARAM MousePoint(int x,int y){return MAKELPARAM(static_cast<short>(x),static_cast<short>(y));}
LPARAM LogicalMousePoint(Win32WindowSystem& windows,WindowHandle window,float x,float y){
    const float scale=windows.GetDpiScale(window);
    return MousePoint(static_cast<int>(std::lround(x*scale)),static_cast<int>(std::lround(y*scale)));
}

bool ExerciseNativeGlfwFocusOnWin32(
    GlfwWindowSystem& windows, WindowHandle window, WindowUI& ui) {
    const auto hwnd = reinterpret_cast<HWND>(windows.GetNativeHandle(window));
    if (!hwnd) return false;
    windows.ShowWindow(window);
    (void)SetFocus(hwnd);
    ui.runtime.Update(0, {640,420}, 1);
    if (GetFocus() != hwnd || !ui.runtime.Interaction().IsWindowFocused())
        return false;
    (void)SetFocus(nullptr);
    ui.runtime.Update(0, {640,420}, 1);
    if (ui.runtime.Interaction().IsWindowFocused()) return false;
    (void)SetFocus(hwnd);
    ui.runtime.Update(0, {640,420}, 1);
    return GetFocus() == hwnd &&
        ui.runtime.Interaction().IsWindowFocused();
}

bool ExerciseNativeGlfwPointerOnWin32(
    GlfwWindowSystem& windows, WindowHandle window, WindowUI& ui) {
    const auto fail=[](const char* stage){
        std::cerr<<"[GLFW native pointer failure] "<<stage<<'\n';
        return false;
    };
    const auto hwnd = reinterpret_cast<HWND>(windows.GetNativeHandle(window));
    if (!hwnd) return fail("native handle");
    windows.ShowWindow(window);
    (void)SetFocus(hwnd);
    ui.runtime.Update(0,{640,420},1);

    RECT client{};
    GetClientRect(hwnd,&client);
    const auto framebuffer=windows.GetFramebufferSize(window);
    std::cout<<"[GLFW native pointer] client="
             <<client.right-client.left<<'x'<<client.bottom-client.top
             <<" framebuffer="<<framebuffer.width<<'x'<<framebuffer.height
             <<" dpi="<<windows.GetDpiScale(window)<<'\n';

    SendMessageW(hwnd,WM_MOUSEMOVE,0,MousePoint(40,40));
    SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MousePoint(40,40));
    SendMessageW(hwnd,WM_LBUTTONUP,0,MousePoint(40,40));
    auto events=ui.runtime.InputQueue().Drain();
    bool sawMove=false,sawDown=false,sawUp=false;
    for(const auto& event:events){
        if(const auto* move=std::get_if<PointerMoveEvent>(&event)){
            sawMove=true;
            std::cout<<"  move="<<move->logicalPosition.x<<','
                     <<move->logicalPosition.y<<'\n';
        } else if(const auto* button=std::get_if<PointerButtonEvent>(&event)){
            (button->pressed?sawDown:sawUp)=true;
            std::cout<<(button->pressed?"  down=":"  up=")
                     <<button->logicalPosition.x<<','
                     <<button->logicalPosition.y<<'\n';
        }
        (void)ui.runtime.InputQueue().Push(event);
    }
    const int clicksBefore=ui.counters.clicks;
    ui.runtime.Update(0,{640,420},1);
    if(!sawMove||!sawDown||!sawUp||ui.counters.clicks!=clicksBefore+1)
        return fail("message-to-click chain");

    // A newly shown window may receive a button message before its first
    // cursor-move callback.  The Win32-backed GLFW path must retain the exact
    // lParam coordinates instead of reusing a stale point or querying the
    // global cursor.
    SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MousePoint(200,40));
    SendMessageW(hwnd,WM_LBUTTONUP,0,MousePoint(200,40));
    events=ui.runtime.InputQueue().Drain();
    bool exactDown=false,exactUp=false;
    for(const auto& event:events){
        if(const auto* button=std::get_if<PointerButtonEvent>(&event)){
            const bool exact=button->logicalPosition==Vec2F{200,40};
            if(button->pressed) exactDown=exact;
            else exactUp=exact;
        }
        (void)ui.runtime.InputQueue().Push(event);
    }
    ui.runtime.Update(0,{640,420},1);
    return exactDown&&exactUp
        ? true:fail("button coordinates without prior move");
}

bool ExerciseNativeWin32(Win32WindowSystem& windows,WindowHandle window,WindowUI& ui){
    const auto fail=[](const char* stage){std::cerr<<"[Win32 native input failure] "<<stage<<'\n';return false;};
    if(IsInputDiagnosticsEnabled())return fail("diagnostics are not opt-in by default");
    std::vector<InputDiagnosticRecord> diagnostics;
    SetInputDiagnosticSink([&](const InputDiagnosticRecord& record){diagnostics.push_back(record);});
    struct DiagnosticScope { ~DiagnosticScope(){SetInputDiagnosticSink({});} } diagnosticScope;
    const auto saw=[&](InputDiagnosticStage stage,InputDiagnosticKind kind){
        for(const auto& record:diagnostics)
            if(record.stage==stage&&record.kind==kind)return true;
        return false;
    };
    const auto hwnd=reinterpret_cast<HWND>(windows.GetNativeHandle(window));
    if(!hwnd)return fail("native handle");
    const int clicksBeforeNativeInput=ui.counters.clicks;
    ::ShowWindow(hwnd,SW_SHOW);
    (void)SetForegroundWindow(hwnd);
    (void)SetActiveWindow(hwnd);
    (void)SetFocus(hwnd);
    ui.runtime.Update(0,{640,420},1);
    if(!ui.runtime.Interaction().IsWindowFocused())return fail("focus gained");

    SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,LogicalMousePoint(windows,window,40,40));
    SendMessageW(hwnd,WM_LBUTTONUP,0,LogicalMousePoint(windows,window,40,40));
    ui.runtime.Update(0,{640,420},1);
    if(ui.counters.clicks!=clicksBeforeNativeInput+1)return fail("button click");

    const int clicksBeforeCaptureLoss=ui.counters.clicks;
    SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,LogicalMousePoint(windows,window,40,40));
    (void)ReleaseCapture();
    SendMessageW(hwnd,WM_LBUTTONUP,0,LogicalMousePoint(windows,window,40,40));
    ui.runtime.Update(0,{640,420},1);
    if(ui.counters.clicks!=clicksBeforeCaptureLoss)return fail("capture-loss cancellation");

    SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,LogicalMousePoint(windows,window,40,100));
    SendMessageW(hwnd,WM_LBUTTONUP,0,LogicalMousePoint(windows,window,40,100));
    SendMessageW(hwnd,WM_CHAR,L'Q',0);
    SendMessageW(hwnd,WM_KEYDOWN,VK_LEFT,0);
    SendMessageW(hwnd,WM_KEYUP,VK_LEFT,0);
    SendMessageW(hwnd,WM_CHAR,L'7',0);
    SendMessageW(hwnd,WM_KEYDOWN,VK_BACK,0);
    SendMessageW(hwnd,WM_KEYUP,VK_BACK,0);
    SendMessageW(hwnd,WM_KEYDOWN,VK_RIGHT,0);
    SendMessageW(hwnd,WM_KEYUP,VK_RIGHT,0);
    SendMessageW(hwnd,WM_CHAR,L'8',0);
    ui.runtime.Update(0,{640,420},1);
    const auto* box=ui.runtime.Widgets().Registry().TryGet(ui.textBox);
    if(!box||!ui.runtime.IsFocused(box->rootNode)||ui.runtime.Widgets().GetText(ui.textBox)!="Q8"){
        const auto* node=box?ui.runtime.Scene().TryGet(box->rootNode):nullptr;
        std::cerr<<"  box="<<(box!=nullptr)<<" focused="<<(box&&ui.runtime.IsFocused(box->rootNode))
                 <<" text="<<ui.runtime.Widgets().GetText(ui.textBox)
                 <<" root="<<(box?box->rootNode.index:0)<<':'<<(box?box->rootNode.generation:0)
                 <<" node="<<(node!=nullptr);
        if(node)std::cerr<<" rect="<<node->layoutState.arrangedRect.x<<','<<node->layoutState.arrangedRect.y<<','<<node->layoutState.arrangedRect.width<<','<<node->layoutState.arrangedRect.height;
        std::cerr<<'\n';
        return fail("textbox focus or WM_CHAR");
    }

    WindowDesc otherDesc;otherDesc.title="XiE UI2D Focus Peer";
    otherDesc.width=80;otherDesc.height=60;otherDesc.visible=true;
    const WindowHandle other=windows.CreateWindow(otherDesc);
    const auto otherHwnd=reinterpret_cast<HWND>(windows.GetNativeHandle(other));
    (void)SetFocus(otherHwnd);
    WindowsTextInputMethod ime(windows);
    const HWND focusBeforeImeActivation=GetFocus();
    ime.Activate(window);
    if(GetFocus()!=focusBeforeImeActivation)return fail("IME activation stole native focus");
    InputEventQueue lateBound(other);
    if(!windows.SetInputEventQueue(other,&lateBound))return fail("late queue bind");
    const auto initialFocus=lateBound.Drain();
    if(initialFocus.size()!=1||
        !std::holds_alternative<WindowFocusEvent>(initialFocus.front())||
        !std::get<WindowFocusEvent>(initialFocus.front()).focused)return fail("late focus synchronization");
    windows.SetInputEventQueue(other,nullptr);
    WindowUI otherUi(other);
    if(!windows.SetInputEventQueue(other,&otherUi.runtime.InputQueue()))
        return fail("second runtime queue bind");
    otherUi.runtime.Update(0,{640,420},1);
    ui.runtime.Update(0,{640,420},1);
    if(ui.runtime.Interaction().IsWindowFocused()||
       !otherUi.runtime.Interaction().IsWindowFocused())
        return fail("exclusive focus after window switch");
    SendMessageW(hwnd,WM_CHAR,L'X',0);
    ui.runtime.Update(0,{640,420},1);
    const bool inactiveRejected=
        ui.runtime.Widgets().GetText(ui.textBox)=="Q8";

    (void)SetFocus(hwnd);
    ui.runtime.Update(0,{640,420},1);
    otherUi.runtime.Update(0,{640,420},1);
    if(!ui.runtime.Interaction().IsWindowFocused()||
       otherUi.runtime.Interaction().IsWindowFocused())
        return fail("exclusive focus after window restore");
    ime.Activate(window);
    ime.SetCaretRect(window,{10,10,2,20});
    SendMessageW(hwnd,WM_LBUTTONDOWN,MK_LBUTTON,LogicalMousePoint(windows,window,40,100));
    SendMessageW(hwnd,WM_LBUTTONUP,0,LogicalMousePoint(windows,window,40,100));
    SendMessageW(hwnd,WM_KEYDOWN,VK_END,0);
    SendMessageW(hwnd,WM_KEYUP,VK_END,0);
    SendMessageW(hwnd,WM_CHAR,L'2',0);
    ui.runtime.Update(0,{640,420},1);
    const bool restored=
        ui.runtime.Widgets().GetText(ui.textBox)=="Q82";
    windows.SetInputEventQueue(other,nullptr);
    windows.DestroyWindow(other);
    if(!inactiveRejected||!restored)
        return fail(inactiveRejected?"refocus input":"unfocused input rejection");
    if(!saw(InputDiagnosticStage::Native,InputDiagnosticKind::TextInput)||
       !saw(InputDiagnosticStage::Queue,InputDiagnosticKind::TextInput)||
       !saw(InputDiagnosticStage::Queue,InputDiagnosticKind::QueueDequeued)||
       !saw(InputDiagnosticStage::Router,InputDiagnosticKind::HitTest)||
       !saw(InputDiagnosticStage::Router,InputDiagnosticKind::TextInput)||
       !saw(InputDiagnosticStage::Focus,InputDiagnosticKind::FocusGained)||
       !saw(InputDiagnosticStage::Focus,InputDiagnosticKind::FocusTargetChanged)||
       !saw(InputDiagnosticStage::TextBox,InputDiagnosticKind::TextInput)||
       !saw(InputDiagnosticStage::Ime,InputDiagnosticKind::ImeActivated)||
       !saw(InputDiagnosticStage::Ime,InputDiagnosticKind::ImeCandidatePosition))
        return fail("diagnostic chain coverage");
    ime.Deactivate(window);
    return true;
}
#endif
}

int main(int argc,char** argv){
    using namespace Engine;using namespace Engine::UI2D;const bool selfTest=argc>1&&std::string(argv[1])=="--self-test";
    GlfwApplicationHost host;if(!host.IsInitialized())return 1;GlfwWindowSystem windows;WindowDesc desc;desc.title="XiE UI2D Input OpenGL";desc.width=640;desc.height=420;desc.visible=!selfTest;const auto window=windows.CreateWindow(desc);WindowUI glUI(window);windows.SetInputEventQueue(window,&glUI.runtime.InputQueue());const auto initialGlfwFocus=glUI.runtime.InputQueue().Drain();if(initialGlfwFocus.size()!=1||!std::holds_alternative<WindowFocusEvent>(initialGlfwFocus.front())||std::get<WindowFocusEvent>(initialGlfwFocus.front()).focused!=windows.IsFocused(window))return 2;if(!Exercise(glUI,window))return 2;const int syntheticClickCount=glUI.counters.clicks;
    GlfwOpenGLSurface surface(windows,window);surface.MakeCurrent();OpenGLBackend backend;if(!backend.Init())return 3;const std::array<float,16> vp={2.0f/640,0,0,0,0,-2.0f/420,0,0,0,0,-1,0,-1,1,0,1};backend.SetViewProjection(vp.data());OpenGL2DRenderer renderer;OpenGLWindowRenderPipeline pipeline(surface,backend,renderer);const auto glStats=pipeline.Render(glUI.runtime.DrawList(),{640,420,1});pipeline.Present();std::cout<<"[OpenGL Input] events="<<glUI.counters.moves+glUI.counters.ups+glUI.counters.clicks+glUI.counters.wheels+glUI.counters.keys<<" draws="<<glStats.drawCallCount<<'\n';
#ifdef _WIN32
    if(!ExerciseNativeGlfwFocusOnWin32(windows,window,glUI))return 3;
    if(!ExerciseNativeGlfwPointerOnWin32(windows,window,glUI))return 8;
#endif
    backend.Shutdown();windows.SetInputEventQueue(window,nullptr);windows.DestroyWindow(window);
#ifdef _WIN32
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Win32ApplicationHost winHost;Win32WindowSystem winWindows;WindowDesc wd;wd.title="XiE UI2D Input Layered";wd.width=640;wd.height=420;wd.visible=false;wd.transparent=true;wd.decorated=false;const auto ww=winWindows.CreateWindow(wd);WindowUI swUI(ww);winWindows.SetInputEventQueue(ww,&swUI.runtime.InputQueue());if(!Exercise(swUI,ww))return 4;if(syntheticClickCount!=swUI.counters.clicks)return 6;Software2DRenderer software;LayeredWindowPresenter presenter(winWindows.GetNativeHandle(ww));LayeredWindowRenderPipeline layered(software,presenter);layered.Resize(640,420,1);const auto swStats=layered.Render(swUI.runtime.DrawList(),{640,420,1});layered.Present();std::cout<<"[Layered Input] events="<<swUI.counters.moves+swUI.counters.ups+swUI.counters.clicks+swUI.counters.wheels+swUI.counters.keys<<" draws="<<swStats.drawCallCount<<'\n';if(!ExerciseNativeWin32(winWindows,ww,swUI))return 5;winWindows.SetInputEventQueue(ww,nullptr);winWindows.DestroyWindow(ww);CoUninitialize();
#endif
    return glStats.drawCallCount>0?0:7;
}
