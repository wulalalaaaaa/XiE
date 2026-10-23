#include "Platform/GLFW/GlfwApplicationHost.h"
#include "Platform/GLFW/GlfwOpenGLSurface.h"
#include "Platform/GLFW/GlfwWindowSystem.h"
#include "Renderer/OpenGLBackend.h"
#include "Renderer2D/Backends/OpenGL/OpenGL2DRenderer.h"
#include "Renderer2D/Backends/OpenGL/OpenGLWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/LayeredWindowPresenter.h"
#include "Renderer2D/Backends/Software/LayeredWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/Software2DRenderer.h"
#include "Renderer2D/TextLayoutService.h"
#include "Renderer2D/TextureRegistry2D.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Core/UIScene.h"
#include "UI2D/Layout/UILayoutTypes.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Theme/ResolvedUITheme.h"

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
#include <vector>

namespace {

Engine::UI2D::UILayoutParams Fixed(float w,float h){Engine::UI2D::UILayoutParams p;p.sizeRule.width={Engine::UI2D::UISizeMode::Fixed,w};p.sizeRule.height={Engine::UI2D::UISizeMode::Fixed,h};return p;}
void Place(Engine::UI2D::UIScene& s,Engine::UI2D::UINodeHandle n,float x,float y,float w,float h){s.SetLayout(n,Fixed(w,h));auto t=s.TryGet(n)->transform;t.position={x,y};s.SetTransform(n,t);}

void BuildScene(Engine::UI2D::UIScene& scene,Engine::TextureHandle ready,Engine::TextureHandle loading,
    Engine::TextureHandle failed,Engine::TextLayoutHandle title){
    using namespace Engine;using namespace Engine::UI2D;
    const auto background=scene.CreateNode("Background");Place(scene,background,0,0,800,600);scene.SetVisual(background,UIPanelVisual{{0.04f,0.05f,0.08f,1}});
    const auto top=scene.CreateNode("TopBar");Place(scene,top,20,20,760,72);scene.SetVisual(top,UIPanelVisual{{0.12f,0.15f,0.22f,0.95f},14,true});
    const auto logo=scene.CreateNode("Logo",top);Place(scene,logo,12,12,48,48);UIImageVisual logoVisual;logoVisual.texture=ready;logoVisual.fit=UIImageFit::Contain;scene.SetVisual(logo,logoVisual);
    const auto text=scene.CreateNode("ChineseTitle",top);Place(scene,text,76,12,360,48);UITextVisual textVisual;textVisual.layout=title;textVisual.color={1,1,1,1};textVisual.verticalAlignment=UITextVerticalAlignment::Center;scene.SetVisual(text,textVisual);
    const auto status=scene.CreateNode("StatusNineSlice",top);Place(scene,status,600,12,130,48);UINineSliceVisual nine;nine.texture=ready;nine.borders={8,8,8,8};scene.SetVisual(status,nine);
    const auto center=scene.CreateNode("CenterPanel");Place(scene,center,180,120,440,300);scene.SetVisual(center,UIPanelVisual{{0.08f,0.1f,0.16f,0.9f},20,true});scene.SetClipChildren(center,true);
    const auto rotated=scene.CreateNode("RotatedImage",center);Place(scene,rotated,40,50,140,100);UIImageVisual image;image.texture=ready;image.fit=UIImageFit::Cover;scene.SetVisual(rotated,image);auto rt=scene.TryGet(rotated)->transform;rt.pivot={0.5f,0.5f};rt.rotationRadians=0.35f;rt.scale={1.1f,0.9f};scene.SetTransform(rotated,rt);
    const auto circle=scene.CreateNode("Circle",center);Place(scene,circle,220,45,90,90);UIShapeVisual circleVisual;circleVisual.type=UIShapeType::Circle;circleVisual.color={0.2f,0.7f,1,0.85f};scene.SetVisual(circle,circleVisual);
    const auto ring=scene.CreateNode("Ring",center);Place(scene,ring,320,45,90,90);UIShapeVisual ringVisual;ringVisual.type=UIShapeType::Ring;ringVisual.color={1,0.6f,0.2f,0.9f};ringVisual.innerRadius=28;ringVisual.outerRadius=42;scene.SetVisual(ring,ringVisual);
    const auto line=scene.CreateNode("Line",center);Place(scene,line,40,200,260,40);UIShapeVisual lineVisual;lineVisual.type=UIShapeType::Line;lineVisual.color={0.7f,0.8f,1,1};lineVisual.lineStart={0,30};lineVisual.lineEnd={250,5};lineVisual.thickness=3;scene.SetVisual(line,lineVisual);
    const auto outside=scene.CreateNode("ClippedChild",center);Place(scene,outside,390,240,100,80);scene.SetVisual(outside,UIPanelVisual{{1,0.1f,0.2f,0.8f}});
    const TextureHandle states[]={ready,loading,failed,{}};for(int i=0;i<4;++i){const auto n=scene.CreateNode("ResourceState");Place(scene,n,80.0f+i*110.0f,470,90,90);UIImageVisual v;v.texture=states[i];v.fit=UIImageFit::Contain;scene.SetVisual(n,v);}
    const auto lower=scene.CreateNode("LowerNode");Place(scene,lower,590,470,150,80);scene.SetVisual(lower,UIPanelVisual{{0.1f,0.8f,0.4f,0.8f},10,true});scene.SetZOrder(lower,1);
    const auto upper=scene.CreateNode("UpperNode");Place(scene,upper,630,500,130,70);scene.SetVisual(upper,UIPanelVisual{{0.8f,0.2f,0.6f,0.8f},10,true});scene.SetZOrder(upper,2);
}

} // namespace

int main(int argc,char** argv){
    using namespace Engine;using namespace Engine::UI2D;
    const bool selfTest=argc>1&&std::string(argv[1])=="--self-test";
    TextureRegistry2D textures;std::vector<std::uint8_t> pixels(64*32*4,255);for(int y=0;y<32;++y)for(int x=0;x<64;++x){const auto i=static_cast<std::size_t>((y*64+x)*4);pixels[i]=static_cast<std::uint8_t>(40+x*3);pixels[i+1]=static_cast<std::uint8_t>(80+y*4);pixels[i+2]=220;}
    const auto ready=textures.CreateTextureRGBA8(64,32,pixels,AlphaMode::Straight);const auto loading=textures.CreateTextureState(32,32,AlphaMode::Straight,TextureState::Loading);const auto failed=textures.CreateTextureState(32,32,AlphaMode::Straight,TextureState::Failed);
    BasicTextLayoutService text;const auto title=text.CreateLayout({},U"XiE UI2D 渲染测试：中文",340,24);
    UIScene scene;BuildScene(scene,ready,loading,failed,title);BasicLayoutEngine layout(&textures,&text);ResolvedUITheme theme;const auto layoutResult=layout.UpdateLayout(scene,{{800,600},1,theme,scene.Root()});
    BasicUIRenderBuilder builder(&textures,&text);DrawList2D drawList;const auto build=builder.Build(scene,theme,drawList,{{800,600},1,scene.Root(),false});
    if(!layoutResult.Succeeded()||!build.success||drawList.Empty()){std::cerr<<"UI2D RenderTest build failed\n";return 1;}
    Software2DRenderer software(&textures,&text);software.Resize(800,600);software.Render(drawList,1);if(software.GetStats().drawCallCount==0){std::cerr<<"Software render emitted no draws\n";return 2;}
    std::cout<<"[Software] commands="<<software.GetStats().commandCount<<" draws="<<software.GetStats().drawCallCount<<" clips="<<software.GetStats().clipChangeCount<<'\n';

    GlfwApplicationHost host;if(!host.IsInitialized()){std::cerr<<"GLFW init failed\n";return 3;}GlfwWindowSystem windows;WindowDesc desc;desc.title="XiE UI2D OpenGL";desc.width=800;desc.height=600;desc.visible=!selfTest;const auto window=windows.CreateWindow(desc);GlfwOpenGLSurface surface(windows,window);surface.MakeCurrent();OpenGLBackend backend;if(!backend.Init()){std::cerr<<"OpenGL backend init failed\n";return 4;}const std::array<float,16> vp={2.0f/800,0,0,0,0,-2.0f/600,0,0,0,0,-1,0,-1,1,0,1};backend.SetViewProjection(vp.data());OpenGL2DRenderer gl;gl.SetTextureRegistry(&textures);gl.SetTextLayoutService(&text);OpenGLWindowRenderPipeline pipeline(surface,backend,gl);const auto glStats=pipeline.Render(drawList,{800,600,1});pipeline.Present();std::cout<<"[OpenGL] draws="<<glStats.drawCallCount<<" clips="<<glStats.clipChangeCount<<" textUploads="<<gl.TextTextureUploadCount()<<'\n';backend.Shutdown();windows.DestroyWindow(window);
#ifdef _WIN32
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Win32ApplicationHost winHost;Win32WindowSystem winWindows;WindowDesc layeredDesc;layeredDesc.title="XiE UI2D Layered";layeredDesc.width=800;layeredDesc.height=600;layeredDesc.visible=!selfTest;layeredDesc.transparent=true;layeredDesc.decorated=false;const auto layeredWindow=winWindows.CreateWindow(layeredDesc);LayeredWindowPresenter presenter(winWindows.GetNativeHandle(layeredWindow));LayeredWindowRenderPipeline layered(software,presenter);layered.Resize(800,600,1);const auto layeredStats=layered.Render(drawList,{800,600,1});layered.Present();std::cout<<"[Layered] draws="<<layeredStats.drawCallCount<<'\n';winWindows.DestroyWindow(layeredWindow);CoUninitialize();
#endif
    return glStats.drawCallCount>0&&glStats.clipChangeCount>0?0:5;
}
