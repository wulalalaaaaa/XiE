#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIFocusManager.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Input/UIEventContext.h"
#include "UI2D/Input/UIEventListenerRegistry.h"
#include "UI2D/Input/UIFocusRequestQueue.h"
#include "UI2D/Input/UIFocusUtils.h"
#include "UI2D/Input/UIInteractionState.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Theme/ResolvedUITheme.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {
using namespace Engine;
using namespace Engine::UI2D;

int failures = 0;
void Expect(bool condition, const char* message) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << message << '\n';
    if (!condition) ++failures;
}

UILayoutParams Fixed(float width, float height) {
    UILayoutParams layout;
    layout.sizeRule.width = {UISizeMode::Fixed, width};
    layout.sizeRule.height = {UISizeMode::Fixed, height};
    return layout;
}

void Place(UIScene& scene, UINodeHandle node, float x, float y, float w, float h) {
    (void)scene.SetLayout(node, Fixed(w, h));
    UITransform transform = scene.TryGet(node)->transform;
    transform.position = {x, y};
    (void)scene.SetTransform(node, transform);
}

struct Fixture {
    WindowHandle window{31, 1};
    UIScene scene;
    BasicLayoutEngine layout;
    BasicUIHitTester hit;
    BasicUIInputRouter router;
    BasicUIFocusManager focus;
    UIFocusRequestQueue requests;
    UIEventListenerRegistry listeners;
    UISceneMutationQueue mutations;
    UIInteractionRuntimeState interaction;
    UIFocusPolicy policy{};
    UINodeHandle panel{};
    UINodeHandle a{};
    UINodeHandle b{};
    UINodeHandle c{};
    UINodeHandle programmatic{};
    UINodeHandle disabled{};

    explicit Fixture(bool initiallyFocused = true) {
        panel = scene.CreateNode("Panel"); Place(scene, panel, 10, 10, 270, 120);
        a = scene.CreateNode("A", panel); Place(scene, a, 10, 10, 40, 40);
        b = scene.CreateNode("B", panel); Place(scene, b, 60, 10, 40, 40);
        c = scene.CreateNode("C", panel); Place(scene, c, 110, 10, 40, 40);
        programmatic = scene.CreateNode("Programmatic", panel); Place(scene, programmatic, 160, 10, 40, 40);
        disabled = scene.CreateNode("Disabled", panel); Place(scene, disabled, 210, 10, 40, 40);
        (void)scene.SetFocusProperties(a, {true, 0});
        (void)scene.SetFocusProperties(b, {true, 0});
        (void)scene.SetFocusProperties(c, {true, 10});
        (void)scene.SetFocusProperties(programmatic, {true, -1});
        (void)scene.SetFocusProperties(disabled, {true, 20});
        (void)scene.SetEnabled(disabled, false);
        Relayout();
        scene.SetNodeInvalidatedCallback([this](UINodeHandle node) {
            UIFocusInvalidation invalidation;
            invalidation.node = node;
            invalidation.reason = UIFocusInvalidationReason::Destroyed;
            if (const UINodeRecord* record = scene.TryGet(node)) {
                invalidation.lastKnownParent = record->parent;
                invalidation.lastKnownTraversalOrder = record->insertionOrder;
            }
            auto context = FocusContext();
            focus.OnNodeInvalidated(scene, invalidation, context);
            listeners.RemoveAllForNode(node);
            interaction.OnNodeInvalidated(node);
        });
        if (initiallyFocused) {
            auto context = FocusContext();
            focus.OnWindowFocusChanged(scene, true, context);
        }
    }

    void Relayout() {
        static ResolvedUITheme theme;
        const UILayoutResult result = layout.UpdateLayout(scene, {{320, 180}, 1.0f, theme, scene.Root()});
        if (result.Succeeded()) scene.ClearDirty(UIDirtyFlags::Layout);
    }
    UIFocusDispatchContext FocusContext() {
        return {window, listeners, mutations, interaction.capture, requests, policy};
    }
    UIFocusRequestResult Flush() {
        auto context = FocusContext();
        return requests.Flush(scene, focus, context);
    }
    UIFocusRequestResult Focus(UINodeHandle node, UIFocusReason reason = UIFocusReason::Programmatic) {
        requests.Enqueue({node, reason, UIFocusRequestPriority::Explicit, false});
        return Flush();
    }
    UIFocusRequestResult Clear() {
        requests.Enqueue({{}, UIFocusReason::Clear, UIFocusRequestPriority::Explicit, true});
        return Flush();
    }
    UIInputDispatchResult Dispatch(std::span<const InputEvent> events) {
        UIInputDispatchContext context{
            window, hit, focus, listeners, mutations, interaction, {}, requests, policy};
        return router.Dispatch(scene, events, context);
    }
    UIInputDispatchResult Dispatch(const InputEvent& event) {
        return Dispatch(std::span<const InputEvent>(&event, 1));
    }
};

PointerButtonEvent Down(WindowHandle window, Vec2F point) {
    PointerButtonEvent event;
    event.window = window;
    event.logicalPosition = point;
    event.button = PointerButton::Primary;
    event.pressed = true;
    return event;
}

KeyEvent Tab(WindowHandle window, bool shift = false, bool repeat = false) {
    KeyEvent event;
    event.window = window;
    event.key = InputKeyCode::Tab;
    event.pressed = true;
    event.repeat = repeat;
    event.modifiers = shift ? InputModifiers::Shift : InputModifiers::None;
    return event;
}

void TestValidityAndQueue() {
    Fixture f;
    const std::uint64_t visual = f.scene.VisualRevision();
    Expect(f.Focus(f.a).status == UIFocusRequestStatus::Applied && f.focus.IsFocused(f.a),
        "1/30. Valid focus request applies and IsFocused reports active node");
    Expect(f.scene.VisualRevision() > visual,
        "35/93. Focus change advances VisualRevision for one rebuild cycle");
    Expect(f.Focus(f.a).status == UIFocusRequestStatus::AlreadyFocused,
        "8. Repeating the current focus returns AlreadyFocused");
    Expect(f.Clear().status == UIFocusRequestStatus::Cleared && !f.focus.GetFocusedNode().IsValid(),
        "9. ClearFocus removes logical focus");

    const UINodeHandle plain = f.scene.CreateNode("Plain", f.panel);
    Expect(f.Focus(plain).status == UIFocusRequestStatus::RejectedNotFocusable,
        "2. focusable=false is rejected");
    (void)f.scene.SetFocusable(plain, true);
    (void)f.scene.SetVisibility(plain, false); f.Relayout();
    Expect(f.Focus(plain).status == UIFocusRequestStatus::RejectedNotFocusable,
        "3-4. Local and effective visibility participate in IsFocusable");
    (void)f.scene.SetVisibility(plain, true); (void)f.scene.SetEnabled(plain, false); f.Relayout();
    Expect(f.Focus(plain).status == UIFocusRequestStatus::RejectedNotFocusable,
        "5-6. Local and effective enabled state participate in IsFocusable");
    Expect(f.Focus({9999, 1}).status == UIFocusRequestStatus::RejectedInvalidTarget,
        "7. Invalid generation handle is safely rejected");
    (void)f.scene.SetEnabled(plain, true); (void)f.scene.SetHitTestVisible(plain, false); f.Relayout();
    Expect(f.Focus(plain).status == UIFocusRequestStatus::Applied,
        "17. hitTestVisible does not affect programmatic focus");

    f.Clear();
    f.requests.Enqueue({f.a, UIFocusReason::Pointer, UIFocusRequestPriority::DefaultBehavior, false});
    f.requests.Enqueue({f.b, UIFocusReason::Programmatic, UIFocusRequestPriority::Explicit, false});
    Expect(f.Flush().current == f.b, "86. Explicit request outranks pointer default behavior");
    f.requests.Enqueue({f.a, UIFocusReason::Programmatic, UIFocusRequestPriority::Explicit, false});
    f.requests.Enqueue({f.c, UIFocusReason::Programmatic, UIFocusRequestPriority::Explicit, false});
    Expect(f.Flush().current == f.c, "87. Last valid request wins within the same priority");
    f.requests.Enqueue({f.a, UIFocusReason::Pointer, UIFocusRequestPriority::DefaultBehavior, false});
    f.requests.Enqueue({{}, UIFocusReason::Clear, UIFocusRequestPriority::Explicit, true});
    Expect(!f.Flush().current.IsValid(), "88. Explicit ClearFocus overrides default pointer focus");
}

void TestFocusEventsAndWithin() {
    Fixture f;
    std::vector<std::string> log;
    auto record = [&](UINodeHandle node, UIEventType type, UIEventPhaseMask phases, std::string name) {
        f.listeners.AddListener(node, type, phases,
            [&, name = std::move(name)](UIEventContext& context) {
                log.push_back(name);
                if (context.Event().type == UIEventType::FocusChanging) {
                    Expect(context.Event().focusReason == UIFocusReason::Programmatic,
                        "23. FocusChanging carries its focus reason");
                }
            });
    };
    record(f.scene.Root(), UIEventType::FocusChanging, UIEventPhaseMask::Preview, "changing-preview");
    record(f.a, UIEventType::FocusChanging, UIEventPhaseMask::Target, "changing-target");
    record(f.a, UIEventType::FocusChanging, UIEventPhaseMask::Bubble, "changing-bubble");
    record(f.a, UIEventType::FocusGained, UIEventPhaseMask::Target, "gained");
    f.Focus(f.a);
    Expect(log == std::vector<std::string>({"changing-preview", "changing-target", "changing-bubble", "gained"}),
        "18/24/28. FocusChanging precedes commit and focus events use Preview/Target/Bubble");
    Expect(f.focus.FocusRoute().size() == 3 && f.focus.FocusRoute().front() == f.scene.Root() &&
        f.focus.HasFocusWithin(f.scene.Root()) && f.focus.HasFocusWithin(f.panel) &&
        !f.focus.HasFocusWithin(f.b),
        "31-34. FocusRoute and FocusWithin reflect the Root-to-target ancestor chain");

    log.clear();
    record(f.a, UIEventType::FocusLost, UIEventPhaseMask::Target, "lost");
    record(f.b, UIEventType::FocusGained, UIEventPhaseMask::Target, "gained-b");
    bool related = false;
    f.listeners.AddListener(f.b, UIEventType::FocusChanging, UIEventPhaseMask::Target,
        [&](UIEventContext& context) {
            related = context.Event().relatedTarget == f.a;
        });
    f.Focus(f.b);
    const auto lost = std::find(log.begin(), log.end(), "lost");
    const auto gained = std::find(log.begin(), log.end(), "gained-b");
    Expect(lost != log.end() && gained != log.end() && lost < gained && related,
        "22/24-27. Lost precedes Gained and relatedTarget identifies the opposite endpoint");

    Fixture prevented;
    prevented.Focus(prevented.a);
    prevented.listeners.AddListener(prevented.b, UIEventType::FocusChanging, UIEventPhaseMask::Target,
        [](UIEventContext& context) { context.PreventDefault(); });
    Expect(prevented.Focus(prevented.b).status == UIFocusRequestStatus::RejectedPrevented &&
        prevented.focus.GetFocusedNode() == prevented.a,
        "19-20. PreventDefault on FocusChanging preserves old focus");
    prevented.listeners.AddListener(prevented.a, UIEventType::FocusChanging, UIEventPhaseMask::Target,
        [](UIEventContext& context) { context.PreventDefault(); });
    prevented.requests.Enqueue({{}, UIFocusReason::NodeInvalidated,
        UIFocusRequestPriority::Lifecycle, true});
    Expect(!prevented.Flush().current.IsValid(),
        "21. Lifecycle focus cleanup cannot be prevented");

    Fixture deferred;
    deferred.listeners.AddListener(deferred.a, UIEventType::FocusGained, UIEventPhaseMask::Target,
        [&](UIEventContext& context) { context.RequestFocus(deferred.b); });
    deferred.Focus(deferred.a);
    Expect(deferred.focus.GetFocusedNode() == deferred.a && !deferred.requests.Empty(),
        "29/89. A focus callback queues, rather than recursively applying, another request");
    deferred.Flush();
    Expect(deferred.focus.GetFocusedNode() == deferred.b,
        "89. Deferred callback request applies at the next safe point");
}

void TestPointerAndKeyboard() {
    Fixture f;
    f.Dispatch(InputEvent{Down(f.window, {25, 25})});
    Expect(f.focus.GetFocusedNode() == f.a,
        "11/16/102. PointerDown focuses its HitTest target, including tabIndex=-1 semantics");
    f.Clear();
    (void)f.scene.SetFocusable(f.a, false);
    (void)f.scene.SetFocusable(f.panel, true);
    f.Dispatch(InputEvent{Down(f.window, {25, 25})});
    Expect(f.focus.GetFocusedNode() == f.panel,
        "12. Pointer default focus walks to the nearest focusable ancestor");

    Fixture prevented;
    prevented.listeners.AddListener(prevented.a, UIEventType::PointerDown, UIEventPhaseMask::Target,
        [](UIEventContext& context) { context.PreventDefault(); });
    prevented.Dispatch(InputEvent{Down(prevented.window, {25, 25})});
    Expect(!prevented.focus.GetFocusedNode().IsValid(),
        "13. PointerDown PreventDefault blocks default focus");

    Fixture explicitRequest;
    explicitRequest.listeners.AddListener(explicitRequest.a, UIEventType::PointerDown, UIEventPhaseMask::Target,
        [&](UIEventContext& context) { context.RequestFocus(explicitRequest.b); });
    explicitRequest.Dispatch(InputEvent{Down(explicitRequest.window, {25, 25})});
    Expect(explicitRequest.focus.GetFocusedNode() == explicitRequest.b,
        "14. Listener explicit focus request overrides pointer default focus");
    explicitRequest.interaction.ClearAll();
    explicitRequest.Dispatch(InputEvent{Down(explicitRequest.window, {310, 170})});
    Expect(!explicitRequest.focus.GetFocusedNode().IsValid(),
        "15. Background PointerDown clears focus under the default policy");

    Fixture tab;
    tab.Dispatch(InputEvent{Tab(tab.window)});
    Expect(tab.focus.GetFocusedNode() == tab.a, "42/44. Forward Tab with no focus selects first candidate");
    tab.Dispatch(InputEvent{Tab(tab.window)});
    Expect(tab.focus.GetFocusedNode() == tab.b, "36/39/42. Equal tabIndex follows stable insertion order");
    tab.Dispatch(InputEvent{Tab(tab.window)});
    Expect(tab.focus.GetFocusedNode() == tab.c, "38/40-41. Tab sorts by tabIndex and skips negative/disabled nodes");
    tab.Dispatch(InputEvent{Tab(tab.window)});
    Expect(tab.focus.GetFocusedNode() == tab.a, "46. Forward Tab wraps when policy enables wrapping");
    tab.Dispatch(InputEvent{Tab(tab.window, true)});
    Expect(tab.focus.GetFocusedNode() == tab.c, "43/45/48. Shift+Tab navigates backward and wraps");

    Fixture noWrap;
    noWrap.policy.wrapTabNavigation = false;
    noWrap.Focus(noWrap.c);
    noWrap.Dispatch(InputEvent{Tab(noWrap.window)});
    Expect(noWrap.focus.GetFocusedNode() == noWrap.c, "47. wrap=false holds focus at the boundary");
    noWrap.listeners.AddListener(noWrap.c, UIEventType::KeyDown, UIEventPhaseMask::Target,
        [](UIEventContext& context) { context.PreventDefault(); });
    noWrap.Dispatch(InputEvent{Tab(noWrap.window, true)});
    Expect(noWrap.focus.GetFocusedNode() == noWrap.c, "49. KeyDown PreventDefault blocks Tab navigation");

    Fixture explicitTab;
    explicitTab.Focus(explicitTab.a);
    explicitTab.listeners.AddListener(explicitTab.a, UIEventType::KeyDown, UIEventPhaseMask::Target,
        [&](UIEventContext& context) { context.RequestFocus(explicitTab.c); });
    explicitTab.Dispatch(InputEvent{Tab(explicitTab.window)});
    Expect(explicitTab.focus.GetFocusedNode() == explicitTab.c,
        "50. Explicit KeyDown focus request suppresses the Tab default move");

    Fixture sameBatch;
    int keyOnA = 0;
    sameBatch.listeners.AddListener(sameBatch.a, UIEventType::KeyDown, UIEventPhaseMask::Target,
        [&](UIEventContext&) { ++keyOnA; });
    const InputEvent batch[] = {Down(sameBatch.window, {25, 25}), Tab(sameBatch.window, false, true)};
    sameBatch.Dispatch(batch);
    Expect(keyOnA == 1 && sameBatch.focus.GetFocusedNode() == sameBatch.b,
        "51/85/92. Per-event focus commit updates same-batch keyboard target and repeat moves once");
}

void TestCacheRootInvalidationAndWindow() {
    Fixture f;
    (void)f.focus.FocusCandidates(f.scene);
    const std::uint32_t rebuilt = f.focus.DebugStats().candidateRebuilds;
    (void)f.focus.FocusCandidates(f.scene);
    Expect(f.focus.DebugStats().candidateRebuilds == rebuilt,
        "52. Unchanged FocusRevision reuses the candidate cache");
    const UINodeHandle extra = f.scene.CreateNode("Extra", f.panel);
    (void)f.scene.SetFocusProperties(extra, {true, 5});
    (void)f.focus.FocusCandidates(f.scene);
    Expect(f.focus.DebugStats().candidateRebuilds == rebuilt + 1,
        "53/55-57. Candidate-affecting Scene changes advance revision and rebuild once");
    const std::uint64_t revision = f.scene.FocusRevision();
    (void)f.scene.ReparentNode(extra, f.scene.Root());
    Expect(f.scene.FocusRevision() > revision, "54. Reparent advances FocusRevision");
    (void)f.scene.DestroyNode(extra);
    const auto candidates = f.focus.FocusCandidates(f.scene);
    Expect(std::find(candidates.begin(), candidates.end(), extra) == candidates.end(),
        "58. Candidate cache never retains destroyed handles");

    Fixture nextFallback;
    (void)nextFallback.focus.FocusCandidates(nextFallback.scene);
    nextFallback.Focus(nextFallback.b);
    (void)nextFallback.scene.DestroyNode(nextFallback.b);
    nextFallback.Flush();
    Expect(nextFallback.focus.GetFocusedNode() == nextFallback.c,
        "60. Without a focusable ancestor, invalidation selects the next Tab candidate");
    Fixture previousFallback;
    (void)previousFallback.focus.FocusCandidates(previousFallback.scene);
    previousFallback.Focus(previousFallback.c);
    (void)previousFallback.scene.DestroyNode(previousFallback.c);
    previousFallback.Flush();
    Expect(previousFallback.focus.GetFocusedNode() == previousFallback.b,
        "61. Without a next candidate, invalidation selects the previous Tab candidate");
    Fixture clearFallback;
    (void)clearFallback.scene.SetFocusable(clearFallback.b, false);
    (void)clearFallback.scene.SetFocusable(clearFallback.c, false);
    (void)clearFallback.focus.FocusCandidates(clearFallback.scene);
    clearFallback.Focus(clearFallback.a);
    (void)clearFallback.scene.DestroyNode(clearFallback.a);
    clearFallback.Flush();
    Expect(!clearFallback.focus.GetFocusedNode().IsValid(),
        "62. Invalidation clears focus when no fallback candidate exists");

    Fixture root;
    auto context = root.FocusContext();
    root.focus.SetFocusRoot(root.scene, root.panel, context);
    Expect(root.focus.GetFocusRoot() == root.panel, "79. Focus Root defaults to Scene Root and can be changed");
    const UINodeHandle outside = root.scene.CreateNode("Outside");
    (void)root.scene.SetFocusable(outside, true);
    Expect(root.Focus(outside).status == UIFocusRequestStatus::RejectedNotFocusable,
        "80. A node outside Focus Root cannot be focused");
    root.Focus(root.a);
    root.focus.SetFocusRoot(root.scene, root.panel, context);
    Expect(root.focus.GetFocusedNode() == root.a, "81. Focus inside a new Root is preserved");
    const UINodeHandle subroot = root.scene.CreateNode("SubRoot", root.panel);
    const UINodeHandle only = root.scene.CreateNode("Only", subroot);
    (void)root.scene.SetFocusProperties(only, {true, 0});
    root.focus.SetFocusRoot(root.scene, subroot, context);
    Expect(root.focus.GetFocusedNode() == only, "82/84. Root change selects and tabs only within its subtree");
    const UINodeHandle emptyRoot = root.scene.CreateNode("EmptyRoot", root.panel);
    root.focus.SetFocusRoot(root.scene, emptyRoot, context);
    Expect(!root.focus.GetFocusedNode().IsValid(), "83. Focus Root without candidates clears focus");

    Fixture invalid;
    (void)invalid.scene.SetFocusable(invalid.panel, true);
    invalid.Focus(invalid.a);
    (void)invalid.scene.DestroyNode(invalid.a);
    invalid.Flush();
    Expect(invalid.focus.GetFocusedNode() == invalid.panel,
        "59. Destroyed focus falls back to nearest focusable ancestor");
    invalid.Focus(invalid.b);
    (void)invalid.scene.SetVisibility(invalid.b, false); invalid.Relayout();
    auto invalidContext = invalid.FocusContext();
    invalid.focus.OnNodeInvalidated(invalid.scene,
        {invalid.b, UIFocusInvalidationReason::Hidden, invalid.panel, 0}, invalidContext);
    invalid.Flush();
    Expect(invalid.focus.GetFocusedNode() == invalid.panel,
        "63-64. Hidden or disabled focused nodes use lifecycle fallback");
    invalid.Focus(invalid.c);
    const UINodeHandle newParent = invalid.scene.CreateNode("NewParent", invalid.panel);
    (void)invalid.scene.ReparentNode(invalid.c, newParent);
    invalid.focus.OnNodeInvalidated(invalid.scene,
        {invalid.c, UIFocusInvalidationReason::Reparented, invalid.panel, 0}, invalidContext);
    Expect(invalid.focus.GetFocusedNode() == invalid.c && invalid.focus.FocusRoute().back() == invalid.c,
        "65-68. Nonfocused changes are inert and in-root Reparent preserves focus with a new route");

    Fixture window;
    window.Focus(window.a);
    int keyCount = 0;
    int textCount = 0;
    window.listeners.AddListener(window.a, UIEventType::KeyDown, UIEventPhaseMask::Target,
        [&](UIEventContext&) { ++keyCount; });
    window.listeners.AddListener(window.a, UIEventType::TextInput, UIEventPhaseMask::Target,
        [&](UIEventContext&) { ++textCount; });
    WindowFocusEvent lost{window.window, false};
    window.Dispatch(InputEvent{lost});
    KeyEvent key{window.window, InputKeyCode::Enter, true};
    TextInputEvent text{window.window, U'x'};
    const InputEvent inactive[] = {key, text}; window.Dispatch(inactive);
    Expect(!window.focus.HasActiveFocus() && keyCount == 0 && textCount == 0,
        "70-73/78. Deactivation saves candidate, clears active focus and suppresses Key/Text");
    WindowFocusEvent gained{window.window, true};
    window.Dispatch(InputEvent{gained});
    Expect(window.focus.GetFocusedNode() == window.a,
        "74. Window activation restores a still-valid candidate");
    window.Dispatch(InputEvent{lost});
    (void)window.scene.SetEnabled(window.a, false); window.Relayout();
    window.Dispatch(InputEvent{gained});
    Expect(!window.focus.GetFocusedNode().IsValid(), "75. Invalid restore candidate is discarded");

    Fixture noRestore;
    noRestore.policy.restoreFocusOnWindowActivated = false;
    noRestore.Focus(noRestore.a);
    noRestore.Dispatch(InputEvent{WindowFocusEvent{noRestore.window, false}});
    noRestore.Dispatch(InputEvent{WindowFocusEvent{noRestore.window, true}});
    Expect(!noRestore.focus.GetFocusedNode().IsValid(), "76. Restore policy can disable activation restore");

    Fixture other;
    other.Focus(other.b);
    Expect(window.focus.GetFocusedNode() != other.focus.GetFocusedNode(),
        "10/77. Focus and restore state are isolated per window");
}

void TestInitialWindowFocusIsPlatformDriven() {
    Fixture inactive(false);
    Expect(!inactive.focus.IsWindowFocused(),
        "new focus manager starts inactive until a platform focus event");
    const auto request = inactive.Focus(inactive.a);
    Expect(request.status == UIFocusRequestStatus::Deferred &&
        !inactive.focus.HasActiveFocus(),
        "focus request in a visible but inactive window is deferred");

    int keys = 0;
    int text = 0;
    inactive.listeners.AddListener(inactive.a, UIEventType::KeyDown,
        UIEventPhaseMask::Target, [&](UIEventContext&) { ++keys; });
    inactive.listeners.AddListener(inactive.a, UIEventType::TextInput,
        UIEventPhaseMask::Target, [&](UIEventContext&) { ++text; });
    inactive.Dispatch(InputEvent{KeyEvent{
        inactive.window, InputKeyCode::A, true}});
    inactive.Dispatch(InputEvent{TextInputEvent{inactive.window, U'a'}});
    Expect(keys == 0 && text == 0,
        "inactive window rejects Key and TextInput for the old TextBox");

    inactive.Dispatch(InputEvent{WindowFocusEvent{
        inactive.window, true, 0.0}});
    inactive.Flush();
    Expect(inactive.focus.IsWindowFocused() &&
        inactive.focus.GetFocusedNode() == inactive.a,
        "platform focus gain restores the deferred target");
}

} // namespace

int main() {
    TestValidityAndQueue();
    TestFocusEventsAndWithin();
    TestPointerAndKeyboard();
    TestCacheRootInvalidationAndWindow();
    TestInitialWindowFocusIsPlatformDriven();
    std::cout << (failures == 0 ? "UI2D focus-manager tests passed\n" : "UI2D focus-manager tests failed\n");
    return failures == 0 ? 0 : 1;
}
