#include "Application/ApplicationContext.h"
#include "Application/ApplicationRuntime.h"
#include "Application/ApplicationWindowProvider.h"
#include "Input/InputEventQueue.h"
#include "Platform/IApplicationHost.h"
#include "Platform/IWindowSystem.h"
#include "Renderer2D/IWindowRenderPipeline.h"
#include "UI2D/Core/UIScene.h"
#include "UI2D/Core/UITransform.h"
#include "UI2D/Core/UIVisual.h"
#include "UI2D/Input/UIEventContext.h"
#include "UI2D/Layout/UILayoutTypes.h"
#include "UI2D/Runtime/UIWindowRuntime.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeHost final : public Engine::IApplicationHost {
public:
    void PollEvents() override {}
    void WaitForEvents() override { ++waitCount; }
    void WaitForEventsTimeout(double) override { ++waitCount; }
    void WakeEventLoop() override { ++wakeCount; }
    bool ShouldExit() const override { return shouldExit; }
    void RequestExit() override { shouldExit = true; }

    int waitCount = 0;
    int wakeCount = 0;
    bool shouldExit = false;
};

class FakeWindows final : public Engine::IWindowSystem {
public:
    struct Record {
        Engine::WindowHandle handle{};
        bool visible = false;
        bool focused = false;
        bool closeRequested = false;
        Engine::RenderSurfaceSize framebuffer{};
    };

    Engine::WindowHandle CreateWindow(
        const Engine::WindowDesc& description) override {
        Record record;
        record.handle = {nextIndex++, 1};
        record.visible = description.visible;
        record.focused = description.visible && !description.noActivate;
        record.framebuffer = {
            static_cast<int>(std::lround(description.width * 1.25f)),
            static_cast<int>(std::lround(description.height * 1.25f))};
        records.push_back(record);
        return record.handle;
    }

    void DestroyWindow(Engine::WindowHandle window) override {
        records.erase(std::remove_if(records.begin(), records.end(),
            [window](const auto& value) { return value.handle == window; }),
            records.end());
        ++destroyCount;
    }
    void ShowWindow(Engine::WindowHandle window) override {
        if (auto* value = Find(window)) value->visible = true;
    }
    void HideWindow(Engine::WindowHandle window) override {
        if (auto* value = Find(window)) value->visible = false;
    }
    void SetWindowPosition(Engine::WindowHandle, int, int) override {}
    void SetWindowSize(Engine::WindowHandle window, int width,
        int height) override {
        if (auto* value = Find(window)) {
            value->framebuffer = {
                static_cast<int>(std::lround(width * 1.25f)),
                static_cast<int>(std::lround(height * 1.25f))};
        }
    }
    bool ActivateWindow(Engine::WindowHandle window) override {
        if (auto* value = Find(window)) {
            value->visible = true;
            value->focused = true;
            value->closeRequested = false;
            return true;
        }
        return false;
    }
    bool RequestClose(Engine::WindowHandle window) override {
        if (auto* value = Find(window)) {
            value->closeRequested = true;
            return true;
        }
        return false;
    }
    bool ClearCloseRequest(Engine::WindowHandle window) override {
        if (auto* value = Find(window)) {
            value->closeRequested = false;
            return true;
        }
        return false;
    }
    bool IsVisible(Engine::WindowHandle window) const override {
        const auto* value = Find(window);
        return value != nullptr && value->visible;
    }
    bool IsMinimized(Engine::WindowHandle) const override { return false; }
    bool IsMaximized(Engine::WindowHandle) const override { return false; }
    bool IsFocused(Engine::WindowHandle window) const override {
        const auto* value = Find(window);
        return value != nullptr && value->focused;
    }
    bool IsCloseRequested(Engine::WindowHandle window) const override {
        const auto* value = Find(window);
        return value != nullptr && value->closeRequested;
    }
    float GetDpiScale(Engine::WindowHandle) const override { return 1.25f; }
    Engine::RenderSurfaceSize GetFramebufferSize(
        Engine::WindowHandle window) const override {
        const auto* value = Find(window);
        return value != nullptr ? value->framebuffer
                                : Engine::RenderSurfaceSize{};
    }
    Engine::NativeWindowHandle GetNativeHandle(
        Engine::WindowHandle) const override { return 0; }
    Engine::Vec2F GetWindowScreenPositionPhysical(
        Engine::WindowHandle) const override { return {-1920.0f, 48.0f}; }

    std::size_t Size() const noexcept { return records.size(); }
    int DestroyCount() const noexcept { return destroyCount; }

private:
    Record* Find(Engine::WindowHandle window) {
        const auto found = std::find_if(records.begin(), records.end(),
            [window](const auto& value) { return value.handle == window; });
        return found == records.end() ? nullptr : &*found;
    }
    const Record* Find(Engine::WindowHandle window) const {
        const auto found = std::find_if(records.begin(), records.end(),
            [window](const auto& value) { return value.handle == window; });
        return found == records.end() ? nullptr : &*found;
    }

    std::uint32_t nextIndex = 1;
    int destroyCount = 0;
    std::vector<Record> records;
};

struct PipelineCounters {
    int resize = 0;
    int render = 0;
    int present = 0;
};

class FakePipeline final : public Engine::IWindowRenderPipeline {
public:
    explicit FakePipeline(std::shared_ptr<PipelineCounters> value)
        : counters(std::move(value)) {}

    void Resize(int, int, float) override { ++counters->resize; }
    Engine::Render2DStats Render(const Engine::DrawList2D& drawList,
        const Engine::Render2DView&) override {
        ++counters->render;
        Engine::Render2DStats result;
        result.commandCount = static_cast<std::uint32_t>(
            drawList.Commands().size());
        result.drawCallCount = result.commandCount;
        return result;
    }
    void Present() override { ++counters->present; }

private:
    std::shared_ptr<PipelineCounters> counters;
};

class FakeProvider final : public Engine::IApplicationWindowProvider {
public:
    FakeProvider(FakeWindows& value, Engine::ApplicationRuntime& appRuntime,
        Engine::WindowContext& appContext)
        : windows(value), runtime(appRuntime), context(appContext) {}

    bool SupportsStandard() const noexcept override { return false; }
    bool SupportsTransparent() const noexcept override { return true; }
    const char* BackendName() const noexcept override {
        return "interactive_overlay_fake";
    }

    Engine::ApplicationResult<Engine::ApplicationWindowResources>
    CreateStandard(const Engine::StandardWindowOptions&) override {
        return Engine::ApplicationResult<
            Engine::ApplicationWindowResources>::Failure({
                Engine::ApplicationErrorCode::Unsupported,
                "CreateStandardWindow", "standard_window",
                "contract provider only supports transparent windows",
                std::nullopt, BackendName()});
    }

    Engine::ApplicationResult<Engine::ApplicationWindowResources>
    CreateTransparent(const Engine::TransparentWindowOptions& options) override {
        received = options;
        Engine::WindowDesc description;
        description.title = options.title;
        description.width = options.size.width;
        description.height = options.size.height;
        description.visible = options.visible;
        description.transparent = options.transparent;
        description.alwaysOnTop = options.alwaysOnTop;
        description.toolWindow = options.toolWindow;
        description.noActivate =
            options.focusPolicy == Engine::WindowFocusPolicy::NoActivate;
        description.clickThrough = options.clickThrough;
        description.opacity = options.opacity;
        const auto window = windows.CreateWindow(description);
        counters = std::make_shared<PipelineCounters>();
        context.window = window;
        context.renderPipeline = std::make_unique<FakePipeline>(counters);
        context.visible = options.visible;
        context.focused = options.visible &&
            options.focusPolicy != Engine::WindowFocusPolicy::NoActivate;
        context.framePolicy = options.visible ? Engine::FramePolicy::Static
            : Engine::FramePolicy::Hidden;
        context.framebufferSize = windows.GetFramebufferSize(window);
        Engine::ApplicationWindowResources resources;
        resources.window = window;
        resources.windows = &windows;
        resources.runtime = &runtime;
        resources.context = &context;
        resources.backend = BackendName();
        resources.attachInput = [](Engine::InputEventQueue*) { return true; };
        resources.close = [this, window] { windows.DestroyWindow(window); };
        return Engine::ApplicationResult<
            Engine::ApplicationWindowResources>::Success(std::move(resources));
    }

    std::optional<Engine::TransparentWindowOptions> received;
    std::shared_ptr<PipelineCounters> counters;

private:
    FakeWindows& windows;
    Engine::ApplicationRuntime& runtime;
    Engine::WindowContext& context;
};

} // namespace

#define XIE_CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::cerr << "FAIL line " << __LINE__ << ": " #expression << '\n'; \
            return 1; \
        } \
    } while (false)

int main() {
    using namespace Engine;
    using namespace Engine::UI2D;

    FakeHost host;
    FakeWindows windows;
    ApplicationRuntime runtime({host, windows});
    WindowContext providerContext;
    FakeProvider provider(windows, runtime, providerContext);

    ApplicationContextOptions contextOptions;
    contextOptions.applicationName = "XiE Interactive Overlay Contract";
    contextOptions.initializeBackends = false;
    contextOptions.runtime = &runtime;
    auto contextResult = ApplicationContext::Create(std::move(contextOptions));
    XIE_CHECK(contextResult);
    auto context = std::move(contextResult.Value());
    auto registration = context->RegisterWindowProvider(provider);
    XIE_CHECK(registration);

    TransparentWindowOptions options;
    options.title = "XiE Interactive Overlay Contract";
    options.size = {480, 320};
    options.visible = false;
    options.transparent = true;
    options.alwaysOnTop = true;
    options.toolWindow = true;
    options.focusPolicy = WindowFocusPolicy::Activate;
    options.clickThrough = false;
    options.closePolicy = WindowClosePolicy::Hide;
    options.placement = {-1920, 48, false};
    auto sessionResult = context->Windows().CreateTransparent(options);
    XIE_CHECK(sessionResult);
    XIE_CHECK(provider.received.has_value());
    XIE_CHECK(provider.received->focusPolicy == WindowFocusPolicy::Activate);
    XIE_CHECK(!provider.received->clickThrough);

    auto session = std::move(sessionResult.Value());
    XIE_CHECK(session.BindUI());
    XIE_CHECK(session.UI() != nullptr);
    XIE_CHECK(session.InputQueue() != nullptr);
    XIE_CHECK(session.QueryDpi() && session.QueryDpi().Value() == 1.25f);
    XIE_CHECK(session.QueryPlacement() &&
        session.QueryPlacement().Value().x == -1920);
    XIE_CHECK(!context->QueryWindowActivity(session).Value().hasPendingWork);

    const auto target = session.UI()->Scene().CreateNode(
        "XiE.InteractiveOverlay.Target");
    UILayoutParams targetLayout;
    targetLayout.sizeRule.width = {UISizeMode::Fixed, 180.0f};
    targetLayout.sizeRule.height = {UISizeMode::Fixed, 100.0f};
    session.UI()->Scene().SetLayout(target, targetLayout);
    session.UI()->Scene().SetVisual(target,
        UIPanelVisual{{0.2f, 0.4f, 0.8f, 0.8f}, 8.0f, true});
    session.UI()->Scene().SetFocusable(target, true);
    session.UI()->Scene().SetHitTestVisible(target, true);
    bool pointerSeen = false;
    session.UI()->AddEventListener(target, UIEventType::PointerDown,
        UIEventPhaseMask::All,
        [&pointerSeen](UIEventContext&) { pointerSeen = true; });

    bool externalWakeSeen = false;
    XIE_CHECK(context->SetExternalWakeCallback(
        [&externalWakeSeen] { externalWakeSeen = true; }));
    const auto wakeBefore = host.wakeCount;
    XIE_CHECK(context->Wake());
    XIE_CHECK(host.wakeCount == wakeBefore + 1 && externalWakeSeen);
    XIE_CHECK(context->PumpWindowEvents());

    XIE_CHECK(session.Show());
    XIE_CHECK(session.QueryState().visible && session.QueryState().focused);
    XIE_CHECK(session.UI()->EnqueueInput(PointerButtonEvent{
        session.Handle(), {40.0f, 40.0f}, PointerButton::Left, true,
        InputModifiers::None, 0, 0.0}));
    XIE_CHECK(context->ProcessWindowFrame(session, 1.0 / 60.0));
    XIE_CHECK(pointerSeen);
    const auto rendered = context->RenderWindow(session, true);
    XIE_CHECK(rendered && provider.counters->render > 0 &&
        provider.counters->present > 0);

    XIE_CHECK(session.Hide());
    XIE_CHECK(!session.QueryState().visible);
    XIE_CHECK(!context->QueryWindowActivity(session).Value().hasPendingWork);
    XIE_CHECK(session.RequestClose());
    XIE_CHECK(context->ProcessWindowEvents());
    XIE_CHECK(!session.IsClosed() && !session.QueryState().visible);
    XIE_CHECK(session.Show());
    XIE_CHECK(session.Handle().IsValid());

    XIE_CHECK(session.Close());
    XIE_CHECK(session.Close());
    XIE_CHECK(session.IsClosed());
    XIE_CHECK(windows.Size() == 0 && windows.DestroyCount() == 1);
    XIE_CHECK(registration.Value().Unregister());
    XIE_CHECK(context->Shutdown());
    XIE_CHECK(context->Shutdown());
    return 0;
}

#undef XIE_CHECK
