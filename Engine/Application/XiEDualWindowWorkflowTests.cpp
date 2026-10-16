#include "Application/ApplicationRuntime.h"
#include "Application/ApplicationWindowProvider.h"
#include "Application/XiEApplication.h"
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
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeHost final : public Engine::IApplicationHost {
public:
    void PollEvents() override {
        ++m_PollCount;
        if (m_exitAfterPolls != 0 && m_PollCount >= m_exitAfterPolls) {
            m_ShouldExit = true;
        }
    }
    void WaitForEvents() override { ++m_WaitCount; }
    void WaitForEventsTimeout(double seconds) override {
        ++m_WaitCount;
        m_LastWaitSeconds = seconds;
    }
    void WakeEventLoop() override { ++m_WakeCount; }
    bool ShouldExit() const override { return m_ShouldExit; }
    void RequestExit() override { m_ShouldExit = true; }

    void Reset(std::uint32_t exitAfterPolls) {
        m_ShouldExit = false;
        m_PollCount = 0;
        m_exitAfterPolls = exitAfterPolls;
    }
    int WakeCount() const noexcept { return m_WakeCount; }
    int WaitCount() const noexcept { return m_WaitCount; }

private:
    bool m_ShouldExit = false;
    std::uint32_t m_exitAfterPolls = 0;
    std::uint32_t m_PollCount = 0;
    int m_WaitCount = 0;
    int m_WakeCount = 0;
    double m_LastWaitSeconds = 0.0;
};

class FakeWindows final : public Engine::IWindowSystem {
public:
    struct Record {
        Engine::WindowHandle handle{};
        Engine::WindowDesc description;
        bool visible = false;
        bool focused = false;
        bool closeRequested = false;
        Engine::RenderSurfaceSize framebuffer{};
    };

    Engine::WindowHandle CreateWindow(
        const Engine::WindowDesc& description) override {
        Record record;
        record.handle = {m_NextIndex++, 1};
        record.description = description;
        record.visible = description.visible;
        record.focused = description.visible && !description.noActivate;
        record.framebuffer = {
            static_cast<int>(std::lround(description.width * 1.25f)),
            static_cast<int>(std::lround(description.height * 1.25f))};
        m_Records.push_back(record);
        return record.handle;
    }

    void DestroyWindow(Engine::WindowHandle window) override {
        m_Records.erase(std::remove_if(m_Records.begin(), m_Records.end(),
            [window](const auto& record) { return record.handle == window; }),
            m_Records.end());
        ++m_DestroyCount;
    }
    void ShowWindow(Engine::WindowHandle window) override {
        if (auto* record = Find(window)) record->visible = true;
    }
    void HideWindow(Engine::WindowHandle window) override {
        if (auto* record = Find(window)) record->visible = false;
    }
    void SetWindowPosition(Engine::WindowHandle, int, int) override {}
    void SetWindowSize(Engine::WindowHandle window, int width,
        int height) override {
        if (auto* record = Find(window)) {
            record->framebuffer = {
                static_cast<int>(std::lround(width * 1.25f)),
                static_cast<int>(std::lround(height * 1.25f))};
        }
    }
    bool ActivateWindow(Engine::WindowHandle window) override {
        if (auto* record = Find(window)) {
            record->focused = true;
            return true;
        }
        return false;
    }
    bool RequestClose(Engine::WindowHandle window) override {
        if (auto* record = Find(window)) {
            record->closeRequested = true;
            return true;
        }
        return false;
    }
    bool ClearCloseRequest(Engine::WindowHandle window) override {
        if (auto* record = Find(window)) {
            record->closeRequested = false;
            return true;
        }
        return false;
    }
    bool IsVisible(Engine::WindowHandle window) const override {
        const auto* record = Find(window);
        return record != nullptr && record->visible;
    }
    bool IsMinimized(Engine::WindowHandle) const override { return false; }
    bool IsFocused(Engine::WindowHandle window) const override {
        const auto* record = Find(window);
        return record != nullptr && record->focused;
    }
    bool IsCloseRequested(Engine::WindowHandle window) const override {
        const auto* record = Find(window);
        return record != nullptr && record->closeRequested;
    }
    float GetDpiScale(Engine::WindowHandle) const override { return 1.25f; }
    Engine::RenderSurfaceSize GetFramebufferSize(
        Engine::WindowHandle window) const override {
        const auto* record = Find(window);
        return record != nullptr ? record->framebuffer : Engine::RenderSurfaceSize{};
    }
    Engine::NativeWindowHandle GetNativeHandle(
        Engine::WindowHandle) const override { return 0; }

    std::size_t Size() const noexcept { return m_Records.size(); }
    int DestroyCount() const noexcept { return m_DestroyCount; }

private:
    Record* Find(Engine::WindowHandle window) {
        const auto found = std::find_if(m_Records.begin(), m_Records.end(),
            [window](const auto& record) { return record.handle == window; });
        return found == m_Records.end() ? nullptr : &*found;
    }
    const Record* Find(Engine::WindowHandle window) const {
        const auto found = std::find_if(m_Records.begin(), m_Records.end(),
            [window](const auto& record) { return record.handle == window; });
        return found == m_Records.end() ? nullptr : &*found;
    }

    std::uint32_t m_NextIndex = 1;
    int m_DestroyCount = 0;
    std::vector<Record> m_Records;
};

struct PipelineStats {
    int resizeCount = 0;
    int renderCount = 0;
    int presentCount = 0;
};

class FakePipeline final : public Engine::IWindowRenderPipeline {
public:
    explicit FakePipeline(std::shared_ptr<PipelineStats> stats)
        : m_Stats(std::move(stats)) {}

    void Resize(int, int, float) override { ++m_Stats->resizeCount; }
    Engine::Render2DStats Render(const Engine::DrawList2D& drawList,
        const Engine::Render2DView&) override {
        ++m_Stats->renderCount;
        Engine::Render2DStats result;
        result.commandCount = static_cast<std::uint32_t>(
            drawList.Commands().size());
        result.drawCallCount = result.commandCount;
        return result;
    }
    void Present() override { ++m_Stats->presentCount; }

private:
    std::shared_ptr<PipelineStats> m_Stats;
};

class FakeProvider final : public Engine::IApplicationWindowProvider {
public:
    FakeProvider(FakeWindows& windows, Engine::ApplicationRuntime& runtime)
        : m_Windows(windows), m_Runtime(runtime) {}

    bool SupportsStandard() const noexcept override { return true; }
    bool SupportsTransparent() const noexcept override { return true; }
    const char* BackendName() const noexcept override { return "workflow_fake"; }

    Engine::ApplicationResult<Engine::ApplicationWindowResources>
    CreateStandard(const Engine::StandardWindowOptions& options) override {
        Engine::WindowDesc description;
        description.title = options.title;
        description.width = options.size.width;
        description.height = options.size.height;
        description.visible = options.visible;
        description.resizable = options.resizable;
        description.decorated = options.decorated;
        description.alwaysOnTop = options.alwaysOnTop;
        description.toolWindow = options.toolWindow;
        description.noActivate =
            options.focusPolicy == Engine::WindowFocusPolicy::NoActivate;

        const auto window = m_Windows.CreateWindow(description);
        auto stats = std::make_shared<PipelineStats>();
        Engine::WindowContext context;
        context.window = window;
        context.renderPipeline = std::make_unique<FakePipeline>(stats);
        context.visible = options.visible;
        context.focused = options.focusPolicy != Engine::WindowFocusPolicy::NoActivate;
        context.framePolicy = options.visible ? Engine::FramePolicy::Static
                                               : Engine::FramePolicy::Hidden;
        context.framebufferSize = m_Windows.GetFramebufferSize(window);
        if (!m_Runtime.Windows().Register(std::move(context)).IsValid()) {
            m_Windows.DestroyWindow(window);
            return Engine::ApplicationResult<
                Engine::ApplicationWindowResources>::Failure({
                    Engine::ApplicationErrorCode::BackendFailure,
                    "CreateStandardWindow", "standard_window",
                    "fake provider could not register standard window",
                    std::nullopt, "workflow_fake"});
        }
        m_StandardStats = std::move(stats);
        return MakeResources(window, m_Runtime.Windows().TryGet(window),
            [this, window] {
                (void)m_Runtime.Windows().Unregister(window);
                m_Windows.DestroyWindow(window);
            });
    }

    Engine::ApplicationResult<Engine::ApplicationWindowResources>
    CreateTransparent(const Engine::TransparentWindowOptions& options) override {
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

        const auto window = m_Windows.CreateWindow(description);
        auto stats = std::make_shared<PipelineStats>();
        auto context = std::make_unique<Engine::WindowContext>();
        context->window = window;
        context->renderPipeline = std::make_unique<FakePipeline>(stats);
        context->visible = options.visible;
        context->focused = options.focusPolicy != Engine::WindowFocusPolicy::NoActivate;
        context->framePolicy = options.visible ? Engine::FramePolicy::Static
                                               : Engine::FramePolicy::Hidden;
        context->framebufferSize = m_Windows.GetFramebufferSize(window);
        auto* contextPointer = context.get();
        m_TransparentContext = std::move(context);
        m_TransparentStats = std::move(stats);
        return MakeResources(window, contextPointer, [this, window] {
            m_TransparentContext.reset();
            m_Windows.DestroyWindow(window);
        });
    }

    int StandardRenderCount() const noexcept {
        return m_StandardStats ? m_StandardStats->renderCount : 0;
    }
    int TransparentRenderCount() const noexcept {
        return m_TransparentStats ? m_TransparentStats->renderCount : 0;
    }
    int StandardPresentCount() const noexcept {
        return m_StandardStats ? m_StandardStats->presentCount : 0;
    }
    int TransparentPresentCount() const noexcept {
        return m_TransparentStats ? m_TransparentStats->presentCount : 0;
    }
    Engine::InputEventQueue* QueueFor(Engine::WindowHandle window) const noexcept {
        const auto found = std::find_if(m_Queues.begin(), m_Queues.end(),
            [window](const auto& value) { return value.first == window; });
        return found == m_Queues.end() ? nullptr : found->second;
    }

private:
    Engine::ApplicationResult<Engine::ApplicationWindowResources> MakeResources(
        Engine::WindowHandle window, Engine::WindowContext* context,
        std::function<void()> close) {
        Engine::ApplicationWindowResources resources;
        resources.window = window;
        resources.windows = &m_Windows;
        resources.runtime = &m_Runtime;
        resources.context = context;
        resources.backend = "workflow_fake";
        resources.attachInput = [this, window](Engine::InputEventQueue* queue) {
            const auto found = std::find_if(m_Queues.begin(), m_Queues.end(),
                [window](const auto& value) { return value.first == window; });
            if (found == m_Queues.end()) m_Queues.emplace_back(window, queue);
            else found->second = queue;
            return true;
        };
        resources.bindRenderResources = [](Engine::TextureRegistry2D*,
            Engine::ITextLayoutService*) {};
        resources.close = std::move(close);
        return Engine::ApplicationResult<
            Engine::ApplicationWindowResources>::Success(std::move(resources));
    }

    FakeWindows& m_Windows;
    Engine::ApplicationRuntime& m_Runtime;
    std::shared_ptr<PipelineStats> m_StandardStats;
    std::shared_ptr<PipelineStats> m_TransparentStats;
    std::unique_ptr<Engine::WindowContext> m_TransparentContext;
    std::vector<std::pair<Engine::WindowHandle, Engine::InputEventQueue*>> m_Queues;
};

class FakeDesktopBackend final : public Engine::IDesktopBackend {
public:
    Engine::DesktopBackendCapabilities Probe() const noexcept override {
        return {true, true, true, true, "workflow_desktop_fake"};
    }
    Engine::ApplicationResult<void> Start(
        const Engine::DesktopBackendStartOptions& value) override {
        m_Callbacks = value;
        m_Started = true;
        return Engine::ApplicationResult<void>::Success();
    }
    Engine::ApplicationResult<Engine::DesktopRegistrationId> CreateTray(
        const Engine::TrayOptions&) override {
        m_Tray = ++m_NextId;
        return Engine::ApplicationResult<
            Engine::DesktopRegistrationId>::Success(m_Tray);
    }
    Engine::ApplicationResult<void> UpdateTray(
        Engine::DesktopRegistrationId, const std::vector<Engine::TrayMenuItem>&,
        const std::string&) override {
        return Engine::ApplicationResult<void>::Success();
    }
    Engine::ApplicationResult<void> RemoveTray(
        Engine::DesktopRegistrationId) override {
        m_Tray = 0;
        return Engine::ApplicationResult<void>::Success();
    }
    Engine::ApplicationResult<Engine::DesktopRegistrationId>
    RegisterGlobalShortcut(const Engine::ShortcutOptions&) override {
        return Engine::ApplicationResult<
            Engine::DesktopRegistrationId>::Success(++m_NextId);
    }
    Engine::ApplicationResult<void> UnregisterGlobalShortcut(
        Engine::DesktopRegistrationId) override {
        return Engine::ApplicationResult<void>::Success();
    }
    Engine::ApplicationResult<Engine::SingleInstanceAcquireResult>
    AcquireSingleInstance(const Engine::SingleInstanceOptions&) override {
        Engine::SingleInstanceAcquireResult result;
        result.status = Engine::SingleInstanceAcquireStatus::PrimaryAcquired;
        result.registration = ++m_NextId;
        return Engine::ApplicationResult<
            Engine::SingleInstanceAcquireResult>::Success(std::move(result));
    }
    Engine::ApplicationResult<void> ReleaseSingleInstance(
        Engine::DesktopRegistrationId) override {
        return Engine::ApplicationResult<void>::Success();
    }
    void Pump() override {}
    void Stop() noexcept override {
        m_Started = false;
        m_Callbacks = {};
    }
    void EmitTrayCommand() {
        if (m_Callbacks.enqueueEvent) {
            m_Callbacks.enqueueEvent({Engine::DesktopBackendEventType::TrayCommand,
                m_Tray, 41});
        }
    }

private:
    Engine::DesktopBackendStartOptions m_Callbacks;
    Engine::DesktopRegistrationId m_Tray = 0;
    Engine::DesktopRegistrationId m_NextId = 0;
    bool m_Started = false;
};

} // namespace

#define XIE_CHECK(expression) \
    do { if (!(expression)) return 1; } while (false)

int main() {
    using namespace Engine;
    using namespace Engine::UI2D;

    FakeHost host;
    FakeWindows windows;
    ApplicationRuntime runtime({host, windows});
    auto desktopBackend = std::make_unique<FakeDesktopBackend>();
    auto* desktopBackendPointer = desktopBackend.get();
    FakeProvider provider(windows, runtime);

    ApplicationContextOptions contextOptions;
    contextOptions.applicationName = "XiE Dual Window Workflow";
    contextOptions.initializeBackends = false;
    contextOptions.runtime = &runtime;
    contextOptions.desktopBackend = std::move(desktopBackend);
    auto contextResult = ApplicationContext::Create(std::move(contextOptions));
    XIE_CHECK(contextResult);
    auto context = std::move(contextResult.Value());
    auto providerRegistration = context->RegisterWindowProvider(provider);
    XIE_CHECK(providerRegistration);

    XIE_CHECK(context->Desktop().Start());
    int desktopCommands = 0;
    auto tray = context->Desktop().Tray().Create({
        "workflow", {}, {{41, "Toggle", TrayMenuItemKind::Command}},
        [&desktopCommands](DesktopCommandId id) {
            if (id == 41) ++desktopCommands;
        }, {}});
    XIE_CHECK(tray);
    desktopBackendPointer->EmitTrayCommand();

    StandardWindowOptions standardOptions;
    standardOptions.title = "XiE Standard Workflow";
    standardOptions.size = {800, 500};
    standardOptions.visible = true;
    standardOptions.focusPolicy = WindowFocusPolicy::Activate;
    standardOptions.closePolicy = WindowClosePolicy::RequestApplicationExit;
    auto standardResult = context->Windows().CreateStandard(standardOptions);
    XIE_CHECK(standardResult);
    auto standard = std::move(standardResult.Value());

    TransparentWindowOptions transparentOptions;
    transparentOptions.title = "XiE Transparent Workflow";
    transparentOptions.size = {480, 320};
    transparentOptions.visible = true;
    transparentOptions.focusPolicy = WindowFocusPolicy::NoActivate;
    transparentOptions.clickThrough = false;
    transparentOptions.closePolicy = WindowClosePolicy::Hide;
    auto transparentResult = context->Windows().CreateTransparent(
        transparentOptions);
    XIE_CHECK(transparentResult);
    auto transparent = std::move(transparentResult.Value());

    auto standardUI = standard.BindUI();
    auto transparentUI = transparent.BindUI();
    XIE_CHECK(standardUI && transparentUI);
    XIE_CHECK(standardUI.Value() != transparentUI.Value());
    XIE_CHECK(standard.InputQueue() != transparent.InputQueue());
    XIE_CHECK(provider.QueueFor(standard.Handle()) == standard.InputQueue());
    XIE_CHECK(provider.QueueFor(transparent.Handle()) == transparent.InputQueue());

    const auto standardState = standard.QueryState();
    const auto transparentState = transparent.QueryState();
    XIE_CHECK(standardState.visible && standardState.focused);
    XIE_CHECK(transparentState.visible && !transparentState.focused);
    XIE_CHECK(standard.QueryDpi() && standard.QueryDpi().Value() == 1.25f);
    XIE_CHECK(transparent.QueryDpi() && transparent.QueryDpi().Value() == 1.25f);
    XIE_CHECK(standard.QueryFocus() && standard.QueryFocus().Value());
    XIE_CHECK(transparent.QueryFocus() && !transparent.QueryFocus().Value());

    const auto target = transparent.UI()->Scene().CreateNode(
        "XiE.Workflow.TransparentTarget");
    UILayoutParams targetLayout;
    targetLayout.sizeRule.width = {UISizeMode::Fixed, 200.0f};
    targetLayout.sizeRule.height = {UISizeMode::Fixed, 120.0f};
    transparent.UI()->Scene().SetLayout(target, targetLayout);
    transparent.UI()->Scene().SetVisual(target,
        UIPanelVisual{{0.1f, 0.4f, 0.8f, 1.0f}, 8.0f, true});
    transparent.UI()->Scene().SetFocusable(target, true);
    transparent.UI()->Scene().SetHitTestVisible(target, true);
    bool pointerSeen = false;
    transparent.UI()->AddEventListener(target, UIEventType::PointerDown,
        UIEventPhaseMask::All,
        [&pointerSeen](UIEventContext&) { pointerSeen = true; });

    XIE_CHECK(transparent.Hide());
    XIE_CHECK(!transparent.QueryState().visible);
    XIE_CHECK(transparent.Show());
    XIE_CHECK(transparent.QueryState().visible);
    XIE_CHECK(transparent.UI()->EnqueueInput(PointerButtonEvent{
        transparent.Handle(), {40.0f, 40.0f}, PointerButton::Left, true,
        InputModifiers::None, 0, 0.0}));
    XIE_CHECK(context->ProcessWindowFrame(transparent, 1.0 / 60.0));
    XIE_CHECK(pointerSeen);

    host.Reset(2);
    auto run = ApplicationRunOptions{};
    int standardUpdates = 0;
    int transparentUpdates = 0;
    run.update = [&standardUpdates, &transparentUpdates, &transparent](
        WindowSession& session, double) {
        if (session.Kind() == WindowKind::Standard) ++standardUpdates;
        else ++transparentUpdates;
        if (session.Kind() == WindowKind::Standard &&
            standardUpdates == 1) {
            (void)transparent.Hide();
            (void)transparent.Show();
        }
    };
    auto ran = context->Run(run);
    XIE_CHECK(ran);
    XIE_CHECK(desktopCommands == 1);
    XIE_CHECK(standardUpdates >= 1 && transparentUpdates >= 1);
    XIE_CHECK(provider.StandardRenderCount() >= 1);
    XIE_CHECK(provider.TransparentRenderCount() >= 1);
    XIE_CHECK(provider.StandardPresentCount() >= 1);
    XIE_CHECK(provider.TransparentPresentCount() >= 1);

    XIE_CHECK(context->Wait(0.01));
    const auto wakeBefore = host.WakeCount();
    XIE_CHECK(context->Wake());
    XIE_CHECK(host.WakeCount() == wakeBefore + 1);

    XIE_CHECK(transparent.Close());
    XIE_CHECK(transparent.IsClosed());
    XIE_CHECK(context->Windows().Size() == 1);
    XIE_CHECK(windows.Size() == 1);

    host.Reset(0);
    XIE_CHECK(standard.RequestClose());
    XIE_CHECK(context->Run());
    XIE_CHECK(standard.IsClosed());
    XIE_CHECK(context->Windows().Size() == 0);
    XIE_CHECK(windows.Size() == 0);
    XIE_CHECK(context->Shutdown());
    XIE_CHECK(context->Shutdown());
    XIE_CHECK(providerRegistration.Value().Unregister());
    XIE_CHECK(windows.DestroyCount() == 2);
    return 0;
}

#undef XIE_CHECK
