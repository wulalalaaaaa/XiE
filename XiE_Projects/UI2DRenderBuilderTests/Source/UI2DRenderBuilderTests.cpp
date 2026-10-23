#include "Platform/IRenderSurface.h"
#include "Renderer/IRenderBackend.h"
#include "Renderer2D/Backends/OpenGL/OpenGL2DRenderer.h"
#include "Renderer2D/Backends/Software/Software2DRenderer.h"
#include "Renderer2D/DrawListValidator.h"
#include "Renderer2D/TextLayoutService.h"
#include "Renderer2D/TextureRegistry2D.h"
#include "UI2D/Animation/IUIAnimator.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/IUIFocusManager.h"
#include "UI2D/Input/IUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

int failures = 0;
void Expect(bool value, const char* message) {
    std::cout << (value ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!value) ++failures;
}
bool Near(float a, float b, float epsilon = 0.02f) { return std::abs(a-b) <= epsilon; }

Engine::UI2D::UILayoutParams Fixed(float width, float height) {
    Engine::UI2D::UILayoutParams value;
    value.sizeRule.width = {Engine::UI2D::UISizeMode::Fixed,width};
    value.sizeRule.height = {Engine::UI2D::UISizeMode::Fixed,height};
    return value;
}
void Place(Engine::UI2D::UIScene& scene, Engine::UI2D::UINodeHandle node,
    float x,float y,float width,float height) {
    scene.SetLayout(node,Fixed(width,height));
    auto transform=scene.TryGet(node)->transform; transform.position={x,y}; scene.SetTransform(node,transform);
}
bool Layout(Engine::UI2D::BasicLayoutEngine& layout, Engine::UI2D::UIScene& scene,
    Engine::Vec2F size={400,300}) {
    static const Engine::UI2D::ResolvedUITheme theme{};
    return layout.UpdateLayout(scene,{size,1.0f,theme,scene.Root()}).Succeeded();
}
Engine::UI2D::UIRenderResult Build(Engine::UI2D::BasicUIRenderBuilder& builder,
    const Engine::UI2D::UIScene& scene, Engine::DrawList2D& list,
    Engine::UI2D::UIRenderContext context={}) {
    static const Engine::UI2D::ResolvedUITheme theme{};
    if (!context.root.IsValid()) context.root=scene.Root();
    if (context.logicalViewportSize==Engine::Vec2F{}) context.logicalViewportSize={400,300};
    return builder.Build(scene,theme,list,context);
}
std::uint8_t A(const Engine::PremultipliedBitmap& b,int x,int y){return b.bgra[static_cast<std::size_t>((y*b.width+x)*4+3)];}
std::uint8_t R(const Engine::PremultipliedBitmap& b,int x,int y){return b.bgra[static_cast<std::size_t>((y*b.width+x)*4+2)];}
Engine::UI2D::UIHitTestResult Hit(
    const Engine::UI2D::BasicUIHitTester& tester,
    const Engine::UI2D::UIScene& scene,
    Engine::Vec2F point);

class FakeSurface final : public Engine::IRenderSurface {
public:
    Engine::WindowHandle GetWindow() const override{return {1,1};}
    void MakeCurrent() override{} void Present() override{}
    void Resize(int w,int h) override{size={w,h};}
    Engine::RenderSurfaceSize GetFramebufferSize() const override{return size;}
    Engine::RenderSurfaceSize size{128,128};
};
class FakeBackend final : public Engine::IRenderBackend {
public:
    bool Init() override{return true;} void SetViewport(int,int) override{}
    void SetScissorEnabled(bool enabled) override{scissorEnabled=enabled;++scissorChanges;}
    void SetScissorRect(int,int,int,int) override{++scissorRects;}
    void SetBlendMode(Engine::BlendMode,Engine::AlphaMode) override{}
    void UploadMesh(const float* v,int count,int dim,const float*,int,const unsigned int*,int) override {
        vertices.assign(v,v+count*dim);++meshUploads;
    }
    void UploadTextureRGBA8(const unsigned char*,int,int) override{++textureUploads;}
    void BindTextureRGBA8(std::uint64_t key,const unsigned char*,int,int) override {
        if (textureKeys.insert(key).second) ++textureUploads;
    }
    void SetTextureEnabled(bool) override{} void ClearTexture() override{}
    void SetMaterialTint(const float*) override{} void SetViewProjection(const float*) override{}
    void BeginFrame() override{} void DrawMesh() override{++draws;} void EndFrame() override{}
    void Shutdown() override{}
    int draws=0,meshUploads=0,textureUploads=0,scissorChanges=0,scissorRects=0;
    bool scissorEnabled=false; std::vector<float> vertices;
    std::unordered_set<std::uint64_t> textureKeys;
};

void TestBasicPanelAndState() {
    using namespace Engine; using namespace Engine::UI2D;
    BasicLayoutEngine layout; BasicUIRenderBuilder builder; DrawList2D list;
    UIScene empty; Layout(layout,empty); auto result=Build(builder,empty,list);
    Expect(result.success&&list.Empty(),"1. Empty Scene produces empty DrawList");
    UIRenderContext bad; bad.root={999,1}; bad.logicalViewportSize={400,300};
    Expect(!Build(builder,empty,list,bad).success,"2. Invalid Root returns failure");
    Expect(list.Empty(),"3. monostate emits no command");

    UIScene scene; const auto panel=scene.CreateNode("panel"); Place(scene,panel,10,10,100,50);
    scene.SetVisual(panel,UIPanelVisual{{0.8f,0.4f,0.2f,1.0f},0,false}); Layout(layout,scene);
    result=Build(builder,scene,list);
    Expect(result.success&&list.Commands().size()==1&&std::holds_alternative<SolidRectCommand>(list.Commands()[0]),"8. Panel emits SolidRect");
    auto solid=std::get<SolidRectCommand>(list.Commands()[0]);
    Expect(Near(solid.color.r,0.8f)&&solid.alphaMode==AlphaMode::Premultiplied,"10. Panel premultiplied color is correct");
    scene.SetVisual(panel,UIPanelVisual{{1,1,1,1},500,true}); Build(builder,scene,list);
    const bool hasRounded=!list.Empty()&&std::holds_alternative<RoundedRectCommand>(list.Commands()[0]);
    Expect(hasRounded,"9. Rounded Panel emits RoundedRect");
    if(hasRounded) Expect(Near(std::get<RoundedRectCommand>(list.Commands()[0]).radius,25),"12. Rounded Panel radius clamps");

    scene.SetVisibility(panel,false); Layout(layout,scene); Build(builder,scene,list);
    Expect(list.Empty(),"4. invisible node emits no command");
    scene.SetVisibility(panel,true); scene.TryGet(panel)->opacity=0; scene.MarkDirty(panel,UIDirtyFlags::Layout); Layout(layout,scene); Build(builder,scene,list);
    Expect(list.Empty(),"5. effectiveOpacity zero emits no command");
    scene.TryGet(panel)->opacity=1; scene.SetEnabled(panel,false); Layout(layout,scene); Build(builder,scene,list);
    Expect(!list.Empty(),"6. disabled node still renders");
    scene.SetHitTestVisible(panel,false); Build(builder,scene,list);
    Expect(!list.Empty(),"7. hitTestVisible false still renders");

    const auto child=scene.CreateNode("child",panel); Place(scene,child,0,0,20,20);
    scene.SetVisual(child,UIPanelVisual{{1,0,0,1},0,false}); scene.TryGet(panel)->opacity=0.5f;
    scene.TryGet(child)->opacity=0.5f; scene.MarkAllDirty(UIDirtyFlags::Layout); Layout(layout,scene); Build(builder,scene,list);
    const auto childCommand=std::get<SolidRectCommand>(list.Commands().back());
    Expect(Near(childCommand.color.r,0.25f)&&Near(childCommand.color.a,0.25f),"11. effectiveOpacity applies premultiplied RGB and alpha");
}

void TestImagesAndNineSlice() {
    using namespace Engine; using namespace Engine::UI2D;
    TextureRegistry2D textures; BasicLayoutEngine layout(&textures); BasicUIRenderBuilder builder(&textures); DrawList2D list;
    std::vector<std::uint8_t> pixels(200*100*4,255);
    const auto ready=textures.CreateTextureRGBA8(200,100,pixels,AlphaMode::Straight);
    const auto loading=textures.CreateTextureState(32,32,AlphaMode::Straight,TextureState::Loading);
    const auto failed=textures.CreateTextureState(32,32,AlphaMode::Straight,TextureState::Failed);
    UIScene scene; const auto image=scene.CreateNode("image"); Place(scene,image,0,0,100,100);
    UIImageVisual visual; visual.texture=ready; visual.fit=UIImageFit::Stretch; visual.tint={1,1,1,0.5f}; scene.SetVisual(image,visual); Layout(layout,scene); Build(builder,scene,list);
    auto sprite=std::get<SpriteCommand>(list.Commands()[0]);
    Expect(sprite.texture==ready,"13. Ready texture emits Sprite");
    Expect(Near(sprite.dst.width,100)&&Near(sprite.dst.height,100),"17. Stretch fills local bounds");
    Expect(sprite.alphaMode==AlphaMode::Straight&&Near(sprite.color.a,0.5f),"21. Texture alpha mode, tint and opacity are retained");
    visual.fit=UIImageFit::Contain; scene.SetVisual(image,visual); Build(builder,scene,list); sprite=std::get<SpriteCommand>(list.Commands()[0]);
    Expect(Near(sprite.dst.width,100)&&Near(sprite.dst.height,50)&&Near(sprite.dst.y,25),"18. Contain preserves aspect and centers");
    visual.fit=UIImageFit::Cover; scene.SetVisual(image,visual); Build(builder,scene,list); sprite=std::get<SpriteCommand>(list.Commands()[0]);
    Expect(Near(sprite.uv.width,0.5f)&&Near(sprite.uv.x,0.25f),"19. Cover crops UV");
    visual.fit=UIImageFit::None; scene.SetVisual(image,visual); Build(builder,scene,list); sprite=std::get<SpriteCommand>(list.Commands()[0]);
    Expect(Near(sprite.dst.width,100)&&Near(sprite.dst.height,100),"20. None uses centered intrinsic size constrained by node");
    visual.texture=loading; scene.SetVisual(image,visual); auto result=Build(builder,scene,list);
    Expect(std::holds_alternative<SolidRectCommand>(list.Commands()[0])&&result.loadingTextureCount==1,"14. Loading texture emits neutral placeholder");
    visual.texture=failed; scene.SetVisual(image,visual); result=Build(builder,scene,list);
    Expect(result.failedTextureCount==1,"15. Failed texture emits failed placeholder");
    visual.texture={}; scene.SetVisual(image,visual); result=Build(builder,scene,list);
    Expect(result.missingTextureCount==1,"16. Invalid texture emits missing placeholder");

    UIScene nineScene; const auto nine=nineScene.CreateNode("nine"); Place(nineScene,nine,0,0,10,8);
    UINineSliceVisual nv; nv.texture=ready; nv.borders={8,6,8,6}; nv.tint={1,1,1,0.5f}; nineScene.SetVisual(nine,nv);
    Layout(layout,nineScene); result=Build(builder,nineScene,list);
    const auto command=std::get<NineSliceCommand>(list.Commands()[0]);
    Expect(result.success&&command.texture==ready,"31. NineSlice emits NineSliceCommand");
    Expect(Near(command.left+command.right,10)&&Near(command.top+command.bottom,8),"32-33. Oversized destination borders shrink proportionally");
    Expect(Near(command.sourceLeftUv,8.0f/200.0f),"NineSlice source borders convert to UV");
    Expect(Near(command.color.a,0.5f),"35. NineSlice opacity is applied");
    nv.texture=loading; nineScene.SetVisual(nine,nv); result=Build(builder,nineScene,list);
    Expect(result.loadingTextureCount==1&&std::holds_alternative<SolidRectCommand>(list.Commands()[0]),"34. Loading NineSlice uses placeholder");

    const auto revisionBeforeReady = textures.Revision();
    Expect(textures.UpdateTextureRGBA8(loading,32,32,pixels,AlphaMode::Straight)&&
        textures.Revision()>revisionBeforeReady,"Resource transition Loading -> Ready advances revision");
    visual.texture=loading; scene.SetVisual(image,visual); result=Build(builder,scene,list);
    Expect(result.success&&std::holds_alternative<SpriteCommand>(list.Commands()[0]),
        "Loading -> Ready on the same TextureHandle emits Sprite");
}

void TestTextAndShapes() {
    using namespace Engine; using namespace Engine::UI2D;
    BasicTextLayoutService text; BasicLayoutEngine layout(nullptr,&text); BasicUIRenderBuilder builder(nullptr,&text); DrawList2D list;
    const auto chinese=text.CreateLayout({},U"中文缓存",80,20); const auto count=text.RasterizeCount();
    UIScene scene; const auto node=scene.CreateNode("text"); Place(scene,node,0,0,120,60);
    UITextVisual tv; tv.layout=chinese; tv.color={1,1,1,1}; scene.SetVisual(node,tv); Layout(layout,scene); Build(builder,scene,list);
    Expect(std::holds_alternative<TextCommand>(list.Commands()[0]),"22/28. Ready Chinese TextLayout emits TextCommand");
    auto command=std::get<TextCommand>(list.Commands()[0]); Expect(Near(command.position.x,0),"24. Text Start alignment");
    tv.horizontalAlignment=UITextHorizontalAlignment::Center; tv.verticalAlignment=UITextVerticalAlignment::Center; scene.SetVisual(node,tv); Build(builder,scene,list); command=std::get<TextCommand>(list.Commands()[0]);
    Expect(Near(command.position.x,20)&&Near(command.position.y,(60-32)*0.5f),"25/27. Text Center alignment");
    tv.horizontalAlignment=UITextHorizontalAlignment::End; tv.verticalAlignment=UITextVerticalAlignment::End; scene.SetVisual(node,tv); Build(builder,scene,list); command=std::get<TextCommand>(list.Commands()[0]);
    Expect(Near(command.position.x,40)&&Near(command.position.y,28),"26/27. Text End alignment");
    Expect(text.RasterizeCount()==count,"29. Builder never rerasterizes a TextLayout");
    tv.layout={}; scene.SetVisual(node,tv); auto result=Build(builder,scene,list);
    Expect(list.Empty()&&result.missingTextLayoutCount==1,"23. Invalid TextLayout safely skips");

    const UIShapeType types[]={UIShapeType::SolidRect,UIShapeType::RoundedRect,UIShapeType::Line,UIShapeType::Circle,UIShapeType::Ring};
    for(int i=0;i<5;++i){UIShapeVisual shape;shape.type=types[i];shape.color={1,1,1,1};shape.cornerRadius=20;shape.lineEnd={30,20};shape.thickness=4;shape.innerRadius=5;shape.outerRadius=15;scene.SetVisual(node,shape);Build(builder,scene,list);
        Expect(!list.Empty(),(std::string("36-40. Shape emits command index ")+std::to_string(i)).c_str());}
    UIShapeVisual ring; ring.type=UIShapeType::Ring; ring.color={1,1,1,1};ring.innerRadius=99;ring.outerRadius=10;scene.SetVisual(node,ring);result=Build(builder,scene,list);
    const auto normalized=std::get<RingCommand>(list.Commands()[0]); Expect(normalized.thickness>=0&&normalized.thickness<=normalized.radius,"41. Invalid Ring radii normalize");
    UIShapeVisual line;line.type=UIShapeType::Line;line.color={1,1,1,1};line.lineEnd={10,10};line.thickness=-2;scene.SetVisual(node,line);result=Build(builder,scene,list);
    Expect(list.Empty()&&result.invalidVisualCount==1,"42. Invalid line width safely rejects");
}

void TestTransformOrderClipAndBackends() {
    using namespace Engine; using namespace Engine::UI2D;
    BasicLayoutEngine layout; BasicUIRenderBuilder builder; BasicUIHitTester hit; DrawList2D list;
    UIScene scene; const auto parent=scene.CreateNode("parent");Place(scene,parent,40,40,80,80);scene.SetVisual(parent,UIPanelVisual{{0,0,1,1}});
    const auto low=scene.CreateNode("low",parent);Place(scene,low,0,0,60,60);scene.SetVisual(low,UIPanelVisual{{1,0,0,1}});scene.SetZOrder(low,1);
    const auto high=scene.CreateNode("high",parent);Place(scene,high,0,0,60,60);scene.SetVisual(high,UIPanelVisual{{0,1,0,1}});scene.SetZOrder(high,2);
    auto t=scene.TryGet(high)->transform;t.pivot={0.5f,0.5f};t.rotationRadians=0.5f;t.scale={1.2f,0.8f};scene.SetTransform(high,t);
    scene.SetClipChildren(parent,true);Layout(layout,scene);auto result=Build(builder,scene,list);
    Expect(std::holds_alternative<SolidRectCommand>(list.Commands()[0])&&std::holds_alternative<PushClipRectCommand>(list.Commands()[1]),"55-57. Parent visual precedes child Clip push");
    Expect(result.clipPushCount==1&&result.clipPopCount==1&&DrawListValidator{}.Validate(list).valid,"55/61. Clip stack balances and validates");
    const auto lowCmd=std::get<SolidRectCommand>(list.Commands()[2]);const auto highCmd=std::get<SolidRectCommand>(list.Commands()[3]);
    Expect(lowCmd.transform==scene.TryGet(low)->layoutState.computedTransform.localToScene,"43/47. Nested translation uses layout localToScene");
    Expect(highCmd.transform==scene.TryGet(high)->layoutState.computedTransform.localToScene,"44-48. Scale, rotation and Pivot are preserved as transform, not AABB");
    Expect(list.Commands().size()==5&&scene.PainterTraversal()[1].node==parent,"50-54. Draw command order follows shared parent/z/insertion traversal");
    Expect(Hit(hit,scene,{50,50}).target==high,"54. HitTest reverse order selects last rendered overlapping node");
    scene.TryGet(high)->layoutState.computedTransform.localToScene.m00=std::numeric_limits<float>::quiet_NaN();
    result=Build(builder,scene,list);
    Expect(!result.success&&result.invalidTransformCount==1,"49. Non-finite transform skips node and reports failure");
    scene.MarkDirty(high,UIDirtyFlags::Layout);Layout(layout,scene);

    const auto nestedClip=scene.CreateNode("nestedClip",parent);Place(scene,nestedClip,5,5,30,30);scene.SetClipChildren(nestedClip,true);
    const auto nested=scene.CreateNode("nested",nestedClip);Place(scene,nested,0,0,20,20);scene.SetVisual(nested,UIPanelVisual{{1,1,1,1}});Layout(layout,scene);result=Build(builder,scene,list);
    Expect(result.clipPushCount==2&&result.clipPopCount==2,"58. Nested clips pair correctly");
    scene.SetVisibility(nestedClip,false);Layout(layout,scene);result=Build(builder,scene,list);
    Expect(result.clipPushCount==1,"59. Invisible node emits no Clip");
    const auto leaf=scene.CreateNode("leaf");Place(scene,leaf,200,0,20,20);scene.SetClipChildren(leaf,true);Layout(layout,scene);result=Build(builder,scene,list);
    Expect(result.clipPushCount==1,"60. Childless clip node emits no meaningless Clip");

    Software2DRenderer software;software.Resize(200,200);software.Render(list,1);
    Expect(software.GetBitmap().bgra.size()==200u*200u*4u,"71-75. Software backend accepts transformed clipped command sequence");
    DrawList2D rotatedList;SolidRectCommand rotatedCommand;rotatedCommand.rect={0,0,40,20};rotatedCommand.color={0.5f,0,0,0.5f};rotatedCommand.transform=Multiply(TranslationMatrix({50,50}),RotationMatrix(0.785398163f));rotatedList.AddSolidRect(rotatedCommand);software.Resize(120,120);software.Render(rotatedList,1);
    Expect(A(software.GetBitmap(),57,71)>100&&A(software.GetBitmap(),37,50)==0,"48/72-73. Software rasterizes true rotated premultiplied geometry, not its AABB");
    FakeSurface surface;FakeBackend backend;OpenGL2DRenderer gl;gl.Render(surface,backend,list,{128,128,1});
    Expect(backend.draws>0&&backend.scissorRects>0&&backend.scissorChanges>=2,"71/75. OpenGL backend accepts same commands and applies Clip");

    DrawList2D textList; BasicTextLayoutService text;const auto textHandle=text.CreateLayout({},U"中文",60,18);
    TextCommand tc;tc.layout=textHandle;tc.color={1,1,1,1};textList.AddText(tc);gl.SetTextLayoutService(&text);gl.Render(surface,backend,textList,{128,128,1});gl.Render(surface,backend,textList,{128,128,1});
    Expect(gl.TextTextureUploadCount()==1&&backend.textureUploads==1,
        "30/77. OpenGL uploads each Chinese layout texture once");
}

Engine::UI2D::UIHitTestResult Hit(const Engine::UI2D::BasicUIHitTester& tester,const Engine::UI2D::UIScene& scene,Engine::Vec2F p){return tester.HitTest(scene,{},p);}

class NullRouter final:public Engine::UI2D::IUIInputRouter{public:Engine::UI2D::UIInputDispatchResult Dispatch(Engine::UI2D::UIScene&,std::span<const Engine::InputEvent> e,Engine::UI2D::UIInputDispatchContext&)override{Engine::UI2D::UIInputDispatchResult r;r.consumedInputCount=static_cast<std::uint32_t>(e.size());return r;}void OnNodeInvalidated(Engine::UI2D::UIScene&,Engine::UI2D::UINodeHandle)override{}};
void TestRuntimeDirtyAndVisualConsistency(){
    using namespace Engine;using namespace Engine::UI2D;
    TextureRegistry2D textures;BasicTextLayoutService text;BasicLayoutEngine layout(&textures,&text);BasicUIRenderBuilder builder(&textures,&text);BasicUIHitTester hit;NullRouter router;NullUIAnimator animator;UIWindowRuntime runtime({8,1},{layout,hit,router,builder},nullptr);
    const auto node=runtime.Scene().CreateNode("runtime");Place(runtime.Scene(),node,0,0,30,30);runtime.Scene().SetVisual(node,UIPanelVisual{{1,0,0,1}});
    auto first=runtime.Update(0,{100,100},1);auto clean=runtime.Update(0,{100,100},1);
    Expect(first.drawListRebuilt&& !clean.drawListRebuilt,"62-63/69. Dirty builds once, successful Build clears dirty cache");
    auto p=runtime.Scene().TryGet(node)->layout;p.sizeRule.width.value=40;runtime.Scene().SetLayout(node,p);Expect(runtime.Update(0,{100,100},1).drawListRebuilt,"64. Layout change rebuilds");
    auto tr=runtime.Scene().TryGet(node)->transform;tr.rotationRadians=0.2f;runtime.Scene().SetTransform(node,tr);Expect(runtime.Update(0,{100,100},1).drawListRebuilt,"65. Transform change rebuilds");
    runtime.SetTheme(std::make_shared<ResolvedUITheme>());Expect(runtime.Update(0,{100,100},1).drawListRebuilt,"66. Theme change rebuilds");
    const std::uint8_t pixel[]={255,255,255,255};textures.CreateTextureRGBA8(1,1,pixel,AlphaMode::Straight);Expect(runtime.Update(0,{100,100},1).drawListRebuilt,"67. Texture revision rebuilds");
    text.CreateLayout({},U"revision",40,12);Expect(runtime.Update(0,{100,100},1).drawListRebuilt,"68. Text revision rebuilds");
    runtime.SetVisible(false);runtime.Scene().SetVisual(node,UIPanelVisual{{0,1,0,1}});Expect(!runtime.Update(0,{100,100},1).drawListRebuilt,"70. Hidden UIWindowRuntime does not Build");

    UIScene visual;const auto lower=visual.CreateNode("lower");Place(visual,lower,0,0,40,40);visual.SetVisual(lower,UIPanelVisual{{1,0,0,1}});const auto upper=visual.CreateNode("upper");Place(visual,upper,0,0,40,40);visual.SetVisual(upper,UIPanelVisual{{0,1,0,1}});visual.SetZOrder(upper,2);Layout(layout,visual);DrawList2D list;Build(builder,visual,list);Software2DRenderer software(&textures,&text);software.Resize(50,50);software.Render(list,1);
    Expect(Hit(hit,visual,{10,10}).target==upper&&software.GetBitmap().bgra[(10*50+10)*4+1]>200,"Painter order is visually consistent with HitTest");
    visual.TryGet(upper)->opacity=0;visual.MarkDirty(upper,UIDirtyFlags::Layout);Layout(layout,visual);Build(builder,visual,list);software.Render(list,1);
    Expect(Hit(hit,visual,{10,10}).target==upper&&R(software.GetBitmap(),10,10)>200,"opacity=0 remains explicitly hittable while skipped visually");
}

} // namespace

int main(){std::cout<<std::unitbuf;TestBasicPanelAndState();TestImagesAndNineSlice();TestTextAndShapes();TestTransformOrderClipAndBackends();TestRuntimeDirtyAndVisualConsistency();std::cout<<(failures?"UI2D render-builder tests failed\n":"UI2D render-builder tests passed\n");return failures?EXIT_FAILURE:EXIT_SUCCESS;}
