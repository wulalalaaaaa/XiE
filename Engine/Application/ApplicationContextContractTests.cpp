#include "Application/XiEApplication.h"

#include "Application/ApplicationRuntime.h"
#include "Platform/IApplicationHost.h"
#include "Platform/IWindowSystem.h"

#include <string>
#include <memory>
#include <iostream>
#include <thread>
#include <utility>

namespace {

class FakeHost final : public Engine::IApplicationHost {
public:
    void PollEvents() override {}
    void WaitForEvents() override { ++m_WaitCalls; }
    void WaitForEventsTimeout(double seconds) override {
        ++m_WaitTimeoutCalls;
        m_LastTimeout = seconds;
    }
    void WakeEventLoop() override {}
    bool ShouldExit() const override { return m_Exit; }
    void RequestExit() override { m_Exit = true; }
    int WaitTimeoutCalls() const noexcept { return m_WaitTimeoutCalls; }
    double LastTimeout() const noexcept { return m_LastTimeout; }

private:
    bool m_Exit = false;
    int m_WaitCalls = 0;
    int m_WaitTimeoutCalls = 0;
    double m_LastTimeout = 0.0;
};

class FakeWindows final : public Engine::IWindowSystem {
public:
    Engine::WindowHandle CreateWindow(const Engine::WindowDesc&) override {
        m_Visible = true;
        return m_Window;
    }
    void DestroyWindow(Engine::WindowHandle window) override {
        if (window == m_Window) m_Visible = false;
    }
    void ShowWindow(Engine::WindowHandle) override { m_Visible = true; }
    void HideWindow(Engine::WindowHandle) override { m_Visible = false; }
    void SetWindowPosition(Engine::WindowHandle, int, int) override {}
    void SetWindowSize(Engine::WindowHandle, int width, int height) override {
        m_Size = {width, height};
    }
    bool ActivateWindow(Engine::WindowHandle) override {
        m_Focused = true;
        return true;
    }
    bool RequestClose(Engine::WindowHandle) override {
        m_CloseRequested = true;
        return true;
    }
    bool ClearCloseRequest(Engine::WindowHandle) override {
        m_CloseRequested = false;
        return true;
    }
    bool IsVisible(Engine::WindowHandle) const override { return m_Visible; }
    bool IsMinimized(Engine::WindowHandle) const override { return false; }
    bool IsFocused(Engine::WindowHandle) const override { return m_Focused; }
    bool IsCloseRequested(Engine::WindowHandle) const override {
        return m_CloseRequested;
    }
    float GetDpiScale(Engine::WindowHandle) const override { return 1.25f; }
    Engine::RenderSurfaceSize GetFramebufferSize(
        Engine::WindowHandle) const override { return m_Size; }
    Engine::NativeWindowHandle GetNativeHandle(
        Engine::WindowHandle) const override { return 0; }

private:
    Engine::WindowHandle m_Window{1, 1};
    Engine::RenderSurfaceSize m_Size{800, 600};
    bool m_Visible = false;
    bool m_Focused = false;
    bool m_CloseRequested = false;
};

class FakeProvider final : public Engine::IApplicationWindowProvider {
public:
    FakeProvider(FakeWindows& windows, Engine::ApplicationRuntime& runtime,
        Engine::WindowContext& context)
        : m_Windows(windows), m_Runtime(runtime), m_Context(context) {}

    bool SupportsStandard() const noexcept override { return true; }
    const char* BackendName() const noexcept override { return "fake"; }

    Engine::ApplicationResult<Engine::ApplicationWindowResources>
    CreateStandard(const Engine::StandardWindowOptions& options) override {
        Engine::WindowDesc desc;
        desc.title = options.title;
        desc.width = options.size.width;
        desc.height = options.size.height;
        const auto window = m_Windows.CreateWindow(desc);
        Engine::ApplicationWindowResources resources;
        resources.window = window;
        resources.windows = &m_Windows;
        resources.runtime = &m_Runtime;
        resources.context = &m_Context;
        resources.backend = "fake";
        resources.attachInput = [](Engine::InputEventQueue*) { return true; };
        resources.close = [this, window] { m_Windows.DestroyWindow(window); };
        return Engine::ApplicationResult<
            Engine::ApplicationWindowResources>::Success(std::move(resources));
    }

private:
    FakeWindows& m_Windows;
    Engine::ApplicationRuntime& m_Runtime;
    Engine::WindowContext& m_Context;
};

class FakeDesktopBackend final : public Engine::IDesktopBackend {
public:
    Engine::DesktopBackendCapabilities Probe() const noexcept override {
        return {true, true, true, true, "fake.context_desktop"};
    }

    Engine::ApplicationResult<void> Start(
        const Engine::DesktopBackendStartOptions& value) override {
        callbacks = value;
        started = true;
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<Engine::DesktopRegistrationId> CreateTray(
        const Engine::TrayOptions&) override {
        return Engine::ApplicationResult<
            Engine::DesktopRegistrationId>::Success(nextId++);
    }

    Engine::ApplicationResult<void> UpdateTray(
        Engine::DesktopRegistrationId,
        const std::vector<Engine::TrayMenuItem>&,
        const std::string&) override {
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<void> RemoveTray(
        Engine::DesktopRegistrationId) override {
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<Engine::DesktopRegistrationId>
    RegisterGlobalShortcut(const Engine::ShortcutOptions&) override {
        return Engine::ApplicationResult<
            Engine::DesktopRegistrationId>::Success(nextId++);
    }

    Engine::ApplicationResult<void> UnregisterGlobalShortcut(
        Engine::DesktopRegistrationId) override {
        return Engine::ApplicationResult<void>::Success();
    }

    Engine::ApplicationResult<Engine::SingleInstanceAcquireResult>
    AcquireSingleInstance(const Engine::SingleInstanceOptions&) override {
        Engine::SingleInstanceAcquireResult result;
        result.status = Engine::SingleInstanceAcquireStatus::PrimaryAcquired;
        result.registration = nextId++;
        return Engine::ApplicationResult<
            Engine::SingleInstanceAcquireResult>::Success(std::move(result));
    }

    Engine::ApplicationResult<void> ReleaseSingleInstance(
        Engine::DesktopRegistrationId) override {
        return Engine::ApplicationResult<void>::Success();
    }

    void Pump() override {}

    void Stop() noexcept override {
        started = false;
        callbacks = {};
    }

    void Emit(Engine::DesktopBackendEvent event) {
        if (callbacks.enqueueEvent) callbacks.enqueueEvent(std::move(event));
    }

    Engine::DesktopBackendStartOptions callbacks;
    Engine::DesktopRegistrationId nextId = 1;
    bool started = false;
};

} // namespace

#define XIE_CHECK(expression) \
    do { if (!(expression)) { \
        std::cerr << "FAIL line " << __LINE__ << ": " #expression << '\n'; \
        return 1; \
    } } while (false)

int main() {
    using namespace Engine;

    const StandardWindowOptions defaults;
    XIE_CHECK(defaults.size.width == 1280);
    XIE_CHECK(defaults.size.height == 720);
    XIE_CHECK(defaults.visible);
    XIE_CHECK(WindowComposition::ValidateStandardOptions(defaults));

    TransparentWindowOptions invalidTransparent;
    invalidTransparent.opacity = 2.0f;
    const auto invalid = WindowComposition::ValidateTransparentOptions(
        invalidTransparent);
    XIE_CHECK(!invalid);
    XIE_CHECK(invalid.Error().code == ApplicationErrorCode::InvalidOptions);

    ApplicationContextOptions disabled;
    disabled.initializeBackends = false;
    auto disabledResult = ApplicationContext::Create(std::move(disabled));
    XIE_CHECK(disabledResult);
    XIE_CHECK(disabledResult.Value()->Capabilities().Find(
        ApplicationCapability::StandardWindow)->status ==
        CapabilityStatus::Unavailable);
    XIE_CHECK(disabledResult.Value()->Capabilities().Find(
        ApplicationCapability::MediaPlayback)->status ==
        CapabilityStatus::Unavailable);
    const auto& disabledCapabilities = disabledResult.Value()->Capabilities();
    const auto* disabledImageCodec = disabledCapabilities.Find(
        ApplicationCapability::ImageCodec);
    const auto* disabledImageClipboard = disabledCapabilities.Find(
        ApplicationCapability::ImageClipboard);
    const auto* disabledImageFile = disabledCapabilities.Find(
        ApplicationCapability::ImageFile);
    const auto* disabledImageAsync = disabledCapabilities.Find(
        ApplicationCapability::ImageAsyncTasks);
    XIE_CHECK(disabledImageCodec != nullptr &&
        disabledImageCodec->reason == "module_not_configured:optional_image_codec");
    XIE_CHECK(disabledImageClipboard != nullptr &&
        disabledImageClipboard->reason ==
            "module_not_configured:optional_image_clipboard");
    XIE_CHECK(disabledImageFile != nullptr &&
        disabledImageFile->reason ==
            "module_not_configured:optional_image_file");
    XIE_CHECK(disabledImageAsync != nullptr &&
        disabledImageAsync->reason ==
            "module_not_configured:optional_image_async_tasks");
    const auto unavailable = disabledResult.Value()->Windows().CreateStandard();
    XIE_CHECK(!unavailable);
    XIE_CHECK(unavailable.Error().code == ApplicationErrorCode::Unsupported);
    XIE_CHECK(disabledResult.Value()->Shutdown());
    XIE_CHECK(disabledResult.Value()->Shutdown());

    auto configuredImages = std::make_shared<ImageServices>();
    ApplicationContextOptions imageOptions;
    imageOptions.initializeBackends = false;
    imageOptions.imageServices = configuredImages;
    auto imageContext = ApplicationContext::Create(std::move(imageOptions));
    XIE_CHECK(imageContext && imageContext.Value()->Images() ==
        configuredImages.get());
    const auto& configuredCapabilities = imageContext.Value()->Capabilities();
    const auto* configuredImageCodec = configuredCapabilities.Find(
        ApplicationCapability::ImageCodec);
    XIE_CHECK(configuredImageCodec != nullptr &&
        configuredImageCodec->reason == "backend_unavailable:image_codec");
    const auto* configuredImageFile = configuredCapabilities.Find(
        ApplicationCapability::ImageFile);
    const auto* configuredImageAsync = configuredCapabilities.Find(
        ApplicationCapability::ImageAsyncTasks);
    XIE_CHECK(configuredImageFile != nullptr &&
        configuredImageFile->reason == "backend_unavailable:image_file");
    XIE_CHECK(configuredImageAsync != nullptr &&
        configuredImageAsync->reason == "backend_unavailable:image_async_tasks");
    XIE_CHECK(imageContext.Value()->Shutdown());
    const auto& shutdownCapabilities = imageContext.Value()->Capabilities();
    const auto* shutdownImageCodec = shutdownCapabilities.Find(
        ApplicationCapability::ImageCodec);
    XIE_CHECK(shutdownImageCodec != nullptr &&
        shutdownImageCodec->reason == "service_shutdown:image_codec");
    const auto* shutdownImageFile = shutdownCapabilities.Find(
        ApplicationCapability::ImageFile);
    const auto* shutdownImageAsync = shutdownCapabilities.Find(
        ApplicationCapability::ImageAsyncTasks);
    XIE_CHECK(shutdownImageFile != nullptr &&
        shutdownImageFile->reason == "service_shutdown:image_file");
    XIE_CHECK(shutdownImageAsync != nullptr &&
        shutdownImageAsync->reason == "service_shutdown:image_async_tasks");

    {
        FakeHost desktopHost;
        FakeWindows desktopWindows;
        ApplicationRuntime desktopRuntime({desktopHost, desktopWindows});
        auto desktopBackend = std::make_unique<FakeDesktopBackend>();
        auto* desktopBackendRaw = desktopBackend.get();
        ApplicationContextOptions desktopOptions;
        desktopOptions.initializeBackends = false;
        desktopOptions.runtime = &desktopRuntime;
        desktopOptions.desktopBackend = std::move(desktopBackend);
        auto desktopContext = ApplicationContext::Create(
            std::move(desktopOptions));
        XIE_CHECK(desktopContext);
        XIE_CHECK(desktopContext.Value()->Desktop().Start());

        const auto startedCapabilities = desktopContext.Value()->Capabilities();
        const auto* startedTray = startedCapabilities.Find(
            ApplicationCapability::SystemTray);
        XIE_CHECK(startedTray != nullptr);
        XIE_CHECK(startedTray->status == CapabilityStatus::Supported);
        const auto startedRevision = startedTray->health.revision;

        const auto ownerThread = std::this_thread::get_id();
        bool callbackOnOwnerThread = false;
        auto contextTray = desktopContext.Value()->Desktop().Tray().Create({
            "context", {}, {{41, "Context", TrayMenuItemKind::Command}},
            [&](DesktopCommandId) {
                callbackOnOwnerThread = std::this_thread::get_id() ==
                    ownerThread;
            }, {}});
        XIE_CHECK(contextTray);
        std::thread desktopWorker([&] {
            desktopBackendRaw->Emit({DesktopBackendEventType::TrayCommand,
                1, 41});
        });
        desktopWorker.join();
        XIE_CHECK(desktopContext.Value()->PumpDesktopEvents());
        XIE_CHECK(callbackOnOwnerThread);

        desktopBackendRaw->Emit({DesktopBackendEventType::HealthChanged, 0, 0,
            ApplicationCapability::SystemTray, CapabilityStatus::Degraded,
            "TaskbarCreated", "tray_rebuild_failed", 123, true});
        XIE_CHECK(desktopContext.Value()->PumpDesktopEvents());
        const auto changedCapabilities = desktopContext.Value()->Capabilities();
        const auto* changedTray = changedCapabilities.Find(
            ApplicationCapability::SystemTray);
        XIE_CHECK(changedTray != nullptr);
        XIE_CHECK(changedTray->status == CapabilityStatus::Degraded);
        XIE_CHECK(changedTray->reason == "tray_rebuild_failed");
        XIE_CHECK(changedTray->health.lastNativeError == 123);
        XIE_CHECK(changedTray->health.revision > startedRevision);

        XIE_CHECK(desktopContext.Value()->Shutdown());
        const auto stoppedCapabilities = desktopContext.Value()->Capabilities();
        const auto* stoppedTray = stoppedCapabilities.Find(
            ApplicationCapability::SystemTray);
        XIE_CHECK(stoppedTray != nullptr);
        XIE_CHECK(stoppedTray->status == CapabilityStatus::Unavailable);
        XIE_CHECK(stoppedTray->health.reason == "service_shutdown");
    }

    FakeHost host;
    FakeWindows windows;
    ApplicationRuntime runtime({host, windows});
    WindowContext context;
    context.window = {1, 1};
    FakeProvider provider(windows, runtime, context);
    ApplicationContextOptions bridge;
    bridge.initializeBackends = false;
    bridge.runtime = &runtime;
    auto appResult = ApplicationContext::Create(std::move(bridge));
    XIE_CHECK(appResult);
    auto registration = appResult.Value()->RegisterWindowProvider(provider);
    XIE_CHECK(registration);
    const auto* openGl = appResult.Value()->Capabilities().Find(
        ApplicationCapability::OpenGLPresentation);
    XIE_CHECK(openGl != nullptr);
    XIE_CHECK(openGl->status == CapabilityStatus::Degraded);
    XIE_CHECK(openGl->reason == "runtime_probe_deferred:external_standard_provider");
    auto windowResult = appResult.Value()->Windows().CreateStandard();
    XIE_CHECK(windowResult);
    WindowSession session = std::move(windowResult.Value());
    XIE_CHECK(session.BindUI());
    XIE_CHECK(session.BackendName() == "fake");
    const auto placement = session.QueryPlacement();
    XIE_CHECK(placement);
    XIE_CHECK(placement.Value().width == 640);
    XIE_CHECK(placement.Value().height == 480);
    XIE_CHECK(!placement.Value().maximized);
    XIE_CHECK(session.RequestClose());
    XIE_CHECK(appResult.Value()->ProcessWindowEvents());
    XIE_CHECK(!session.QueryState().visible);
    XIE_CHECK(session.Show());
    XIE_CHECK(appResult.Value()->Wait(0.05));
    XIE_CHECK(host.WaitTimeoutCalls() == 1);
    XIE_CHECK(host.LastTimeout() == 0.05);
    const auto activeUnregister = registration.Value().Unregister();
    XIE_CHECK(!activeUnregister);
    XIE_CHECK(activeUnregister.Error().code ==
        ApplicationErrorCode::LifecycleViolation);
    const auto duplicate = session.BindUI();
    XIE_CHECK(!duplicate);
    XIE_CHECK(duplicate.Error().code == ApplicationErrorCode::AlreadyBound);

    ApplicationResult<void> wrongThread = ApplicationResult<void>::Success();
    std::thread worker([&] { wrongThread = session.SetSize(320, 240); });
    worker.join();
    XIE_CHECK(!wrongThread);
    XIE_CHECK(wrongThread.Error().code == ApplicationErrorCode::WrongThread);

    XIE_CHECK(session.QueryDpi());
    XIE_CHECK(session.QueryDpi().Value() == 1.25f);
    XIE_CHECK(session.Close());
    XIE_CHECK(session.Close());
    XIE_CHECK(registration.Value().Unregister());
    XIE_CHECK(appResult.Value()->Shutdown());

    ApplicationContextOptions directShutdown;
    directShutdown.initializeBackends = false;
    directShutdown.runtime = &runtime;
    auto directResult = ApplicationContext::Create(std::move(directShutdown));
    XIE_CHECK(directResult);
    auto directRegistration =
        directResult.Value()->RegisterWindowProvider(provider);
    XIE_CHECK(directRegistration);
    XIE_CHECK(directResult.Value()->Shutdown());
    XIE_CHECK(!directRegistration.Value().IsRegistered());
    XIE_CHECK(directRegistration.Value().Unregister());
    return 0;
}

#undef XIE_CHECK
