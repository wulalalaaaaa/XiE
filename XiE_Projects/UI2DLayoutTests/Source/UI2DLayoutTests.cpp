#include "Renderer2D/TextureRegistry2D.h"
#include "Renderer2D/ITextLayoutService.h"
#include "UI2D/Animation/IUIAnimator.h"
#include "UI2D/Core/UIScene.h"
#include "UI2D/Input/IUIFocusManager.h"
#include "UI2D/Input/IUIHitTester.h"
#include "UI2D/Input/IUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/IUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#include <cmath>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

int failures = 0;

bool Near(float a, float b, float epsilon = 0.01f) { return std::abs(a - b) <= epsilon; }
bool Near(Engine::Vec2F a, Engine::Vec2F b) { return Near(a.x, b.x) && Near(a.y, b.y); }
bool Near(Engine::RectF a, Engine::RectF b) {
    return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.width, b.width) && Near(a.height, b.height);
}

void Expect(bool value, const char* message) {
    std::cout << (value ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!value) ++failures;
}

Engine::UI2D::UILayoutParams Fixed(float width, float height) {
    Engine::UI2D::UILayoutParams layout;
    layout.sizeRule.width = {Engine::UI2D::UISizeMode::Fixed, width};
    layout.sizeRule.height = {Engine::UI2D::UISizeMode::Fixed, height};
    return layout;
}

void SetMode(Engine::UI2D::UIScene& scene, Engine::UI2D::UINodeHandle node, Engine::UI2D::UILayoutMode mode) {
    auto layout = scene.TryGet(node)->layout;
    layout.mode = mode;
    (void)scene.SetLayout(node, layout);
}

void SetPositionAndSize(
    Engine::UI2D::UIScene& scene,
    Engine::UI2D::UINodeHandle node,
    Engine::Vec2F position,
    Engine::Vec2F size) {
    auto transform = scene.TryGet(node)->transform;
    transform.position = position;
    transform.size = size;
    (void)scene.SetTransform(node, transform);
}

Engine::UI2D::UILayoutResult Layout(
    Engine::UI2D::BasicLayoutEngine& engine,
    Engine::UI2D::UIScene& scene,
    Engine::Vec2F size = {800.0f, 600.0f},
    float dpi = 1.0f) {
    static const Engine::UI2D::ResolvedUITheme theme{};
    return engine.UpdateLayout(scene, {size, dpi, theme, scene.Root()});
}

Engine::RectF Rect(const Engine::UI2D::UIScene& scene, Engine::UI2D::UINodeHandle node) {
    return scene.TryGet(node)->layoutState.arrangedRect;
}

class NullHitTester final : public Engine::UI2D::IUIHitTester {
public:
    Engine::UI2D::UIHitTestResult HitTest(
        const Engine::UI2D::UIScene&,
        const Engine::UI2D::UIHitTestContext&,
        Engine::Vec2F) const override { return {}; }
};

class FakeTextLayouts final : public Engine::ITextLayoutService {
public:
    Engine::TextLayoutHandle CreateLayout(Engine::FontHandle, std::u32string_view, float, float) override {
        return {1, 1};
    }
    const Engine::TextLayoutBitmap* GetBitmap(Engine::TextLayoutHandle handle) const override {
        return handle == Engine::TextLayoutHandle{1, 1} ? &bitmap : nullptr;
    }
    std::uint32_t RasterizeCount() const override { return 0; }
    Engine::TextLayoutBitmap bitmap{42, 18, {}};
};

class NullRouter final : public Engine::UI2D::IUIInputRouter {
public:
    Engine::UI2D::UIInputDispatchResult Dispatch(
        Engine::UI2D::UIScene&, std::span<const Engine::InputEvent> events,
        Engine::UI2D::UIInputDispatchContext&) override {
        Engine::UI2D::UIInputDispatchResult result;
        result.consumedInputCount = static_cast<std::uint32_t>(events.size());
        return result;
    }
    void OnNodeInvalidated(Engine::UI2D::UIScene&, Engine::UI2D::UINodeHandle) override {}
};

class CountingLayout final : public Engine::UI2D::IUILayoutEngine {
public:
    Engine::UI2D::UILayoutResult UpdateLayout(
        Engine::UI2D::UIScene& scene, const Engine::UI2D::UILayoutContext& context) override {
        ++calls;
        return engine.UpdateLayout(scene, context);
    }
    Engine::UI2D::BasicLayoutEngine engine;
    int calls = 0;
};

struct RuntimeServices {
    Engine::UI2D::UI2DServices Get() { return {layout, hit, router, renderer}; }
    CountingLayout layout;
    NullHitTester hit;
    NullRouter router;
    Engine::UI2D::NullUIAnimator animator;
    Engine::UI2D::EmptyUIRenderBuilder renderer;
};

void TestBasicSizeAndEdges() {
    using namespace Engine;
    using namespace Engine::UI2D;
    BasicLayoutEngine engine;

    {
        UIScene scene;
        const auto child = scene.CreateNode("fixed");
        scene.SetLayout(child, Fixed(120, 45));
        Layout(engine, scene, {400, 300});
        Expect(Near(Rect(scene, child), {0, 0, 120, 45}), "1. Fixed width and height");
    }
    {
        UIScene scene;
        const auto child = scene.CreateNode("auto-shape");
        SetPositionAndSize(scene, child, {}, {37, 29});
        Layout(engine, scene);
        Expect(Near(scene.TryGet(child)->layoutState.desiredSize, {37, 29}), "2. Auto uses authored visual size");
    }
    {
        UIScene scene;
        SetMode(scene, scene.Root(), UILayoutMode::Overlay);
        const auto child = scene.CreateNode("stretch");
        auto layout = scene.TryGet(child)->layout;
        layout.sizeRule.width.mode = UISizeMode::Stretch;
        layout.sizeRule.height.mode = UISizeMode::Stretch;
        scene.SetLayout(child, layout);
        Layout(engine, scene, {320, 180});
        Expect(Near(Rect(scene, child), {0, 0, 320, 180}), "3. Stretch fills parent content");
    }
    {
        UIScene scene;
        const auto child = scene.CreateNode("minimum");
        SetPositionAndSize(scene, child, {}, {10, 12});
        auto layout = scene.TryGet(child)->layout;
        layout.sizeRule.minSize = {40, 30};
        scene.SetLayout(child, layout);
        Layout(engine, scene);
        Expect(Near(Rect(scene, child).width, 40) && Near(Rect(scene, child).height, 30), "4. MinSize clamps desired size");
    }
    {
        UIScene scene;
        const auto child = scene.CreateNode("maximum");
        SetPositionAndSize(scene, child, {}, {100, 90});
        auto layout = scene.TryGet(child)->layout;
        layout.sizeRule.maxSize = {60, 55};
        scene.SetLayout(child, layout);
        Layout(engine, scene);
        Expect(Near(Rect(scene, child).width, 60) && Near(Rect(scene, child).height, 55), "5. MaxSize clamps desired size");
    }
    {
        UIScene scene;
        const auto child = scene.CreateNode("negative");
        scene.SetLayout(child, Fixed(-20, -10));
        Layout(engine, scene);
        Expect(Near(Rect(scene, child).width, 0) && Near(Rect(scene, child).height, 0), "6. Negative fixed sizes normalize to zero");
    }
    {
        UIScene scene;
        const auto child = scene.CreateNode("hidden");
        scene.SetLayout(child, Fixed(20, 20));
        scene.SetVisibility(child, false);
        Layout(engine, scene);
        Expect(Near(scene.TryGet(child)->layoutState.desiredSize, {}) && Near(Rect(scene, child).width, 0), "7. Invisible nodes measure to zero");
    }
    {
        UIScene scene;
        const auto child = scene.CreateNode("margin");
        auto layout = Fixed(20, 10);
        layout.margin = {7, 3, 9, 4};
        scene.SetLayout(child, layout);
        Layout(engine, scene);
        Expect(Near(scene.TryGet(child)->layoutState.desiredSize, {20, 10}) && Near(Rect(scene, child).x, 7), "8. Margin stays outside node size");
    }
    {
        UIScene scene;
        auto rootLayout = scene.TryGet(scene.Root())->layout;
        rootLayout.padding = {10, 20, 30, 40};
        scene.SetLayout(scene.Root(), rootLayout);
        Layout(engine, scene, {200, 150});
        Expect(Near(scene.TryGet(scene.Root())->layoutState.contentRect, {10, 20, 160, 90}), "9. Padding shrinks contentRect only");
    }
    {
        UIScene scene;
        SetMode(scene, scene.Root(), UILayoutMode::HorizontalStack);
        const auto a = scene.CreateNode("a");
        const auto b = scene.CreateNode("b");
        auto la = Fixed(20, 10); la.margin = {2, 0, 3, 0};
        auto lb = Fixed(10, 10); lb.margin = {5, 0, 7, 0};
        scene.SetLayout(a, la); scene.SetLayout(b, lb);
        Layout(engine, scene, {100, 20});
        Expect(Near(Rect(scene, a).x, 2) && Near(Rect(scene, b).x, 30), "10. Stack placement includes child margins");
    }
    {
        UIScene scene;
        Layout(engine, scene, {123, 456});
        Expect(Near(Rect(scene, scene.Root()), {0, 0, 123, 456}), "11. Root uses logical window size");
    }
}

void TestAbsoluteAndAnchor() {
    using namespace Engine;
    using namespace Engine::UI2D;
    BasicLayoutEngine engine;

    {
        UIScene scene;
        const auto child = scene.CreateNode("absolute");
        scene.SetLayout(child, Fixed(30, 40));
        SetPositionAndSize(scene, child, {15, 25}, {});
        Layout(engine, scene);
        Expect(Near(Rect(scene, child), {15, 25, 30, 40}), "12. Absolute child position");
    }
    {
        UIScene scene;
        const auto parent = scene.CreateNode("auto-parent");
        const auto child = scene.CreateNode("child", parent);
        scene.SetLayout(child, Fixed(40, 20));
        SetPositionAndSize(scene, child, {10, 5}, {});
        Layout(engine, scene);
        Expect(Near(scene.TryGet(parent)->layoutState.desiredSize, {50, 25}), "13. Absolute Auto parent bounds one child");
    }
    {
        UIScene scene;
        const auto parent = scene.CreateNode("auto-parent");
        const auto a = scene.CreateNode("a", parent);
        const auto b = scene.CreateNode("b", parent);
        scene.SetLayout(a, Fixed(20, 10)); scene.SetLayout(b, Fixed(10, 30));
        SetPositionAndSize(scene, a, {10, 5}, {}); SetPositionAndSize(scene, b, {50, 20}, {});
        Layout(engine, scene);
        Expect(Near(scene.TryGet(parent)->layoutState.desiredSize, {60, 50}), "14. Absolute Auto bounds multiple children");
    }

    auto anchorScene = [](UIAnchor anchor, UISizeMode widthMode, UISizeMode heightMode) {
        UIScene scene;
        SetMode(scene, scene.Root(), UILayoutMode::Anchor);
        const auto child = scene.CreateNode("anchored");
        auto layout = Fixed(20, 10);
        layout.anchor = anchor;
        layout.sizeRule.width.mode = widthMode;
        layout.sizeRule.height.mode = heightMode;
        scene.SetLayout(child, layout);
        return std::pair<UIScene, UINodeHandle>{std::move(scene), child};
    };

    {
        auto [scene, child] = anchorScene({0.5f, 0.5f, 0.5f, 0.5f}, UISizeMode::Fixed, UISizeMode::Fixed);
        Layout(engine, scene, {200, 100});
        Expect(Near(Rect(scene, child), {100, 50, 20, 10}), "15. Point anchor");
    }
    {
        auto [scene, child] = anchorScene({0, 0, 1, 0}, UISizeMode::Stretch, UISizeMode::Fixed);
        Layout(engine, scene, {200, 100});
        Expect(Near(Rect(scene, child).width, 200), "16. Horizontal stretch anchor");
    }
    {
        auto [scene, child] = anchorScene({0, 0, 0, 1}, UISizeMode::Fixed, UISizeMode::Stretch);
        Layout(engine, scene, {200, 100});
        Expect(Near(Rect(scene, child).height, 100), "17. Vertical stretch anchor");
    }
    {
        auto [scene, child] = anchorScene({0, 0, 1, 1}, UISizeMode::Stretch, UISizeMode::Stretch);
        Layout(engine, scene, {200, 100});
        Expect(Near(Rect(scene, child), {0, 0, 200, 100}), "18. Four-direction stretch anchor");
    }
    {
        auto [scene, child] = anchorScene({0, 0, 1, 1}, UISizeMode::Stretch, UISizeMode::Stretch);
        auto layout = scene.TryGet(child)->layout;
        layout.offsetMin = {10, 5}; layout.offsetMax = {-20, -15};
        scene.SetLayout(child, layout);
        Layout(engine, scene, {200, 100});
        Expect(Near(Rect(scene, child), {10, 5, 170, 80}), "19. Anchor offsetMin and offsetMax");
    }
    {
        auto [scene, child] = anchorScene({0, 0, 1, 1}, UISizeMode::Stretch, UISizeMode::Stretch);
        auto layout = scene.TryGet(child)->layout;
        layout.margin = {3, 4, 5, 6};
        scene.SetLayout(child, layout);
        Layout(engine, scene, {200, 100});
        Expect(Near(Rect(scene, child), {3, 4, 192, 90}), "20. Anchor combines margins");
    }
    {
        auto [scene, child] = anchorScene({1.5f, -1.0f, -0.5f, 2.0f}, UISizeMode::Stretch, UISizeMode::Stretch);
        Layout(engine, scene, {200, 100});
        Expect(Near(Rect(scene, child), {0, 0, 200, 100}), "21. Invalid anchors clamp and reorder");
    }
}

void TestOverlayAndStacks() {
    using namespace Engine;
    using namespace Engine::UI2D;
    BasicLayoutEngine engine;

    auto overlayRect = [&](UIHorizontalAlignment horizontal, UIVerticalAlignment vertical, UISizeMode width, UISizeMode height) {
        UIScene scene;
        SetMode(scene, scene.Root(), UILayoutMode::Overlay);
        const auto child = scene.CreateNode("overlay");
        auto layout = Fixed(20, 10);
        layout.horizontalAlignment = horizontal; layout.verticalAlignment = vertical;
        layout.sizeRule.width.mode = width; layout.sizeRule.height.mode = height;
        scene.SetLayout(child, layout);
        Layout(engine, scene, {100, 80});
        return Rect(scene, child);
    };
    Expect(Near(overlayRect(UIHorizontalAlignment::Start, UIVerticalAlignment::Start, UISizeMode::Fixed, UISizeMode::Fixed), {0, 0, 20, 10}), "22. Overlay Start alignment");
    Expect(Near(overlayRect(UIHorizontalAlignment::Center, UIVerticalAlignment::Center, UISizeMode::Fixed, UISizeMode::Fixed), {40, 35, 20, 10}), "23. Overlay Center alignment");
    Expect(Near(overlayRect(UIHorizontalAlignment::End, UIVerticalAlignment::End, UISizeMode::Fixed, UISizeMode::Fixed), {80, 70, 20, 10}), "24. Overlay End alignment");
    Expect(Near(overlayRect(UIHorizontalAlignment::Stretch, UIVerticalAlignment::Stretch, UISizeMode::Stretch, UISizeMode::Stretch), {0, 0, 100, 80}), "25. Overlay Stretch size mode");
    {
        UIScene scene;
        SetMode(scene, scene.Root(), UILayoutMode::Overlay);
        const auto a = scene.CreateNode("a"); const auto b = scene.CreateNode("b");
        scene.SetLayout(a, Fixed(20, 10)); scene.SetLayout(b, Fixed(30, 15));
        Layout(engine, scene);
        Expect(Near(Rect(scene, a).x, Rect(scene, b).x) && Near(Rect(scene, a).y, Rect(scene, b).y), "26. Overlay children overlap");
    }
    {
        UIScene scene;
        SetMode(scene, scene.Root(), UILayoutMode::HorizontalStack);
        const auto a = scene.CreateNode("a"); const auto b = scene.CreateNode("b");
        scene.SetLayout(a, Fixed(20, 10)); scene.SetLayout(b, Fixed(30, 10));
        Layout(engine, scene, {100, 40});
        Expect(Near(Rect(scene, a).x, 0) && Near(Rect(scene, b).x, 20), "27. HorizontalStack left-to-right order");
        auto root = scene.TryGet(scene.Root())->layout; root.spacing = 7; scene.SetLayout(scene.Root(), root);
        Layout(engine, scene, {100, 40});
        Expect(Near(Rect(scene, b).x, 27), "28. HorizontalStack spacing");
        auto lb = scene.TryGet(b)->layout; lb.margin = {4, 0, 6, 0}; scene.SetLayout(b, lb);
        Layout(engine, scene, {100, 40});
        Expect(Near(Rect(scene, b).x, 31), "29. HorizontalStack margins");
        lb.verticalAlignment = UIVerticalAlignment::Center; scene.SetLayout(b, lb);
        Layout(engine, scene, {100, 40});
        Expect(Near(Rect(scene, b).y, 15), "30. HorizontalStack vertical alignment");
    }
    {
        UIScene scene;
        SetMode(scene, scene.Root(), UILayoutMode::HorizontalStack);
        const auto fixed = scene.CreateNode("fixed"); const auto stretch = scene.CreateNode("stretch");
        scene.SetLayout(fixed, Fixed(50, 10));
        auto ls = Fixed(0, 10); ls.sizeRule.width.mode = UISizeMode::Stretch; scene.SetLayout(stretch, ls);
        Layout(engine, scene, {300, 20});
        Expect(Near(Rect(scene, stretch).width, 250), "31. One horizontal stretch child gets remainder");
    }
    {
        UIScene scene;
        SetMode(scene, scene.Root(), UILayoutMode::HorizontalStack);
        const auto a = scene.CreateNode("a"); const auto b = scene.CreateNode("b");
        auto stretch = Fixed(0, 10); stretch.sizeRule.width.mode = UISizeMode::Stretch;
        scene.SetLayout(a, stretch); scene.SetLayout(b, stretch);
        Layout(engine, scene, {300, 20});
        Expect(Near(Rect(scene, a).width, 150) && Near(Rect(scene, b).width, 150), "32. Horizontal stretch children share equally");
        scene.SetVisibility(a, false); Layout(engine, scene, {300, 20});
        Expect(Near(Rect(scene, b).x, 0) && Near(Rect(scene, b).width, 300), "33. Hidden horizontal child occupies no space");
    }
    {
        UIScene scene;
        SetMode(scene, scene.Root(), UILayoutMode::VerticalStack);
        const auto a = scene.CreateNode("a"); const auto b = scene.CreateNode("b");
        scene.SetLayout(a, Fixed(20, 10)); scene.SetLayout(b, Fixed(20, 30));
        Layout(engine, scene, {100, 100});
        Expect(Near(Rect(scene, a).y, 0) && Near(Rect(scene, b).y, 10), "34. VerticalStack top-to-bottom order");
        auto root = scene.TryGet(scene.Root())->layout; root.spacing = 5; scene.SetLayout(scene.Root(), root);
        Layout(engine, scene, {100, 100});
        Expect(Near(Rect(scene, b).y, 15), "35. VerticalStack spacing");
        auto lb = scene.TryGet(b)->layout; lb.horizontalAlignment = UIHorizontalAlignment::Center; scene.SetLayout(b, lb);
        Layout(engine, scene, {100, 100});
        Expect(Near(Rect(scene, b).x, 40), "36. VerticalStack horizontal alignment");
        lb.sizeRule.height.mode = UISizeMode::Stretch; scene.SetLayout(b, lb);
        Layout(engine, scene, {100, 100});
        Expect(Near(Rect(scene, b).height, 85), "37. Vertical stretch child gets remaining height");
        scene.SetVisibility(a, false); Layout(engine, scene, {100, 100});
        Expect(Near(Rect(scene, b).y, 0) && Near(Rect(scene, b).height, 100), "38. Hidden vertical child occupies no space");
    }
}

void TestDirtyRuntimeAndTransforms() {
    using namespace Engine;
    using namespace Engine::UI2D;
    BasicLayoutEngine engine;
    static const ResolvedUITheme theme{};

    {
        UIScene scene; Layout(engine, scene);
        const auto clean = engine.UpdateLayout(scene, {{800, 600}, 1.0f, theme, scene.Root()});
        Expect(clean.measuredNodeCount == 0 && clean.arrangedNodeCount == 0, "39. Clean scene skips Measure and Arrange");
    }
    {
        UIScene scene; const auto child = scene.CreateNode("child");
        SetPositionAndSize(scene, child, {}, {10, 10}); Layout(engine, scene);
        SetPositionAndSize(scene, child, {}, {20, 10}); const auto result = Layout(engine, scene);
        Expect(result.layoutChanged && Near(Rect(scene, child).width, 20), "40. Child size change triggers layout");
    }
    {
        UIScene scene; const auto a = scene.CreateNode("a"); const auto b = scene.CreateNode("b"); const auto child = scene.CreateNode("child", a);
        Layout(engine, scene); scene.ReparentNode(child, b); const auto result = Layout(engine, scene);
        Expect(result.measuredNodeCount == scene.NodeCount() && scene.TryGet(child)->parent == b, "41. Reparent triggers scene layout");
    }
    {
        UIScene scene; const auto child = scene.CreateNode("child"); scene.SetLayout(child, Fixed(10, 10)); Layout(engine, scene);
        scene.SetVisibility(child, false); Layout(engine, scene);
        Expect(Near(Rect(scene, child).width, 0), "42. Visibility change triggers layout");
    }
    {
        RuntimeServices services; UIWindowRuntime runtime({1, 1}, services.Get(), nullptr);
        runtime.Update(0.016, {100, 80}, 1.0f); const int before = services.layout.calls;
        runtime.Update(0.016, {120, 80}, 1.0f);
        Expect(services.layout.calls == before + 1 && Near(Rect(runtime.Scene(), runtime.Scene().Root()).width, 120), "43. Window size change triggers layout");
        const int beforeDpi = services.layout.calls; runtime.Update(0.016, {120, 80}, 1.5f);
        Expect(services.layout.calls == beforeDpi + 1, "44. DPI change triggers layout without physical-pixel math");
    }
    {
        UIScene scene; const auto result = Layout(engine, scene);
        Expect(!scene.HasDirty(UIDirtyFlags::Layout), "45. Layout completion clears Layout Dirty");
        Expect(result.visualInvalidated && result.hitTestInvalidated && scene.HasDirty(UIDirtyFlags::Visual | UIDirtyFlags::HitTest), "46. Changed layout invalidates Visual and HitTest");
    }
    {
        RuntimeServices services; UIWindowRuntime runtime({2, 1}, services.Get(), nullptr); runtime.SetVisible(false);
        runtime.Update(0.016, {100, 80}, 1.0f);
        Expect(services.layout.calls == 0, "47. Hidden UIWindowRuntime skips layout");
    }
    {
        UIScene scene; const auto parent = scene.CreateNode("parent"); const auto child = scene.CreateNode("child", parent);
        scene.SetLayout(parent, Fixed(100, 100)); scene.SetLayout(child, Fixed(10, 10));
        SetPositionAndSize(scene, parent, {10, 20}, {}); SetPositionAndSize(scene, child, {5, 6}, {}); Layout(engine, scene);
        const auto origin = TransformPoint(scene.TryGet(child)->layoutState.computedTransform.localToScene, {});
        Expect(Near(origin, {15, 26}), "48. Parent and child translations compose");
    }
    {
        UIScene scene; const auto child = scene.CreateNode("scale"); scene.SetLayout(child, Fixed(10, 20));
        auto transform = scene.TryGet(child)->transform; transform.scale = {2, 3}; scene.SetTransform(child, transform); Layout(engine, scene);
        Expect(Near(scene.TryGet(child)->layoutState.sceneRect.width, 20) && Near(scene.TryGet(child)->layoutState.sceneRect.height, 60), "49. Scale affects final scene AABB");
    }
    {
        UIScene scene; const auto child = scene.CreateNode("rotation"); scene.SetLayout(child, Fixed(10, 20));
        auto transform = scene.TryGet(child)->transform; transform.rotationRadians = 3.14159265f * 0.5f; scene.SetTransform(child, transform); Layout(engine, scene);
        Expect(Near(scene.TryGet(child)->layoutState.sceneRect.width, 20) && Near(scene.TryGet(child)->layoutState.sceneRect.height, 10), "50. Rotation produces correct scene AABB");
    }
    {
        UIScene scene; const auto child = scene.CreateNode("pivot"); scene.SetLayout(child, Fixed(20, 20));
        auto transform = scene.TryGet(child)->transform; transform.pivot = {0.5f, 0.5f}; transform.rotationRadians = 3.14159265f; scene.SetTransform(child, transform); Layout(engine, scene);
        Expect(Near(scene.TryGet(child)->layoutState.sceneRect, {0, 0, 20, 20}), "51. Pivot rotation stays around node center");
    }
    {
        UIScene scene; const auto child = scene.CreateNode("inverse"); scene.SetLayout(child, Fixed(20, 10));
        auto transform = scene.TryGet(child)->transform; transform.position = {13, 17}; transform.scale = {1.5f, 0.75f}; transform.rotationRadians = 0.3f; scene.SetTransform(child, transform); Layout(engine, scene);
        const auto& computed = scene.TryGet(child)->layoutState.computedTransform;
        const Vec2F local{4, 6}; const Vec2F roundTrip = TransformPoint(computed.sceneToLocal, TransformPoint(computed.localToScene, local));
        Expect(computed.inverseValid && Near(local, roundTrip), "52. localToScene and sceneToLocal are inverse");
    }
    {
        UIScene scene; const auto child = scene.CreateNode("aabb"); scene.SetLayout(child, Fixed(10, 10));
        auto transform = scene.TryGet(child)->transform; transform.rotationRadians = 3.14159265f * 0.25f; scene.SetTransform(child, transform); Layout(engine, scene);
        const auto aabb = scene.TryGet(child)->layoutState.sceneRect;
        Expect(aabb.width > 14.0f && aabb.height > 14.0f, "53. Scene AABB covers rotated corners");
    }
    {
        RuntimeServices firstServices; RuntimeServices secondServices;
        UIWindowRuntime first({10, 1}, firstServices.Get(), nullptr); UIWindowRuntime second({11, 1}, secondServices.Get(), nullptr);
        first.Update(0.016, {300, 200}, 1.0f); second.Update(0.016, {640, 480}, 1.0f);
        Expect(Near(Rect(first.Scene(), first.Scene().Root()).width, 300) && Near(Rect(second.Scene(), second.Scene().Root()).width, 640), "54. UI windows keep distinct logical sizes");
        const auto secondBefore = Rect(second.Scene(), second.Scene().Root()); first.Update(0.016, {320, 200}, 1.0f);
        Expect(Near(Rect(second.Scene(), second.Scene().Root()), secondBefore), "55. One window layout change does not contaminate another");
    }
}

void TestResourceMetricsAndDemo() {
    using namespace Engine;
    using namespace Engine::UI2D;
    TextureRegistry2D textures;
    const std::vector<std::uint8_t> pixels(30 * 20 * 4, 255);
    const TextureHandle texture = textures.CreateTextureRGBA8(30, 20, pixels, AlphaMode::Premultiplied);
    BasicLayoutEngine engine(&textures);
    UIScene scene;
    const auto image = scene.CreateNode("image");
    scene.SetVisual(image, UIImageVisual{texture});
    Layout(engine, scene);
    Expect(Near(scene.TryGet(image)->layoutState.desiredSize, {30, 20}), "Auto Image uses texture logical metrics");

    FakeTextLayouts textLayouts;
    BasicLayoutEngine textEngine(nullptr, &textLayouts);
    UIScene textScene;
    const auto text = textScene.CreateNode("text");
    textScene.SetVisual(text, UITextVisual{{1, 1}});
    Layout(textEngine, textScene);
    Expect(Near(textScene.TryGet(text)->layoutState.desiredSize, {42, 18}), "Auto Text uses text-layout logical metrics");

    UIScene nineSliceScene;
    const auto nineSlice = nineSliceScene.CreateNode("nine-slice");
    UINineSliceVisual nineSliceVisual;
    nineSliceVisual.borders = {4, 5, 6, 7};
    nineSliceScene.SetVisual(nineSlice, nineSliceVisual);
    Layout(engine, nineSliceScene);
    Expect(Near(nineSliceScene.TryGet(nineSlice)->layoutState.desiredSize, {10, 12}), "Auto NineSlice respects minimum border size");

    UIScene demo;
    SetMode(demo, demo.Root(), UILayoutMode::Overlay);
    const auto topBar = demo.CreateNode("TopBar");
    auto topLayout = Fixed(0, 64); topLayout.sizeRule.width.mode = UISizeMode::Stretch; topLayout.mode = UILayoutMode::HorizontalStack; topLayout.spacing = 8;
    demo.SetLayout(topBar, topLayout);
    const auto logo = demo.CreateNode("Logo", topBar); demo.SetLayout(logo, Fixed(48, 48));
    const auto title = demo.CreateNode("Title", topBar); auto titleLayout = Fixed(0, 48); titleLayout.sizeRule.width.mode = UISizeMode::Stretch; demo.SetLayout(title, titleLayout);
    const auto status = demo.CreateNode("Status", topBar); demo.SetLayout(status, Fixed(120, 40));
    const auto center = demo.CreateNode("CenterPanel"); auto centerLayout = Fixed(0, 0); centerLayout.sizeRule.width.mode = UISizeMode::Stretch; centerLayout.sizeRule.height.mode = UISizeMode::Stretch; centerLayout.mode = UILayoutMode::Overlay; centerLayout.margin = {0, 64, 0, 80}; demo.SetLayout(center, centerLayout);
    const auto preview = demo.CreateNode("Preview", center); auto previewLayout = Fixed(300, 300); previewLayout.horizontalAlignment = UIHorizontalAlignment::Center; previewLayout.verticalAlignment = UIVerticalAlignment::Center; demo.SetLayout(preview, previewLayout);
    const auto bottom = demo.CreateNode("BottomPanel"); auto bottomLayout = Fixed(280, 64); bottomLayout.mode = UILayoutMode::HorizontalStack; bottomLayout.spacing = 8; bottomLayout.horizontalAlignment = UIHorizontalAlignment::Center; bottomLayout.verticalAlignment = UIVerticalAlignment::End; demo.SetLayout(bottom, bottomLayout);
    for (int i = 0; i < 4; ++i) { const auto item = demo.CreateNode("Item", bottom); demo.SetLayout(item, Fixed(64, 64)); }
    Layout(engine, demo, {800, 600});
    const std::string dump = engine.DumpScene(demo);
    std::cout << "--- Layout demo ---\n" << dump;
    Expect(dump.find("TopBar") != std::string::npos && Near(Rect(demo, preview).width, 300), "Layout demo scene produces reusable final rectangles");
}

} // namespace

int main() {
    TestBasicSizeAndEdges();
    TestAbsoluteAndAnchor();
    TestOverlayAndStacks();
    TestDirtyRuntimeAndTransforms();
    TestResourceMetricsAndDemo();
    std::cout << (failures == 0 ? "UI2D layout tests passed\n" : "UI2D layout tests failed\n");
    return failures == 0 ? 0 : 1;
}
