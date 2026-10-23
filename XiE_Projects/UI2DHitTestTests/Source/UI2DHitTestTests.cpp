#include "UI2D/Animation/IUIAnimator.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/HitTest/UIHitGeometry.h"
#include "UI2D/Input/IUIFocusManager.h"
#include "UI2D/Input/IUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/IUIRenderBuilder.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <span>
#include <string>

namespace {

int failures = 0;

void Expect(bool value, const char* message) {
    std::cout << (value ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!value) ++failures;
}

bool Near(float a, float b, float epsilon = 0.02f) { return std::abs(a - b) <= epsilon; }
bool Near(Engine::Vec2F a, Engine::Vec2F b) { return Near(a.x, b.x) && Near(a.y, b.y); }

Engine::UI2D::UILayoutParams Fixed(float width, float height) {
    Engine::UI2D::UILayoutParams layout;
    layout.sizeRule.width = {Engine::UI2D::UISizeMode::Fixed, width};
    layout.sizeRule.height = {Engine::UI2D::UISizeMode::Fixed, height};
    return layout;
}

void Place(
    Engine::UI2D::UIScene& scene,
    Engine::UI2D::UINodeHandle node,
    float x,
    float y,
    float width,
    float height) {
    scene.SetLayout(node, Fixed(width, height));
    auto transform = scene.TryGet(node)->transform;
    transform.position = {x, y};
    scene.SetTransform(node, transform);
}

void Layout(
    Engine::UI2D::BasicLayoutEngine& engine,
    Engine::UI2D::UIScene& scene,
    Engine::Vec2F size = {800.0f, 600.0f}) {
    static const Engine::UI2D::ResolvedUITheme theme{};
    const auto result = engine.UpdateLayout(scene, {size, 1.0f, theme, scene.Root()});
    Expect(result.Succeeded(), "layout prerequisite succeeds");
}

Engine::UI2D::UIHitTestResult Hit(
    const Engine::UI2D::BasicUIHitTester& tester,
    const Engine::UI2D::UIScene& scene,
    Engine::Vec2F point,
    Engine::UI2D::UINodeHandle root = {}) {
    Engine::UI2D::UIHitTestContext context;
    context.root = root;
    return tester.HitTest(scene, context, point);
}

void TestGeometry() {
    using namespace Engine;
    using namespace Engine::UI2D;
    const RectF rect{0, 0, 100, 50};
    Expect(PointInRect({20, 20}, rect), "1. Rect interior hits");
    Expect(!PointInRect({101, 20}, rect), "2. Rect exterior rejects");
    Expect(PointInRect({100, 50}, rect), "3. Rect boundary hits");
    Expect(!PointInRect({0, 0}, {0, 0, 0, 50}), "4. Zero-size Rect rejects");
    Expect(PointInRoundedRect({50, 25}, rect, 20), "5. RoundedRect center hits");
    Expect(!PointInRoundedRect({0, 0}, rect, 20), "6. RoundedRect corner exterior rejects");
    Expect(PointInRoundedRect({0, 20}, rect, 20), "7. RoundedRect arc boundary hits");
    Expect(!PointInRoundedRect({0, 0}, rect, 500), "8. RoundedRect radius clamps");
    Expect(PointInCircle({25, 25}, {25, 25}, 25), "9. Circle center hits");
    Expect(PointInCircle({50, 25}, {25, 25}, 25), "10. Circle boundary hits");
    Expect(!PointInCircle({51, 25}, {25, 25}, 25), "11. Circle exterior rejects");
    Expect(PointInHitShape({50, 49}, rect, {UIHitShapeType::Circle}), "12. Non-square Circle uses shortest side");
    Expect(PointInRing({20, 0}, {}, 10, 30), "13. Ring band hits");
    Expect(!PointInRing({}, {}, 10, 30), "14. Ring center hole rejects");
    Expect(!PointInRing({31, 0}, {}, 10, 30), "15. Ring exterior rejects");
    Expect(PointInHitShape({60, 25}, rect, {UIHitShapeType::Ring, 0, 500, 10}),
        "16. Invalid Ring radii normalize safely");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    Expect(!PointInRect({nan, 0}, rect) && !PointInCircle({}, {}, nan), "geometry rejects NaN safely");
}

void TestTransformsAndShapes() {
    using namespace Engine;
    using namespace Engine::UI2D;
    BasicLayoutEngine layout;
    BasicUIHitTester tester;
    UIScene scene;
    const auto translated = scene.CreateNode("translated");
    Place(scene, translated, 100, 80, 50, 40);
    Layout(layout, scene);
    auto result = Hit(tester, scene, {110, 90});
    Expect(result.target == translated && Near(result.targetLocalPosition, {10, 10}), "17. Translation uses sceneToLocal");

    auto transform = scene.TryGet(translated)->transform;
    transform.scale = {2, 2};
    scene.SetTransform(translated, transform);
    Layout(layout, scene);
    result = Hit(tester, scene, {190, 100});
    Expect(result.target == translated && Near(result.targetLocalPosition.x, 45), "18. Scale uses sceneToLocal");

    UIScene rotatedScene;
    const auto rotated = rotatedScene.CreateNode("rotated");
    Place(rotatedScene, rotated, 200, 100, 100, 40);
    transform = rotatedScene.TryGet(rotated)->transform;
    transform.pivot = {0.5f, 0.5f};
    transform.rotationRadians = 3.14159265f * 0.5f;
    rotatedScene.SetTransform(rotated, transform);
    Layout(layout, rotatedScene);
    const Vec2F sceneCenter = TransformPoint(
        rotatedScene.TryGet(rotated)->layoutState.computedTransform.localToScene, {50, 20});
    result = Hit(tester, rotatedScene, sceneCenter);
    Expect(result.target == rotated && Near(result.targetLocalPosition, {50, 20}), "19-20. Rotation and Pivot use precomputed inverse");

    const auto child = rotatedScene.CreateNode("nested", rotated);
    Place(rotatedScene, child, 10, 5, 20, 10);
    Layout(layout, rotatedScene);
    const Vec2F nestedPoint = TransformPoint(
        rotatedScene.TryGet(child)->layoutState.computedTransform.localToScene, {3, 4});
    result = Hit(tester, rotatedScene, nestedPoint);
    Expect(result.target == child && Near(result.targetLocalPosition, {3, 4}), "21-22. Nested transforms round-trip correctly");

    transform = rotatedScene.TryGet(child)->transform;
    transform.scale = {0, 1};
    rotatedScene.SetTransform(child, transform);
    Layout(layout, rotatedScene);
    UIHitTestStats stats;
    UIHitTestContext context; context.stats = &stats;
    result = tester.HitTest(rotatedScene, context, nestedPoint);
    Expect(result.target != child && stats.invalidTransformRejected == 1, "23. Zero scale rejects without crashing");

    UIScene shapes;
    const auto rounded = shapes.CreateNode("rounded"); Place(shapes, rounded, 0, 0, 100, 100);
    shapes.SetHitShape(rounded, {UIHitShapeType::RoundedRect, 30});
    const auto circle = shapes.CreateNode("circle"); Place(shapes, circle, 120, 0, 100, 100);
    shapes.SetHitShape(circle, {UIHitShapeType::Circle});
    const auto ring = shapes.CreateNode("ring"); Place(shapes, ring, 240, 0, 100, 100);
    shapes.SetHitShape(ring, {UIHitShapeType::Ring, 0, 20, 50});
    Layout(layout, shapes);
    Expect(!Hit(tester, shapes, {1, 1}).hit && Hit(tester, shapes, {50, 50}).target == rounded,
        "RoundedRect exact geometry is used");
    Expect(!Hit(tester, shapes, {121, 1}).hit && Hit(tester, shapes, {170, 50}).target == circle,
        "Circle exact geometry is used");
    Expect(!Hit(tester, shapes, {290, 50}).hit && Hit(tester, shapes, {330, 50}).target == ring,
        "Ring exact geometry preserves center hole");
}

void TestClip() {
    using namespace Engine;
    using namespace Engine::UI2D;
    BasicLayoutEngine layout;
    BasicUIHitTester tester;
    UIScene scene;
    const auto clip = scene.CreateNode("clip"); Place(scene, clip, 20, 20, 100, 100);
    scene.SetHitTestVisible(clip, false); scene.SetClipChildren(clip, true);
    const auto child = scene.CreateNode("child", clip); Place(scene, child, 70, 20, 100, 50);
    Layout(layout, scene);
    Expect(Hit(tester, scene, {100, 60}).target == child, "24. Child inside parent Clip hits");
    Expect(!Hit(tester, scene, {150, 60}).hit, "25. Child outside parent Clip rejects");

    const auto nestedClip = scene.CreateNode("nestedClip", clip); Place(scene, nestedClip, 10, 10, 50, 50);
    scene.SetHitTestVisible(nestedClip, false); scene.SetClipChildren(nestedClip, true);
    const auto nested = scene.CreateNode("nested", nestedClip); Place(scene, nested, 20, 20, 50, 50);
    Layout(layout, scene);
    Expect(Hit(tester, scene, {55, 55}).target == nested && Hit(tester, scene, {90, 90}).target != nested,
        "26. All nested Clips must pass");

    Place(scene, child, 0, 0, 100, 100);
    scene.SetHitShape(clip, {UIHitShapeType::RoundedRect, 30}); Layout(layout, scene);
    Expect(Hit(tester, scene, {21, 21}).target == child, "27. Rounded HitShape still uses shared Rect Clip");
    scene.SetHitShape(clip, {UIHitShapeType::Circle}); Layout(layout, scene);
    Expect(Hit(tester, scene, {21, 21}).target == child, "28. Circle HitShape still uses shared Rect Clip");

    auto transform = scene.TryGet(clip)->transform;
    transform.rotationRadians = 3.14159265f * 0.25f; transform.pivot = {0.5f, 0.5f};
    scene.SetTransform(clip, transform); Layout(layout, scene);
    const Vec2F point = TransformPoint(scene.TryGet(child)->layoutState.computedTransform.localToScene, {10, 10});
    Expect(Hit(tester, scene, point).target == child, "29. Transformed parent Clip uses shared sceneRect");
}

void TestStateSortingAndRoute() {
    using namespace Engine;
    using namespace Engine::UI2D;
    BasicLayoutEngine layout;
    BasicUIHitTester tester;
    UIScene scene;
    const auto lower = scene.CreateNode("lower"); Place(scene, lower, 10, 10, 100, 100);
    const auto upper = scene.CreateNode("upper"); Place(scene, upper, 10, 10, 100, 100);
    scene.SetZOrder(lower, 10); scene.SetZOrder(upper, 20);
    Layout(layout, scene);
    Expect(Hit(tester, scene, {20, 20}).target == upper, "37. Higher zOrder wins");
    scene.SetZOrder(lower, 20);
    Expect(Hit(tester, scene, {20, 20}).target == upper, "38. Equal zOrder uses later insertion");

    const auto child = scene.CreateNode("child", upper); Place(scene, child, 0, 0, 50, 50); Layout(layout, scene);
    auto result = Hit(tester, scene, {20, 20});
    Expect(result.target == child, "39-41. Parent-first shared painter traversal puts child above parent");
    Expect(result.hit && result.route.size() == 3 && result.route.front() == scene.Root() &&
        result.route.back() == result.target, "42-43. Route is Root to Target and ends in target");
    result = Hit(tester, scene, {700, 500});
    Expect(!result.hit && !result.target.IsValid() && result.route.empty(), "44. Miss has invalid target and empty route");

    const auto subtree = scene.CreateNode("subtree"); Place(scene, subtree, 300, 0, 100, 100);
    const auto inside = scene.CreateNode("inside", subtree); Place(scene, inside, 0, 0, 100, 100);
    const auto outside = scene.CreateNode("outside"); Place(scene, outside, 300, 0, 100, 100); scene.SetZOrder(outside, 100);
    Layout(layout, scene);
    Expect(Hit(tester, scene, {310, 10}, subtree).target == inside, "46. Context root limits search to one subtree");

    UIScene states;
    const auto visible = states.CreateNode("visible"); Place(states, visible, 0, 0, 100, 100);
    const auto stateChild = states.CreateNode("stateChild", visible); Place(states, stateChild, 0, 0, 50, 50);
    Layout(layout, states);
    states.SetVisibility(stateChild, false); Layout(layout, states);
    Expect(Hit(tester, states, {10, 10}).target == visible, "30. visible=false node rejects");
    states.SetVisibility(stateChild, true); states.SetVisibility(visible, false); Layout(layout, states);
    Expect(!Hit(tester, states, {10, 10}).hit, "31. Invisible ancestor rejects subtree");
    states.SetVisibility(visible, true); states.SetEnabled(stateChild, false); Layout(layout, states);
    Expect(Hit(tester, states, {10, 10}).target == visible, "32. enabled=false node rejects");
    states.SetEnabled(stateChild, true); states.SetEnabled(visible, false); Layout(layout, states);
    Expect(!Hit(tester, states, {10, 10}).hit, "33. Disabled ancestor rejects subtree");
    states.SetEnabled(visible, true); states.SetHitTestVisible(stateChild, false); Layout(layout, states);
    Expect(Hit(tester, states, {10, 10}).target == visible, "34. hitTestVisible=false rejects node");
    states.SetHitTestVisible(stateChild, true); states.TryGet(stateChild)->opacity = 0.0f;
    states.MarkDirty(stateChild, UIDirtyFlags::Layout); Layout(layout, states);
    Expect(Hit(tester, states, {10, 10}).target == stateChild, "35. opacity=0 does not imply pass-through");
    states.SetHitShape(stateChild, {UIHitShapeType::None});
    Expect(Hit(tester, states, {10, 10}).target == visible, "36. HitShape None rejects node");

    const auto stale = states.CreateNode("stale"); Place(states, stale, 200, 0, 50, 50); Layout(layout, states);
    states.DestroyNode(stale); Layout(layout, states);
    result = Hit(tester, states, {210, 10});
    Expect(!result.hit && result.route.empty(), "45. Destroyed handles never appear in route");
}

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

struct RuntimeServices {
    Engine::UI2D::UI2DServices Get() { return {layout, hit, router, render}; }
    Engine::UI2D::BasicLayoutEngine layout;
    Engine::UI2D::BasicUIHitTester hit;
    NullRouter router;
    Engine::UI2D::NullUIAnimator animator;
    Engine::UI2D::EmptyUIRenderBuilder render;
};

void TestDirtyLifecycleRuntimeAndPerformance() {
    using namespace Engine;
    using namespace Engine::UI2D;
    BasicLayoutEngine layout;
    BasicUIHitTester tester;
    UIScene scene;
    const auto first = scene.CreateNode("first"); Place(scene, first, 0, 0, 20, 20); Layout(layout, scene);
    scene.ClearDirty(UIDirtyFlags::HitTest);
    const auto revision = scene.HitTestRevision();
    auto params = scene.TryGet(first)->layout; params.sizeRule.width.value = 40; scene.SetLayout(first, params);
    Expect(scene.HasDirty(UIDirtyFlags::HitTest) && scene.HitTestRevision() > revision,
        "47. Layout change marks HitTest dirty and advances revision");
    Layout(layout, scene); scene.ClearDirty(UIDirtyFlags::HitTest);
    auto transform = scene.TryGet(first)->transform; transform.position = {30, 0}; scene.SetTransform(first, transform);
    Expect(scene.HasDirty(UIDirtyFlags::HitTest), "48. Transform change marks HitTest dirty");
    Layout(layout, scene); scene.ClearDirty(UIDirtyFlags::HitTest); scene.SetZOrder(first, 4);
    Expect(scene.HasDirty(UIDirtyFlags::HitTest), "49. zOrder change marks HitTest dirty");

    const auto parent = scene.CreateNode("parent"); Place(scene, parent, 100, 0, 100, 100);
    scene.ReparentNode(first, parent); Layout(layout, scene);
    Expect(Hit(tester, scene, {135, 5}).target == first, "50. Reparent refreshes transform and result");
    scene.DestroyNode(first); Layout(layout, scene);
    Expect(Hit(tester, scene, {135, 5}).target != first, "51. Destroy removes old handle from traversal");

    RuntimeServices servicesA;
    RuntimeServices servicesB;
    UIWindowRuntime runtimeA({1, 1}, servicesA.Get(), nullptr);
    UIWindowRuntime runtimeB({2, 1}, servicesB.Get(), nullptr);
    const auto nodeA = runtimeA.Scene().CreateNode("A"); Place(runtimeA.Scene(), nodeA, 0, 0, 50, 50);
    const auto nodeB = runtimeB.Scene().CreateNode("B"); Place(runtimeB.Scene(), nodeB, 100, 0, 50, 50);
    Expect(!runtimeA.HitTest({10, 10}).hit, "UIWindowRuntime rejects queries while Layout is dirty");
    runtimeA.Update(0.0, {200, 100}, 1.0f); runtimeB.Update(0.0, {200, 100}, 1.0f);
    Expect(runtimeA.HitTest({10, 10}).target == nodeA && runtimeB.HitTest({10, 10}).target != nodeB,
        "52. UIWindowRuntime hit state is isolated per window");
    runtimeA.SetVisible(false);
    Expect(!runtimeA.HitTest({10, 10}).hit, "53. Hidden UIWindowRuntime always misses");

    UIScene many;
    for (int i = 0; i < 200; ++i) {
        const auto node = many.CreateNode("many"); Place(many, node, static_cast<float>(i * 3), 200, 2, 2);
    }
    const auto top = many.CreateNode("top"); Place(many, top, 0, 0, 20, 20); many.SetZOrder(top, 100);
    Layout(layout, many, {800, 600});
    UIHitTestStats stats; UIHitTestContext context; context.stats = &stats;
    const auto result = tester.HitTest(many, context, {5, 5});
    Expect(result.target == top && stats.visitedNodes == 1, "54-55. Reverse painter order stops at first top hit");
    context.debugDumpEnabled = true; (void)tester.HitTest(many, context, {799, 599});
    Expect(stats.aabbRejected > 0 && tester.LastDebugDump().find("[HitTest]") != std::string::npos,
        "AABB rejection statistics and opt-in dump are available");
    UIScene empty; Layout(layout, empty);
    Expect(!Hit(tester, empty, {1, 1}).hit, "56. Empty scene safely misses");
    Expect(many.PainterTraversal().size() == many.NodeCount(),
        "57. Painter traversal is retained by Scene rather than allocated per candidate");
}

} // namespace

int main() {
    TestGeometry();
    TestTransformsAndShapes();
    TestClip();
    TestStateSortingAndRoute();
    TestDirtyLifecycleRuntimeAndPerformance();
    std::cout << (failures == 0 ? "UI2D hit-test tests passed\n" : "UI2D hit-test tests failed\n");
    return failures == 0 ? 0 : 1;
}
