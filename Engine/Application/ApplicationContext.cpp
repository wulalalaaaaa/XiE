#include "Application/ApplicationContext.h"

#include "Application/ApplicationRuntime.h"
#include "Input/InputEventQueue.h"
#include "Platform/GLFW/GlfwApplicationHost.h"
#include "Platform/GLFW/GlfwOpenGLSurface.h"
#include "Platform/GLFW/GlfwWindowSystem.h"
#include "Platform/IApplicationHost.h"
#include "Platform/IWindowSystem.h"
#include "Platform/Win32/Win32ApplicationHost.h"
#include "Platform/Win32/Win32WindowSystem.h"
#include "Renderer/OpenGLBackend.h"
#include "Renderer2D/Backends/OpenGL/OpenGL2DRenderer.h"
#include "Renderer2D/Backends/OpenGL/OpenGLWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/LayeredWindowPresenter.h"
#include "Renderer2D/Backends/Software/LayeredWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/Software2DRenderer.h"
#include "Renderer2D/TextLayoutService.h"
#include "Renderer2D/TextureRegistry2D.h"
#include "UI2D/HitTest/BasicUIHitTester.h"
#include "UI2D/Input/BasicUIInputRouter.h"
#include "UI2D/Layout/BasicLayoutEngine.h"
#include "UI2D/Rendering/BasicUIRenderBuilder.h"
#include "UI2D/Runtime/UI2DServices.h"
#include "UI2D/Runtime/UIWindowRuntime.h"
#include "UI2D/Theme/ResolvedUITheme.h"
#include "Platform/Windows/TextInput/WindowsTextClipboard.h"
#include "Platform/Windows/TextInput/WindowsTextInputMethod.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <functional>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <GLFW/glfw3.h>
#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef CreateWindowEx
#undef CreateWindowEx
#endif
#ifdef IsMaximized
#undef IsMaximized
#endif
#endif

namespace Engine::Detail {

struct WindowSessionState;

struct ApplicationProviderBinding {
    IApplicationWindowProvider* provider = nullptr;
    std::string backend;
    bool active = true;
};

struct ApplicationCompositionState :
    std::enable_shared_from_this<ApplicationCompositionState> {
    std::thread::id ownerThread = std::this_thread::get_id();
    std::weak_ptr<ApplicationProviderBinding> providerBinding;
    ApplicationRuntime* runtime = nullptr;
    bool ownsRuntime = false;
    bool shuttingDown = false;
    bool running = false;
    bool backendsRequested = true;
    bool glfwRuntimeReady = false;
    bool win32RuntimeReady = false;
    std::string backendInitReason;
    std::shared_ptr<ImageServices> imageServices;
    std::function<void()> externalWakeCallback;
    mutable std::mutex externalWakeMutex;

    std::unique_ptr<GlfwApplicationHost> glfwHost;
    std::unique_ptr<GlfwWindowSystem> glfwWindows;
    std::unique_ptr<ApplicationRuntime> ownedRuntime;

#ifdef _WIN32
    std::unique_ptr<Win32ApplicationHost> win32Host;
    std::unique_ptr<Win32WindowSystem> win32Windows;
#endif

    std::vector<std::shared_ptr<WindowSessionState>> sessions;
};

struct WindowSessionState {
    std::weak_ptr<ApplicationCompositionState> owner;
    ApplicationWindowResources resources;
    WindowKind kind = WindowKind::Standard;
    WindowSize logicalSize{};
    WindowPlacement placement{};
    WindowFocusPolicy focusPolicy = WindowFocusPolicy::Preserve;
    WindowClosePolicy closePolicy = WindowClosePolicy::Hide;
    WindowDpiPolicy dpiPolicy = WindowDpiPolicy::PerMonitor;
    bool closed = false;
    bool closeHandled = false;
    bool dirty = true;
    float lastDpiScale = -1.0f;
    int lastLogicalWidth = 0;
    int lastLogicalHeight = 0;
    Render2DStats lastRenderStats{};
    std::string lastError;

    // The default services are window-local and are destroyed before the
    // native resource callback is invoked by WindowSession::Close().
    std::unique_ptr<BasicTextLayoutService> defaultTextLayouts;
    std::unique_ptr<TextureRegistry2D> defaultTextures;
    std::unique_ptr<UI2D::BasicLayoutEngine> defaultLayout;
    std::unique_ptr<UI2D::BasicUIHitTester> defaultHitTester;
    std::unique_ptr<UI2D::BasicUIInputRouter> defaultInputRouter;
    std::unique_ptr<UI2D::BasicUIRenderBuilder> defaultRenderBuilder;
#ifdef _WIN32
    std::unique_ptr<WindowsTextClipboard> defaultTextClipboard;
    std::unique_ptr<WindowsTextInputMethod> defaultTextInputMethod;
#endif
    TextureRegistry2D* textures = nullptr;
    ITextLayoutService* textLayouts = nullptr;
    ITextClipboard* textClipboard = nullptr;
    ITextInputMethod* textInputMethod = nullptr;
    std::unique_ptr<UI2D::UIWindowRuntime> ui;

    // Standard resources are owned by ApplicationRuntime's WindowContext.
    // Transparent resources use this local context until the layered runtime
    // is converged in a later phase.
    std::unique_ptr<WindowContext> ownedContext;
    std::unique_ptr<OpenGLBackend> openGlBackend;
    std::unique_ptr<OpenGL2DRenderer> openGlRenderer;
    std::unique_ptr<Software2DRenderer> softwareRenderer;
    std::unique_ptr<LayeredWindowPresenter> layeredPresenter;
};

} // namespace Engine::Detail

namespace Engine {
namespace {

using Detail::ApplicationCompositionState;
using Detail::WindowSessionState;

ApplicationError MakeError(ApplicationErrorCode code,
    std::string operation, std::string capability, std::string message,
    std::string backend = {}, std::optional<std::int64_t> nativeCode = {}) {
    return {code, std::move(operation), std::move(capability),
        std::move(message), nativeCode, std::move(backend)};
}

template <typename T>
ApplicationResult<T> Failure(ApplicationError error) {
    return ApplicationResult<T>::Failure(std::move(error));
}

void RecordError(const std::shared_ptr<WindowSessionState>& state,
    const ApplicationError& error) {
    if (state) state->lastError = error.message;
}

void ClearError(const std::shared_ptr<WindowSessionState>& state) {
    if (state) state->lastError.clear();
}

std::optional<ApplicationError> CheckThread(
    const std::shared_ptr<ApplicationCompositionState>& owner,
    std::string operation, std::string capability) {
    if (!owner) {
        return MakeError(ApplicationErrorCode::LifecycleViolation,
            std::move(operation), std::move(capability),
            "application composition is no longer alive");
    }
    if (std::this_thread::get_id() != owner->ownerThread) {
        return MakeError(ApplicationErrorCode::WrongThread,
            std::move(operation), std::move(capability),
            "operation must run on the application thread");
    }
    return std::nullopt;
}

std::optional<ApplicationError> CheckSession(
    const std::shared_ptr<WindowSessionState>& state,
    std::string operation, bool allowClosed = false) {
    if (!state) {
        return MakeError(ApplicationErrorCode::LifecycleViolation,
            std::move(operation), "window", "window session is empty");
    }
    const auto owner = state->owner.lock();
    if (const auto error = CheckThread(owner, operation, "window")) {
        return error;
    }
    if (owner->shuttingDown && !allowClosed) {
        return MakeError(ApplicationErrorCode::ShuttingDown,
            "WindowSession", "window", "application is shutting down",
            state->resources.backend);
    }
    if (state->closed && !allowClosed) {
        return MakeError(ApplicationErrorCode::LifecycleViolation,
            "WindowSession", "window", "window session is already closed",
            state->resources.backend);
    }
    return std::nullopt;
}

WindowContext* ResolveContext(const std::shared_ptr<WindowSessionState>& state) {
    if (!state || state->closed) return nullptr;
    if (state->ownedContext) return state->ownedContext.get();
    if (state->resources.context) return state->resources.context;
    if (state->resources.runtime) {
        return state->resources.runtime->Windows().TryGet(state->resources.window);
    }
    return nullptr;
}

const WindowContext* ResolveContext(
    const std::shared_ptr<const WindowSessionState>& state) {
    return ResolveContext(
        std::const_pointer_cast<WindowSessionState>(state));
}

void SyncContext(const std::shared_ptr<WindowSessionState>& state) {
    WindowContext* context = ResolveContext(state);
    if (!context || !state->resources.windows) return;
    auto& windows = *state->resources.windows;
    context->visible = windows.IsVisible(state->resources.window);
    context->minimized = windows.IsMinimized(state->resources.window);
    context->focused = windows.IsFocused(state->resources.window);
    context->closeRequested = windows.IsCloseRequested(state->resources.window);
    context->dpiScale = std::max(0.01f,
        windows.GetDpiScale(state->resources.window));
    context->framebufferSize = windows.GetFramebufferSize(
        state->resources.window);
    if ((context->framebufferSize.width <= 0 ||
            context->framebufferSize.height <= 0) && context->surface) {
        context->framebufferSize = context->surface->GetFramebufferSize();
    }
    if (context->closeRequested) state->closeHandled = false;
}

WindowFrameActivity ReadActivity(
    const std::shared_ptr<WindowSessionState>& state) {
    WindowFrameActivity result;
    if (!state || state->closed) return result;
    // Hidden sessions remain registered so Show()/Activate() can reuse the
    // same WindowSession, but they must not keep either the native outer loop
    // or the ApplicationContext loop in a render/update spin.  Show() marks
    // the session dirty; the visibility check then makes that work observable
    // on the first visible frame.
    if (!state->resources.windows ||
        !state->resources.windows->IsVisible(state->resources.window)) {
        return result;
    }
    result.needsRender = state->dirty;
    result.hasPendingWork = state->dirty;
    if (state->ui) {
        const auto ui = state->ui->QueryFrameActivity();
        result.hasActiveAnimation = ui.hasActiveAnimation;
        result.hasPendingWork = result.hasPendingWork || ui.HasWork();
        result.needsRender = result.needsRender || ui.needsRender;
    }
    return result;
}

std::array<float, 16> Projection(int width, int height) {
    const float safeWidth = static_cast<float>(std::max(1, width));
    const float safeHeight = static_cast<float>(std::max(1, height));
    return {2.0f / safeWidth, 0, 0, 0,
        0, -2.0f / safeHeight, 0, 0,
        0, 0, -1, 0,
        -1, 1, 0, 1};
}

WindowDesc StandardDesc(const StandardWindowOptions& options) {
    WindowDesc desc;
    desc.title = options.title;
    desc.width = options.size.width;
    desc.height = options.size.height;
    desc.visible = options.visible;
    desc.resizable = options.resizable;
    desc.decorated = options.decorated;
    desc.alwaysOnTop = options.alwaysOnTop;
    desc.toolWindow = options.toolWindow;
    desc.noActivate = options.focusPolicy == WindowFocusPolicy::NoActivate;
    return desc;
}

WindowDesc TransparentDesc(const TransparentWindowOptions& options) {
    WindowDesc desc;
    desc.title = options.title;
    desc.width = options.size.width;
    desc.height = options.size.height;
    desc.visible = options.visible;
    desc.resizable = false;
    desc.decorated = false;
    desc.transparent = options.transparent;
    desc.alwaysOnTop = options.alwaysOnTop;
    desc.toolWindow = options.toolWindow;
    desc.noActivate = options.focusPolicy == WindowFocusPolicy::NoActivate;
    desc.clickThrough = options.clickThrough;
    desc.opacity = options.opacity;
    return desc;
}

CapabilityInfo Capability(ApplicationCapability capability, bool compileTime,
    bool platform, bool initialized, std::string reason,
    std::string backend = {}) {
    CapabilityStatus status = CapabilityStatus::Supported;
    if (!compileTime || !platform || !initialized) {
        status = CapabilityStatus::Unavailable;
    } else if (!reason.empty()) {
        status = CapabilityStatus::Degraded;
    }
    CapabilityHealth health;
    health.status = status;
    health.reason = reason;
    return {capability, status, compileTime, platform, initialized,
        std::move(reason), std::move(backend), std::move(health)};
}

CapabilityInfo DesktopCapability(ApplicationCapability capability,
    bool compileTime, bool platform, bool initialized,
    const CapabilityHealth& health, std::string backend) {
    auto result = Capability(capability, compileTime, platform, initialized,
        health.reason, std::move(backend));
    result.status = health.status;
    result.reason = health.reason;
    result.health = health;
    return result;
}

CapabilitySnapshot BuildCapabilities(
    const std::shared_ptr<ApplicationCompositionState>& state,
    const DesktopServices& desktop) {
    const auto providerBinding = state->providerBinding.lock();
    auto* provider = providerBinding && providerBinding->active
        ? providerBinding->provider : nullptr;
    const bool standardByProvider = provider != nullptr &&
        provider->SupportsStandard();
    const bool transparentByProvider = provider != nullptr &&
        provider->SupportsTransparent();
    const bool standard = standardByProvider ||
        (state->glfwRuntimeReady && state->runtime != nullptr);
    const bool transparent = transparentByProvider ||
        (state->win32RuntimeReady && state->runtime != nullptr);
    const std::string backend = provider != nullptr &&
        provider->BackendName() != nullptr
        ? provider->BackendName() : "xie_internal";
    const std::string initReason = state->backendInitReason;

    std::vector<CapabilityInfo> values;
    values.reserve(18);
    values.push_back(Capability(
        ApplicationCapability::StandardWindow, true, true, standard,
        standard ? (provider ? "" : std::string{})
                 : (initReason.empty() ? "runtime_init_failed:standard_window"
                                        : initReason),
        standard ? backend : ""));
    values.push_back(Capability(
        ApplicationCapability::TransparentWindow,
#ifdef _WIN32
        true,
#else
        false,
#endif
        true, transparent,
        transparent ? (provider ? "" : std::string{})
                    : "platform_or_runtime_unavailable:transparent_window",
        transparent ? backend : ""));
    values.push_back(Capability(
        ApplicationCapability::UI2D, true, true, true, {}, "xie_ui2d"));
    values.push_back(Capability(
        ApplicationCapability::PointerAndKeyboardInput, true, true, true,
        {}, "xie_input"));
    values.push_back(Capability(
        ApplicationCapability::TextInput,
#ifdef _WIN32
        true,
#else
        false,
#endif
        true, state->runtime != nullptr,
        state->runtime != nullptr ? "" : "runtime_init_failed:text_input",
        "xie_platform_text_input"));
    values.push_back(Capability(
        ApplicationCapability::TextClipboard,
#ifdef _WIN32
        true,
#else
        false,
#endif
        true, state->runtime != nullptr,
        state->runtime != nullptr ? "" : "runtime_init_failed:text_clipboard",
        "xie_platform_text_clipboard"));
    const auto* imageServices = state->imageServices.get();
    const bool imageServicesConfigured = imageServices != nullptr;
    const bool imageServicesShuttingDown = imageServices != nullptr &&
        imageServices->IsShuttingDown();
    const bool imageCodec = imageServices != nullptr &&
        imageServices->HasCodec();
    const bool imageClipboard = imageServices != nullptr &&
        imageServices->HasClipboard();
    const bool imageFiles = imageServices != nullptr &&
        imageServices->HasFiles();
    const bool imageAsync = imageServices != nullptr &&
        imageServices->HasAsync();
    const std::string imageCodecReason = imageCodec ? std::string{}
        : imageServicesShuttingDown ? "service_shutdown:image_codec"
        : imageServicesConfigured ? "backend_unavailable:image_codec"
        : "module_not_configured:optional_image_codec";
    const std::string imageClipboardReason = imageClipboard ? std::string{}
        : imageServicesShuttingDown ? "service_shutdown:image_clipboard"
        : imageServicesConfigured ? "backend_unavailable:image_clipboard"
        : "module_not_configured:optional_image_clipboard";
    values.push_back(Capability(
        ApplicationCapability::ImageCodec, imageServicesConfigured,
        imageServicesConfigured, imageCodec, imageCodecReason,
        imageCodec ? imageServices->CodecBackendName() : std::string{}));
    values.push_back(Capability(
        ApplicationCapability::ImageClipboard, imageServicesConfigured,
        imageServicesConfigured, imageClipboard, imageClipboardReason,
        imageClipboard ? imageServices->ClipboardBackendName()
                       : std::string{}));
    const std::string imageFileReason = imageFiles ? std::string{}
        : imageServicesShuttingDown ? "service_shutdown:image_file"
        : imageServicesConfigured ? "backend_unavailable:image_file"
        : "module_not_configured:optional_image_file";
    values.push_back(Capability(
        ApplicationCapability::ImageFile, imageServicesConfigured,
        imageServicesConfigured, imageFiles, imageFileReason,
        imageFiles ? imageServices->FileBackendName() : std::string{}));
    const std::string imageAsyncReason = imageAsync ? std::string{}
        : imageServicesShuttingDown ? "service_shutdown:image_async_tasks"
        : imageServicesConfigured ? "backend_unavailable:image_async_tasks"
        : "module_not_configured:optional_image_async_tasks";
    values.push_back(Capability(
        ApplicationCapability::ImageAsyncTasks, imageServicesConfigured,
        imageServicesConfigured, imageAsync, imageAsyncReason,
        imageAsync ? imageServices->AsyncBackendName() : std::string{}));
    const bool openGlInitialized = state->glfwRuntimeReady || standardByProvider;
    const std::string openGlReason = !openGlInitialized
        ? "runtime_init_failed:opengl_context_deferred"
        : (!state->glfwRuntimeReady && standardByProvider
            ? "runtime_probe_deferred:external_standard_provider" : "");
    values.push_back(Capability(
        ApplicationCapability::OpenGLPresentation, true, true,
        openGlInitialized, openGlReason,
        openGlInitialized ? backend : ""));
    values.push_back(Capability(
        ApplicationCapability::LayeredPresentation,
#ifdef _WIN32
        true,
#else
        false,
#endif
        true, transparent,
        transparent ? "" : "platform_or_runtime_unavailable:layered_presenter",
        transparent ? backend : ""));
    values.push_back(Capability(
        ApplicationCapability::Diagnostics, true, true, true, {},
        "xie_application_diagnostics"));
    const auto desktopCapabilities = desktop.Capabilities();
    const auto desktopHealth = desktop.Health();
    const bool desktopPlatform = desktopCapabilities.systemTray ||
        desktopCapabilities.globalShortcut ||
        desktopCapabilities.singleInstance ||
        desktopCapabilities.primaryInstanceActivation;
    const bool desktopStarted = desktop.IsStarted();
    values.push_back(DesktopCapability(
        ApplicationCapability::DesktopIntegration, desktopPlatform,
        desktopPlatform, desktopStarted, desktopHealth.desktopIntegration,
        desktopCapabilities.backend));
    values.push_back(DesktopCapability(
        ApplicationCapability::SystemTray, desktopCapabilities.systemTray,
        desktopCapabilities.systemTray, desktopStarted,
        desktopHealth.systemTray, desktopCapabilities.backend));
    values.push_back(DesktopCapability(
        ApplicationCapability::GlobalShortcut,
        desktopCapabilities.globalShortcut, desktopCapabilities.globalShortcut,
        desktopStarted, desktopHealth.globalShortcut,
        desktopCapabilities.backend));
    values.push_back(DesktopCapability(
        ApplicationCapability::SingleInstance,
        desktopCapabilities.singleInstance, desktopCapabilities.singleInstance,
        desktopStarted, desktopHealth.singleInstance,
        desktopCapabilities.backend));
    values.push_back(DesktopCapability(
        ApplicationCapability::PrimaryInstanceActivation,
        desktopCapabilities.primaryInstanceActivation,
        desktopCapabilities.primaryInstanceActivation, desktopStarted,
        desktopHealth.primaryInstanceActivation,
        desktopCapabilities.backend));
    values.push_back(Capability(
        ApplicationCapability::MediaPlayback, false, false, false,
        "module_not_implemented:optional_media_services"));
    return CapabilitySnapshot(std::move(values));
}

void InitializeBackendsState(
    const std::shared_ptr<ApplicationCompositionState>& state) {
    if (!state || !state->backendsRequested || state->runtime != nullptr) {
        if (state && !state->backendsRequested && state->runtime == nullptr) {
            state->backendInitReason = "runtime_init_disabled";
        }
        return;
    }

    state->glfwHost = std::make_unique<GlfwApplicationHost>();
    state->glfwRuntimeReady = state->glfwHost->IsInitialized();
    if (state->glfwRuntimeReady) {
        state->glfwWindows = std::make_unique<GlfwWindowSystem>();
        state->ownedRuntime = std::make_unique<ApplicationRuntime>(
            ApplicationServices{*state->glfwHost, *state->glfwWindows});
        state->runtime = state->ownedRuntime.get();
        state->ownsRuntime = true;
    } else {
        state->backendInitReason = "runtime_init_failed:glfwInit";
    }

#ifdef _WIN32
    try {
        state->win32Host = std::make_unique<Win32ApplicationHost>();
        state->win32Windows = std::make_unique<Win32WindowSystem>();
        state->win32RuntimeReady = true;
    } catch (const std::exception& exception) {
        state->backendInitReason = std::string(
            "runtime_init_failed:win32_window_class:") + exception.what();
        state->win32Windows.reset();
        state->win32Host.reset();
    }
    // Transparent Win32 sessions do not depend on a GLFW/OpenGL context or
    // on the Explorer shell.  Keep ApplicationRuntime available for the
    // optional Win32 window composition even when GLFW initialization failed
    // (for example, a headless or graphics-degraded process).  Standard
    // windows remain unavailable and are reported separately through the
    // capability snapshot.
    if (state->runtime == nullptr && state->win32RuntimeReady &&
        state->win32Host && state->win32Windows) {
        state->ownedRuntime = std::make_unique<ApplicationRuntime>(
            ApplicationServices{*state->win32Host, *state->win32Windows});
        state->runtime = state->ownedRuntime.get();
        state->ownsRuntime = true;
    }
#endif
}

void RemoveClosedSessions(const std::shared_ptr<ApplicationCompositionState>& state) {
    state->sessions.erase(std::remove_if(state->sessions.begin(),
        state->sessions.end(), [](const auto& value) {
            return !value || value->closed;
        }), state->sessions.end());
}

std::shared_ptr<WindowSessionState> FindSession(
    const std::shared_ptr<ApplicationCompositionState>& state,
    WindowHandle window) {
    if (!state) return {};
    for (const auto& session : state->sessions) {
        if (session && !session->closed &&
            session->resources.window == window) return session;
    }
    return {};
}

ApplicationResult<void> ValidateBinding(const WindowSessionState& state,
    const UI2DBindingOptions& options) {
    if (!state.resources.windows) {
        return Failure<void>(MakeError(ApplicationErrorCode::BackendFailure,
            "BindUI", "ui2d", "window system is unavailable",
            state.resources.backend));
    }
    if (options.layoutEngine == nullptr &&
        options.inputRouter == nullptr &&
        options.renderBuilder == nullptr &&
        options.hitTester == nullptr) {
        return ApplicationResult<void>::Success();
    }
    // The pointers are intentionally borrowed.  There is no portable way to
    // validate their lifetime here, so document and keep that contract.
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> BindUIInternal(
    const std::shared_ptr<WindowSessionState>& state,
    const UI2DBindingOptions& options) {
    const auto validation = ValidateBinding(*state, options);
    if (!validation) {
        RecordError(state, validation.Error());
        return validation;
    }

    TextureRegistry2D* textures = options.textures;
    if (textures == nullptr) {
        state->defaultTextures = std::make_unique<TextureRegistry2D>();
        textures = state->defaultTextures.get();
    }
    ITextLayoutService* textLayouts = options.textLayouts;
    if (textLayouts == nullptr) {
        state->defaultTextLayouts = std::make_unique<BasicTextLayoutService>();
        textLayouts = state->defaultTextLayouts.get();
    }

    UI2D::IUILayoutEngine* layout = options.layoutEngine;
    if (layout == nullptr) {
        state->defaultLayout = std::make_unique<UI2D::BasicLayoutEngine>(
            textures, textLayouts);
        layout = state->defaultLayout.get();
    }
    UI2D::IUIHitTester* hitTester = options.hitTester;
    if (hitTester == nullptr) {
        state->defaultHitTester = std::make_unique<UI2D::BasicUIHitTester>();
        hitTester = state->defaultHitTester.get();
    }
    UI2D::IUIInputRouter* inputRouter = options.inputRouter;
    if (inputRouter == nullptr) {
        state->defaultInputRouter = std::make_unique<UI2D::BasicUIInputRouter>();
        inputRouter = state->defaultInputRouter.get();
    }
    UI2D::IUIRenderBuilder* renderBuilder = options.renderBuilder;
    if (renderBuilder == nullptr) {
        state->defaultRenderBuilder =
            std::make_unique<UI2D::BasicUIRenderBuilder>(textures, textLayouts);
        renderBuilder = state->defaultRenderBuilder.get();
    }

    ITextClipboard* textClipboard = options.textClipboard;
    ITextInputMethod* textInputMethod = options.textInputMethod;
#ifdef _WIN32
    if (textClipboard == nullptr) {
        state->defaultTextClipboard = std::make_unique<WindowsTextClipboard>();
        textClipboard = state->defaultTextClipboard.get();
    }
    if (textInputMethod == nullptr) {
        state->defaultTextInputMethod =
            std::make_unique<WindowsTextInputMethod>(*state->resources.windows);
        textInputMethod = state->defaultTextInputMethod.get();
    }
#endif

    const auto theme = options.theme ? options.theme
                                     : UI2D::MakeDefaultResolvedUITheme();
    state->ui = std::make_unique<UI2D::UIWindowRuntime>(
        state->resources.window,
        UI2D::UI2DServices{*layout, *hitTester, *inputRouter, *renderBuilder,
            textLayouts, textClipboard, textInputMethod,
            state->resources.windows},
        theme);
    state->textures = textures;
    state->textLayouts = textLayouts;
    state->textClipboard = textClipboard;
    state->textInputMethod = textInputMethod;

    if (options.attachInput && state->resources.attachInput &&
        !state->resources.attachInput(&state->ui->InputQueue())) {
        state->ui.reset();
        state->textures = nullptr;
        state->textLayouts = nullptr;
        state->textClipboard = nullptr;
        state->textInputMethod = nullptr;
        const auto error = MakeError(ApplicationErrorCode::BackendFailure,
            "BindUI", "pointer_keyboard_input",
            "input queue attachment failed", state->resources.backend);
        RecordError(state, error);
        return Failure<void>(error);
    }
    if (options.bindRenderResources && state->resources.bindRenderResources) {
        state->resources.bindRenderResources(textures, textLayouts);
    }
    SyncContext(state);
    if (WindowContext* context = ResolveContext(state); context != nullptr) {
        state->ui->SetVisible(context->visible);
    }
    state->dirty = true;
    ClearError(state);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> CloseInternal(
    const std::shared_ptr<WindowSessionState>& state) {
    if (!state || state->closed) return ApplicationResult<void>::Success();
    const auto owner = state->owner.lock();
    if (!owner) {
        state->ui.reset();
        state->closed = true;
        state->resources.window = {};
        return ApplicationResult<void>::Success();
    }
    if (const auto error = CheckThread(owner, "Close", "window")) {
        RecordError(state, *error);
        return Failure<void>(*error);
    }

    // UI owns focus/input references into the window system and is released
    // before pipelines, surfaces, renderers, and native windows.
    state->ui.reset();
    if (state->ownedContext) {
        state->ownedContext->renderPipeline.reset();
    }
    if (state->resources.close) {
        try {
            state->resources.close();
        } catch (const std::exception& exception) {
            const auto error = MakeError(ApplicationErrorCode::BackendFailure,
                "Close", "window", exception.what(), state->resources.backend);
            state->closed = true;
            RecordError(state, error);
            return Failure<void>(error);
        }
    }
    state->openGlRenderer.reset();
    state->softwareRenderer.reset();
    state->layeredPresenter.reset();
    state->openGlBackend.reset();
    state->ownedContext.reset();
    state->resources.context = nullptr;
    state->resources.window = {};
    state->closed = true;
    state->dirty = false;
    ClearError(state);
    return ApplicationResult<void>::Success();
}

bool HandleRuntimeClose(const std::shared_ptr<WindowSessionState>& state) {
    if (!state || state->closed || !state->resources.windows) return false;
    if (state->closePolicy == WindowClosePolicy::Hide) {
        state->resources.windows->HideWindow(state->resources.window);
        (void)state->resources.windows->ClearCloseRequest(
            state->resources.window);
        if (state->ui) state->ui->SetVisible(false);
        state->dirty = false;
        state->closeHandled = true;
        return true;
    }
    if (state->closePolicy == WindowClosePolicy::RequestApplicationExit &&
        state->resources.runtime != nullptr) {
        state->resources.runtime->RequestExit();
    }
    // Close through the same resource-ordering path used by WindowSession.
    // Returning true tells ApplicationRuntime that the callback already
    // unregistered/destroyed the provider-owned resource, so it must not
    // perform a second native destroy.
    return static_cast<bool>(CloseInternal(state));
}

ApplicationResult<void> InitializeInternalStandard(
    const std::shared_ptr<ApplicationCompositionState>& state,
    const std::shared_ptr<WindowSessionState>& session,
    const StandardWindowOptions& options) {
    if (!state->glfwWindows || !state->glfwRuntimeReady ||
        state->runtime == nullptr) {
        return Failure<void>(MakeError(ApplicationErrorCode::Unsupported,
            "CreateStandardWindow", "standard_window",
            state->backendInitReason.empty()
                ? "standard window backend is unavailable"
                : state->backendInitReason,
            "xie_platform_glfw"));
    }

    WindowHandle window{};
    try {
        window = state->glfwWindows->CreateWindow(StandardDesc(options));
    } catch (const std::exception& exception) {
        return Failure<void>(MakeError(ApplicationErrorCode::BackendFailure,
            "CreateStandardWindow", "standard_window", exception.what(),
            "xie_platform_glfw"));
    }
    if (!window.IsValid()) {
        return Failure<void>(MakeError(ApplicationErrorCode::BackendFailure,
            "CreateStandardWindow", "standard_window",
            "GLFW returned an invalid window handle", "xie_platform_glfw"));
    }

    const auto cleanupWindow = [&] { state->glfwWindows->DestroyWindow(window); };
    if (options.placement.x && options.placement.y) {
        state->glfwWindows->SetWindowPosition(window,
            *options.placement.x, *options.placement.y);
    }
#ifdef _WIN32
    if (options.placement.maximized) {
        if (GLFWwindow* native = state->glfwWindows->GetGlfwWindow(window)) {
            glfwMaximizeWindow(native);
        }
    }
#endif

    auto surface = std::make_unique<GlfwOpenGLSurface>(
        *state->glfwWindows, window);
    surface->MakeCurrent();
    auto backend = std::make_unique<OpenGLBackend>();
    if (!backend->Init()) {
        cleanupWindow();
        return Failure<void>(MakeError(ApplicationErrorCode::BackendFailure,
            "CreateStandardWindow", "opengl_presentation",
            "OpenGL backend initialization failed", "xie_renderer2d_opengl"));
    }
    backend->SetViewProjection(
        Projection(options.size.width, options.size.height).data());
    auto renderer = std::make_unique<OpenGL2DRenderer>();
    auto pipeline = std::make_unique<OpenGLWindowRenderPipeline>(
        *surface, *backend, *renderer);
    WindowContext context;
    context.window = window;
    context.surface = std::move(surface);
    context.renderPipeline = std::move(pipeline);
    context.visible = options.visible;
    context.focused = options.focusPolicy == WindowFocusPolicy::Activate;
    context.framePolicy = options.visible
        ? FramePolicy::Static : FramePolicy::Hidden;
    context.dirty = true;
    if (!state->runtime->Windows().Register(std::move(context)).IsValid()) {
        backend->Shutdown();
        cleanupWindow();
        return Failure<void>(MakeError(ApplicationErrorCode::BackendFailure,
            "CreateStandardWindow", "standard_window",
            "ApplicationRuntime rejected the window context",
            "xie_application"));
    }

    session->resources.window = window;
    session->resources.windows = state->glfwWindows.get();
    session->resources.runtime = state->runtime;
    session->resources.context = state->runtime->Windows().TryGet(window);
    session->resources.backend = "xie_platform_glfw/xie_renderer2d_opengl";
    session->openGlBackend = std::move(backend);
    session->openGlRenderer = std::move(renderer);
    const std::weak_ptr<ApplicationCompositionState> weakOwner = state;
    OpenGLBackend* backendPtr = session->openGlBackend.get();
    OpenGL2DRenderer* rendererPtr = session->openGlRenderer.get();
    session->resources.attachInput = [windows = state->glfwWindows.get(),
        window](InputEventQueue* queue) {
        return windows != nullptr && windows->SetInputEventQueue(window, queue);
    };
    session->resources.bindRenderResources = [rendererPtr](
        TextureRegistry2D* textures, ITextLayoutService* text) {
        if (rendererPtr == nullptr) return;
        rendererPtr->SetTextureRegistry(textures);
        rendererPtr->SetTextLayoutService(text);
        rendererPtr->PrepareTextForDpiScale(1.0f);
    };
    session->resources.close = [weakOwner, window, backendPtr,
        windows = state->glfwWindows.get(), runtime = state->runtime] {
        if (auto owner = weakOwner.lock()) {
            if (backendPtr != nullptr) {
                if (WindowContext* context = runtime->Windows().TryGet(window);
                    context != nullptr && context->surface) {
                    context->surface->MakeCurrent();
                }
                backendPtr->Shutdown();
            }
            (void)runtime->Windows().Unregister(window);
            if (windows != nullptr) windows->DestroyWindow(window);
        }
    };
    if (options.visible && options.focusPolicy == WindowFocusPolicy::Activate) {
        (void)state->glfwWindows->ActivateWindow(window);
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> InitializeInternalTransparent(
    const std::shared_ptr<ApplicationCompositionState>& state,
    const std::shared_ptr<WindowSessionState>& session,
    const TransparentWindowOptions& options) {
#ifdef _WIN32
    if (!state->win32Windows || state->runtime == nullptr) {
        return Failure<void>(MakeError(ApplicationErrorCode::Unsupported,
            "CreateTransparentWindow", "transparent_window",
            "Win32 layered window backend is unavailable",
            "xie_platform_win32"));
    }
    WindowHandle window{};
    try {
        window = state->win32Windows->CreateWindow(TransparentDesc(options));
    } catch (const std::exception& exception) {
        return Failure<void>(MakeError(ApplicationErrorCode::BackendFailure,
            "CreateTransparentWindow", "transparent_window", exception.what(),
            "xie_platform_win32"));
    }
    if (!window.IsValid()) {
        return Failure<void>(MakeError(ApplicationErrorCode::BackendFailure,
            "CreateTransparentWindow", "transparent_window",
            "Win32 returned an invalid window handle", "xie_platform_win32"));
    }
    if (options.placement.x && options.placement.y) {
        state->win32Windows->SetWindowPosition(window,
            *options.placement.x, *options.placement.y);
    }
    auto renderer = std::make_unique<Software2DRenderer>();
    auto presenter = std::make_unique<LayeredWindowPresenter>(
        state->win32Windows->GetNativeHandle(window));
    presenter->SetOpacity(options.opacity);
    auto pipeline = std::make_unique<LayeredWindowRenderPipeline>(
        *renderer, *presenter);
    pipeline->Resize(options.size.width, options.size.height,
        state->win32Windows->GetDpiScale(window));
    session->ownedContext = std::make_unique<WindowContext>();
    session->ownedContext->window = window;
    session->ownedContext->renderPipeline = std::move(pipeline);
    session->ownedContext->visible = options.visible;
    session->ownedContext->focused = false;
    session->ownedContext->framePolicy = options.visible
        ? FramePolicy::Static : FramePolicy::Hidden;
    session->ownedContext->dirty = true;
    session->resources.window = window;
    session->resources.windows = state->win32Windows.get();
    session->resources.runtime = state->runtime;
    session->resources.context = session->ownedContext.get();
    session->resources.backend =
        "xie_platform_win32/xie_renderer2d_software_layered";
    session->softwareRenderer = std::move(renderer);
    session->layeredPresenter = std::move(presenter);
    Software2DRenderer* rendererPtr = session->softwareRenderer.get();
    session->resources.attachInput = [windows = state->win32Windows.get(),
        window](InputEventQueue* queue) {
        return windows != nullptr && windows->SetInputEventQueue(window, queue);
    };
    session->resources.bindRenderResources = [rendererPtr](
        TextureRegistry2D* textures, ITextLayoutService* text) {
        if (rendererPtr == nullptr) return;
        rendererPtr->SetTextureRegistry(textures);
        rendererPtr->SetTextLayoutService(text);
    };
    const std::weak_ptr<ApplicationCompositionState> weakOwner = state;
    session->resources.close = [weakOwner, window,
        windows = state->win32Windows.get()] {
        if (auto owner = weakOwner.lock()) {
            if (windows != nullptr) windows->DestroyWindow(window);
        }
    };
    if (options.visible && options.focusPolicy == WindowFocusPolicy::Activate) {
        (void)state->win32Windows->ActivateWindow(window);
    }
    return ApplicationResult<void>::Success();
#else
    (void)state;
    (void)session;
    (void)options;
    return Failure<void>(MakeError(ApplicationErrorCode::Unsupported,
        "CreateTransparentWindow", "transparent_window",
        "transparent windows require the optional platform backend",
        "xie_platform_win32"));
#endif
}

} // namespace

ApplicationResult<WindowSession> WindowComposition::CreateSession(
    WindowKind kind, const StandardWindowOptions* standard,
    const TransparentWindowOptions* transparent) {
    const auto& state = m_State;
    auto session = std::make_shared<WindowSessionState>();
    session->owner = state;
    session->kind = kind;
    if (standard != nullptr) {
        session->logicalSize = standard->size;
        session->placement = standard->placement;
        session->focusPolicy = standard->focusPolicy;
        session->closePolicy = standard->closePolicy;
        session->dpiPolicy = standard->dpiPolicy;
    } else {
        session->logicalSize = transparent->size;
        session->placement = transparent->placement;
        session->focusPolicy = transparent->focusPolicy;
        session->closePolicy = transparent->closePolicy;
        session->dpiPolicy = transparent->dpiPolicy;
    }

    ApplicationResult<void> initialized = ApplicationResult<void>::Success();
    const auto providerBinding = state->providerBinding.lock();
    auto* provider = providerBinding && providerBinding->active
        ? providerBinding->provider : nullptr;
    if (provider != nullptr) {
        ApplicationResult<ApplicationWindowResources> result = kind ==
            WindowKind::Standard
            ? provider->CreateStandard(*standard)
            : provider->CreateTransparent(*transparent);
        if (!result) return Failure<WindowSession>(result.Error());
        session->resources = std::move(result.Value());
        if (session->resources.backend.empty() &&
            provider->BackendName() != nullptr) {
            session->resources.backend = provider->BackendName();
        }
        if (state->runtime == nullptr) state->runtime = session->resources.runtime;
        if (!session->resources.IsValid()) {
            return Failure<WindowSession>(MakeError(
                ApplicationErrorCode::BackendFailure,
                kind == WindowKind::Standard ? "CreateStandardWindow"
                                             : "CreateTransparentWindow",
                kind == WindowKind::Standard ? "standard_window"
                                             : "transparent_window",
                "window provider returned incomplete resources",
                session->resources.backend));
        }
    } else {
        initialized = kind == WindowKind::Standard
            ? InitializeInternalStandard(state, session, *standard)
            : InitializeInternalTransparent(state, session, *transparent);
        if (!initialized) return Failure<WindowSession>(initialized.Error());
    }
    state->sessions.push_back(session);
    return ApplicationResult<WindowSession>::Success(WindowSession(session));
}

WindowSession::WindowSession(
    std::shared_ptr<Detail::WindowSessionState> state,
    bool closeOnDestruction)
    : m_State(std::move(state)), m_CloseOnDestruction(closeOnDestruction) {}

WindowSession::~WindowSession() noexcept {
    if (m_CloseOnDestruction) (void)Close();
}

WindowSession::WindowSession(WindowSession&& other) noexcept
    : m_State(std::move(other.m_State)),
      m_CloseOnDestruction(other.m_CloseOnDestruction) {
    other.m_CloseOnDestruction = false;
}

WindowSession& WindowSession::operator=(WindowSession&& other) noexcept {
    if (this == &other) return *this;
    if (m_CloseOnDestruction) (void)Close();
    m_State = std::move(other.m_State);
    m_CloseOnDestruction = other.m_CloseOnDestruction;
    other.m_CloseOnDestruction = false;
    return *this;
}

bool WindowSession::IsValid() const noexcept {
    return m_State != nullptr && !m_State->closed &&
        m_State->resources.window.IsValid();
}

bool WindowSession::IsClosed() const noexcept {
    return m_State == nullptr || m_State->closed;
}

WindowKind WindowSession::Kind() const noexcept {
    return m_State ? m_State->kind : WindowKind::Standard;
}

WindowHandle WindowSession::Handle() const noexcept {
    return m_State ? m_State->resources.window : WindowHandle{};
}

WindowSessionStatus WindowSession::Status() const noexcept {
    if (!m_State || m_State->closed) return WindowSessionStatus::Closed;
    if (!m_State->resources.windows) return WindowSessionStatus::Failed;
    if (m_State->resources.windows->IsCloseRequested(
            m_State->resources.window)) {
        return WindowSessionStatus::CloseRequested;
    }
    return m_State->resources.windows->IsVisible(
        m_State->resources.window)
        ? WindowSessionStatus::Visible : WindowSessionStatus::Hidden;
}

WindowStateSnapshot WindowSession::QueryState() const {
    WindowStateSnapshot result;
    if (!m_State || m_State->closed || !m_State->resources.windows) return result;
    auto& windows = *m_State->resources.windows;
    result.status = Status();
    result.visible = windows.IsVisible(m_State->resources.window);
    result.minimized = windows.IsMinimized(m_State->resources.window);
    result.focused = windows.IsFocused(m_State->resources.window);
    result.closeRequested = windows.IsCloseRequested(m_State->resources.window);
    result.dpiScale = std::max(0.01f,
        windows.GetDpiScale(m_State->resources.window));
    result.framebufferSize = windows.GetFramebufferSize(
        m_State->resources.window);
    return result;
}

ApplicationResult<void> WindowSession::Show() {
    if (const auto error = CheckSession(m_State, "Show")) {
        RecordError(m_State, *error);
        return Failure<void>(*error);
    }
    if (!m_State->resources.windows) {
        const auto error = MakeError(ApplicationErrorCode::BackendFailure,
            "Show", "window", "window system is unavailable",
            m_State->resources.backend);
        RecordError(m_State, error);
        return Failure<void>(error);
    }
    if (m_State->focusPolicy == WindowFocusPolicy::Activate) {
        if (!m_State->resources.windows->ActivateWindow(
                m_State->resources.window)) {
            const auto error = MakeError(ApplicationErrorCode::BackendFailure,
                "Show", "window", "window backend could not show and activate it",
                m_State->resources.backend);
            RecordError(m_State, error);
            return Failure<void>(error);
        }
    } else {
        m_State->resources.windows->ShowWindow(m_State->resources.window);
    }
    if (m_State->ui) m_State->ui->SetVisible(true);
    m_State->dirty = true;
    ClearError(m_State);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> WindowSession::Hide() {
    if (const auto error = CheckSession(m_State, "Hide")) {
        RecordError(m_State, *error);
        return Failure<void>(*error);
    }
    if (!m_State->resources.windows) {
        const auto error = MakeError(ApplicationErrorCode::BackendFailure,
            "Hide", "window", "window system is unavailable",
            m_State->resources.backend);
        RecordError(m_State, error);
        return Failure<void>(error);
    }
    m_State->resources.windows->HideWindow(m_State->resources.window);
    if (m_State->ui) m_State->ui->SetVisible(false);
    m_State->dirty = false;
    ClearError(m_State);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> WindowSession::Activate() {
    if (const auto error = CheckSession(m_State, "Activate")) {
        RecordError(m_State, *error);
        return Failure<void>(*error);
    }
    if (!m_State->resources.windows ||
        !m_State->resources.windows->ActivateWindow(m_State->resources.window)) {
        const auto error = MakeError(ApplicationErrorCode::BackendFailure,
            "Activate", "window", "window backend could not activate it",
            m_State->resources.backend);
        RecordError(m_State, error);
        return Failure<void>(error);
    }
    if (m_State->ui) m_State->ui->SetVisible(true);
    m_State->dirty = true;
    ClearError(m_State);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> WindowSession::SetPosition(int x, int y) {
    if (const auto error = CheckSession(m_State, "SetPosition")) {
        RecordError(m_State, *error);
        return Failure<void>(*error);
    }
    if (!m_State->resources.windows) {
        const auto error = MakeError(ApplicationErrorCode::BackendFailure,
            "SetPosition", "window", "window system is unavailable",
            m_State->resources.backend);
        RecordError(m_State, error);
        return Failure<void>(error);
    }
    m_State->resources.windows->SetWindowPosition(
        m_State->resources.window, x, y);
    ClearError(m_State);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> WindowSession::SetSize(int width, int height) {
    if (const auto error = CheckSession(m_State, "SetSize")) {
        RecordError(m_State, *error);
        return Failure<void>(*error);
    }
    if (width <= 0 || height <= 0) {
        const auto error = MakeError(ApplicationErrorCode::InvalidOptions,
            "SetSize", "standard_window", "window size must be positive",
            m_State->resources.backend);
        RecordError(m_State, error);
        return Failure<void>(error);
    }
    if (!m_State->resources.windows) {
        const auto error = MakeError(ApplicationErrorCode::BackendFailure,
            "SetSize", "window", "window system is unavailable",
            m_State->resources.backend);
        RecordError(m_State, error);
        return Failure<void>(error);
    }
    m_State->resources.windows->SetWindowSize(
        m_State->resources.window, width, height);
    m_State->logicalSize = {width, height};
    m_State->dirty = true;
    ClearError(m_State);
    return ApplicationResult<void>::Success();
}

ApplicationResult<float> WindowSession::QueryDpi() const {
    if (const auto error = CheckSession(m_State, "QueryDpi")) {
        return Failure<float>(*error);
    }
    if (!m_State->resources.windows) {
        return Failure<float>(MakeError(ApplicationErrorCode::BackendFailure,
            "QueryDpi", "window", "window system is unavailable",
            m_State->resources.backend));
    }
    return ApplicationResult<float>::Success(std::max(0.01f,
        m_State->resources.windows->GetDpiScale(m_State->resources.window)));
}

ApplicationResult<WindowPlacementSnapshot> WindowSession::QueryPlacement() const {
    if (const auto error = CheckSession(m_State, "QueryPlacement")) {
        return Failure<WindowPlacementSnapshot>(*error);
    }
    if (!m_State->resources.windows) {
        return Failure<WindowPlacementSnapshot>(MakeError(
            ApplicationErrorCode::BackendFailure, "QueryPlacement", "window",
            "window system is unavailable", m_State->resources.backend));
    }

    const float dpi = std::max(0.01f,
        m_State->resources.windows->GetDpiScale(m_State->resources.window));
    const RenderSurfaceSize framebuffer =
        m_State->resources.windows->GetFramebufferSize(m_State->resources.window);
    WindowPlacementSnapshot result;
    result.x = static_cast<int>(std::lround(
        m_State->resources.windows->GetWindowScreenPositionPhysical(
            m_State->resources.window).x));
    result.y = static_cast<int>(std::lround(
        m_State->resources.windows->GetWindowScreenPositionPhysical(
            m_State->resources.window).y));
    result.width = framebuffer.width > 0
        ? static_cast<int>(std::lround(static_cast<float>(framebuffer.width) / dpi))
        : m_State->logicalSize.width;
    result.height = framebuffer.height > 0
        ? static_cast<int>(std::lround(static_cast<float>(framebuffer.height) / dpi))
        : m_State->logicalSize.height;
    result.maximized = m_State->resources.windows->IsMaximized(
        m_State->resources.window);
    return ApplicationResult<WindowPlacementSnapshot>::Success(result);
}

ApplicationResult<bool> WindowSession::QueryFocus() const {
    if (const auto error = CheckSession(m_State, "QueryFocus")) {
        return Failure<bool>(*error);
    }
    if (!m_State->resources.windows) {
        return Failure<bool>(MakeError(ApplicationErrorCode::BackendFailure,
            "QueryFocus", "window", "window system is unavailable",
            m_State->resources.backend));
    }
    return ApplicationResult<bool>::Success(
        m_State->resources.windows->IsFocused(m_State->resources.window));
}

ApplicationResult<void> WindowSession::RequestClose() {
    if (const auto error = CheckSession(m_State, "RequestClose")) {
        RecordError(m_State, *error);
        return Failure<void>(*error);
    }
    if (!m_State->resources.windows ||
        !m_State->resources.windows->RequestClose(m_State->resources.window)) {
        const auto error = MakeError(ApplicationErrorCode::BackendFailure,
            "RequestClose", "window", "window backend rejected close request",
            m_State->resources.backend);
        RecordError(m_State, error);
        return Failure<void>(error);
    }
    ClearError(m_State);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> WindowSession::Close() {
    if (!m_State || m_State->closed) return ApplicationResult<void>::Success();
    const auto result = CloseInternal(m_State);
    if (!result) RecordError(m_State, result.Error());
    return result;
}

ApplicationResult<UI2D::UIWindowRuntime*> WindowSession::BindUI(
    const UI2DBindingOptions& options) {
    if (const auto error = CheckSession(m_State, "BindUI")) {
        RecordError(m_State, *error);
        return Failure<UI2D::UIWindowRuntime*>(*error);
    }
    if (m_State->ui) {
        const auto error = MakeError(ApplicationErrorCode::AlreadyBound,
            "BindUI", "ui2d", "UI2D is already bound to this window",
            m_State->resources.backend);
        RecordError(m_State, error);
        return Failure<UI2D::UIWindowRuntime*>(error);
    }
    const auto result = BindUIInternal(m_State, options);
    if (!result) return Failure<UI2D::UIWindowRuntime*>(result.Error());
    return ApplicationResult<UI2D::UIWindowRuntime*>::Success(m_State->ui.get());
}

UI2D::UIWindowRuntime* WindowSession::UI() noexcept {
    return m_State ? m_State->ui.get() : nullptr;
}

const UI2D::UIWindowRuntime* WindowSession::UI() const noexcept {
    return m_State ? m_State->ui.get() : nullptr;
}

InputEventQueue* WindowSession::InputQueue() noexcept {
    return m_State && m_State->ui ? &m_State->ui->InputQueue() : nullptr;
}

TextureRegistry2D* WindowSession::Textures() noexcept {
    return m_State ? m_State->textures : nullptr;
}

ITextLayoutService* WindowSession::TextLayouts() noexcept {
    return m_State ? m_State->textLayouts : nullptr;
}

ITextClipboard* WindowSession::TextClipboard() noexcept {
    return m_State ? m_State->textClipboard : nullptr;
}

ITextInputMethod* WindowSession::TextInputMethod() noexcept {
    return m_State ? m_State->textInputMethod : nullptr;
}

const std::string& WindowSession::LastError() const noexcept {
    static const std::string empty;
    return m_State ? m_State->lastError : empty;
}

const std::string& WindowSession::BackendName() const noexcept {
    static const std::string empty;
    return m_State ? m_State->resources.backend : empty;
}

WindowComposition::WindowComposition(
    std::shared_ptr<Detail::ApplicationCompositionState> state)
    : m_State(std::move(state)) {}

WindowComposition::~WindowComposition() noexcept {
    (void)CloseAll();
}

WindowComposition::WindowComposition(WindowComposition&& other) noexcept
    : m_State(std::move(other.m_State)) {}

WindowComposition& WindowComposition::operator=(WindowComposition&& other) noexcept {
    if (this == &other) return *this;
    (void)CloseAll();
    m_State = std::move(other.m_State);
    return *this;
}

ApplicationResult<void> WindowComposition::ValidateStandardOptions(
    const StandardWindowOptions& options) {
    if (options.title.size() > 1024) {
        return Failure<void>(MakeError(ApplicationErrorCode::InvalidOptions,
            "CreateStandardWindow", "standard_window",
            "window title exceeds 1024 bytes"));
    }
    if (options.size.width <= 0 || options.size.height <= 0 ||
        options.size.width > 16384 || options.size.height > 16384) {
        return Failure<void>(MakeError(ApplicationErrorCode::InvalidOptions,
            "CreateStandardWindow", "standard_window",
            "window size must be within 1..16384 logical pixels"));
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> WindowComposition::ValidateTransparentOptions(
    const TransparentWindowOptions& options) {
    if (options.title.size() > 1024) {
        return Failure<void>(MakeError(ApplicationErrorCode::InvalidOptions,
            "CreateTransparentWindow", "transparent_window",
            "window title exceeds 1024 bytes"));
    }
    if (options.size.width <= 0 || options.size.height <= 0 ||
        options.size.width > 16384 || options.size.height > 16384) {
        return Failure<void>(MakeError(ApplicationErrorCode::InvalidOptions,
            "CreateTransparentWindow", "transparent_window",
            "window size must be within 1..16384 logical pixels"));
    }
    if (!options.transparent) {
        return Failure<void>(MakeError(ApplicationErrorCode::InvalidOptions,
            "CreateTransparentWindow", "transparent_window",
            "transparent window options must request transparency"));
    }
    if (!std::isfinite(options.opacity) || options.opacity < 0.0f ||
        options.opacity > 1.0f) {
        return Failure<void>(MakeError(ApplicationErrorCode::InvalidOptions,
            "CreateTransparentWindow", "transparent_window",
            "opacity must be finite and within 0..1"));
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<WindowSession> WindowComposition::CreateStandard(
    const StandardWindowOptions& options) {
    if (const auto error = CheckThread(m_State, "CreateStandardWindow",
            "standard_window")) return Failure<WindowSession>(*error);
    if (m_State->shuttingDown) {
        return Failure<WindowSession>(MakeError(
            ApplicationErrorCode::ShuttingDown, "CreateStandardWindow",
            "standard_window", "application is shutting down"));
    }
    if (const auto result = ValidateStandardOptions(options); !result) {
        return Failure<WindowSession>(result.Error());
    }
    return CreateSession(WindowKind::Standard, &options, nullptr);
}

ApplicationResult<WindowSession> WindowComposition::CreateTransparent(
    const TransparentWindowOptions& options) {
    if (const auto error = CheckThread(m_State, "CreateTransparentWindow",
            "transparent_window")) return Failure<WindowSession>(*error);
    if (m_State->shuttingDown) {
        return Failure<WindowSession>(MakeError(
            ApplicationErrorCode::ShuttingDown, "CreateTransparentWindow",
            "transparent_window", "application is shutting down"));
    }
    if (const auto result = ValidateTransparentOptions(options); !result) {
        return Failure<WindowSession>(result.Error());
    }
    const auto providerBinding = m_State->providerBinding.lock();
    const bool hasProvider = providerBinding && providerBinding->active &&
        providerBinding->provider != nullptr;
    if (!hasProvider && !m_State->win32RuntimeReady) {
        return Failure<WindowSession>(MakeError(
            ApplicationErrorCode::Unsupported, "CreateTransparentWindow",
            "transparent_window",
            "transparent window backend is unavailable",
            "xie_platform_win32"));
    }
    return CreateSession(WindowKind::Transparent, nullptr, &options);
}

ApplicationResult<void> WindowComposition::CloseAll() {
    if (!m_State) return ApplicationResult<void>::Success();
    if (const auto error = CheckThread(m_State, "CloseAll", "window")) {
        if (!m_State->shuttingDown) return Failure<void>(*error);
    }
    const auto sessions = m_State->sessions;
    for (const auto& state : sessions) {
        const auto result = CloseInternal(state);
        if (!result) return result;
    }
    RemoveClosedSessions(m_State);
    return ApplicationResult<void>::Success();
}

std::size_t WindowComposition::Size() const noexcept {
    if (!m_State) return 0;
    return static_cast<std::size_t>(std::count_if(m_State->sessions.begin(),
        m_State->sessions.end(), [](const auto& value) {
            return value && !value->closed;
        }));
}

ApplicationContext::ApplicationContext(
    std::shared_ptr<Detail::ApplicationCompositionState> state,
    std::unique_ptr<IDesktopBackend> desktopBackend,
    std::unique_ptr<WindowComposition> windows)
    : m_State(std::move(state)), m_Desktop(std::move(desktopBackend)),
      m_Capabilities(BuildCapabilities(m_State, m_Desktop)),
      m_Windows(std::move(windows)) {
    const std::weak_ptr<Detail::ApplicationCompositionState> weak = m_State;
    m_Desktop.SetOwnerWakeCallback([weak] {
        const auto state = weak.lock();
        if (!state || !state->runtime) return;
        state->runtime->Services().host.WakeEventLoop();
#ifdef _WIN32
        if (state->win32Host) state->win32Host->WakeEventLoop();
#endif
        std::function<void()> externalWake;
        {
            std::lock_guard lock(state->externalWakeMutex);
            externalWake = state->externalWakeCallback;
        }
        if (externalWake) externalWake();
    });
}

WindowSession ApplicationContext::MakeSessionView(
    const std::shared_ptr<Detail::WindowSessionState>& state) {
    return WindowSession(state, false);
}

ApplicationContext::~ApplicationContext() noexcept {
    (void)Shutdown();
}

ApplicationResult<std::unique_ptr<ApplicationContext>> ApplicationContext::Create(
    ApplicationContextOptions options) {
    if (options.applicationName.empty() || options.applicationName.size() > 256) {
        return Failure<std::unique_ptr<ApplicationContext>>(MakeError(
            ApplicationErrorCode::InvalidOptions, "CreateApplicationContext",
            "diagnostics", "application name must be 1..256 bytes"));
    }
    try {
        auto state = std::make_shared<Detail::ApplicationCompositionState>();
        state->runtime = options.runtime;
        state->backendsRequested = options.initializeBackends;
        state->imageServices = std::move(options.imageServices);
        if (options.initializeBackends) InitializeBackendsState(state);
        else state->backendInitReason = "runtime_init_disabled";

        auto windows = std::unique_ptr<WindowComposition>(
            new WindowComposition(state));
        auto context = std::unique_ptr<ApplicationContext>(
            new ApplicationContext(state, std::move(options.desktopBackend),
                std::move(windows)));
        return ApplicationResult<std::unique_ptr<ApplicationContext>>::Success(
            std::move(context));
    } catch (const std::exception& exception) {
        return Failure<std::unique_ptr<ApplicationContext>>(MakeError(
            ApplicationErrorCode::UnknownFailure, "CreateApplicationContext",
            "diagnostics", exception.what()));
    }
}

const CapabilitySnapshot& ApplicationContext::Capabilities() const noexcept {
    if (m_State) m_Capabilities = BuildCapabilities(m_State, m_Desktop);
    return m_Capabilities;
}

WindowComposition& ApplicationContext::Windows() noexcept { return *m_Windows; }
const WindowComposition& ApplicationContext::Windows() const noexcept {
    return *m_Windows;
}

DesktopServices& ApplicationContext::Desktop() noexcept { return m_Desktop; }
const DesktopServices& ApplicationContext::Desktop() const noexcept {
    return m_Desktop;
}

ImageServices* ApplicationContext::Images() noexcept {
    return m_State ? m_State->imageServices.get() : nullptr;
}

const ImageServices* ApplicationContext::Images() const noexcept {
    return m_State ? m_State->imageServices.get() : nullptr;
}

ApplicationProviderRegistration::ApplicationProviderRegistration(
    std::weak_ptr<Detail::ApplicationCompositionState> context,
    std::shared_ptr<Detail::ApplicationProviderBinding> binding)
    : m_Context(std::move(context)), m_Binding(std::move(binding)) {}

ApplicationProviderRegistration::~ApplicationProviderRegistration() noexcept {
    (void)Unregister();
}

ApplicationProviderRegistration::ApplicationProviderRegistration(
    ApplicationProviderRegistration&& other) noexcept
    : m_Context(std::move(other.m_Context)),
      m_Binding(std::move(other.m_Binding)) {}

ApplicationProviderRegistration& ApplicationProviderRegistration::operator=(
    ApplicationProviderRegistration&& other) noexcept {
    if (this == &other) return *this;
    (void)Unregister();
    m_Context = std::move(other.m_Context);
    m_Binding = std::move(other.m_Binding);
    return *this;
}

ApplicationResult<void> ApplicationProviderRegistration::Unregister() {
    if (!m_Binding) return ApplicationResult<void>::Success();
    const auto context = m_Context.lock();
    if (!context || !m_Binding->active) {
        m_Binding->active = false;
        m_Binding.reset();
        m_Context.reset();
        return ApplicationResult<void>::Success();
    }
    if (const auto error = CheckThread(context, "UnregisterWindowProvider",
            "window_provider")) {
        return Failure<void>(*error);
    }
    if (std::any_of(context->sessions.begin(), context->sessions.end(),
            [](const auto& session) {
                return session && !session->closed;
            })) {
        return Failure<void>(MakeError(
            ApplicationErrorCode::LifecycleViolation,
            "UnregisterWindowProvider", "window_provider",
            "close all provider-created window sessions before unregistering",
            m_Binding->backend.empty() ? "provider" : m_Binding->backend));
    }
    if (context->providerBinding.lock() == m_Binding) {
        context->providerBinding.reset();
    }
    m_Binding->active = false;
    m_Binding.reset();
    m_Context.reset();
    return ApplicationResult<void>::Success();
}

bool ApplicationProviderRegistration::IsRegistered() const noexcept {
    return m_Binding && m_Binding->active && !m_Context.expired();
}

ApplicationResult<ApplicationProviderRegistration>
ApplicationContext::RegisterWindowProvider(IApplicationWindowProvider& provider) {
    if (const auto error = CheckThread(m_State, "RegisterWindowProvider",
            "window_provider")) return Failure<ApplicationProviderRegistration>(*error);
    if (m_State->shuttingDown) {
        return Failure<ApplicationProviderRegistration>(MakeError(
            ApplicationErrorCode::ShuttingDown, "RegisterWindowProvider",
            "window_provider", "application is shutting down"));
    }
    if (const auto current = m_State->providerBinding.lock();
        current && current->active) {
        return Failure<ApplicationProviderRegistration>(MakeError(
            ApplicationErrorCode::AlreadyBound, "RegisterWindowProvider",
            "window_provider", "an application window provider is already registered",
            provider.BackendName() ? provider.BackendName() : "provider"));
    }
    if (!provider.SupportsStandard() && !provider.SupportsTransparent()) {
        return Failure<ApplicationProviderRegistration>(MakeError(
            ApplicationErrorCode::InvalidOptions, "RegisterWindowProvider",
            "window_provider", "provider must expose at least one window kind",
            provider.BackendName() ? provider.BackendName() : "provider"));
    }
    auto binding = std::make_shared<Detail::ApplicationProviderBinding>();
    binding->provider = &provider;
    if (provider.BackendName() != nullptr) binding->backend = provider.BackendName();
    m_State->providerBinding = binding;
    return ApplicationResult<ApplicationProviderRegistration>::Success(
        ApplicationProviderRegistration(m_State, std::move(binding)));
}

ApplicationResult<void> ApplicationContext::InitializeBackends() {
    if (const auto error = CheckThread(m_State, "InitializeBackends",
            "window")) return Failure<void>(*error);
    if (m_State->shuttingDown) {
        return Failure<void>(MakeError(ApplicationErrorCode::ShuttingDown,
            "InitializeBackends", "window", "application is shutting down"));
    }
    if (m_State->runtime != nullptr) return ApplicationResult<void>::Success();
    m_State->backendsRequested = true;
    InitializeBackendsState(m_State);
    if (m_State->runtime == nullptr) {
        return Failure<void>(MakeError(ApplicationErrorCode::Unsupported,
            "InitializeBackends", "standard_window",
            m_State->backendInitReason.empty()
                ? "window backends are unavailable" : m_State->backendInitReason,
            "xie_platform_glfw"));
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ApplicationContext::PumpDesktopEvents() {
    if (const auto error = CheckThread(m_State, "PumpDesktopEvents",
            "desktop")) return Failure<void>(*error);
    return m_Desktop.Pump();
}

ApplicationResult<void> ApplicationContext::PumpWindowEvents() {
    if (const auto error = CheckThread(m_State, "PumpWindowEvents",
            "window")) return Failure<void>(*error);
    if (m_State->shuttingDown) {
        return Failure<void>(MakeError(ApplicationErrorCode::ShuttingDown,
            "PumpWindowEvents", "window", "application is shutting down"));
    }
#ifdef _WIN32
    // This host owns the optional Win32 transparent-window backend. The
    // operation is intentionally separate from ProcessWindowEvents(): the
    // latter remains a synchronization-only bridge for callers that already
    // pumped their native host.
    if (m_State->win32Host) m_State->win32Host->PollEvents();
#endif
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ApplicationContext::ProcessWindowEvents() {
    if (const auto error = CheckThread(m_State, "ProcessWindowEvents",
            "window")) return Failure<void>(*error);
    if (m_State->shuttingDown) {
        return Failure<void>(MakeError(ApplicationErrorCode::ShuttingDown,
            "ProcessWindowEvents", "window",
            "application is shutting down"));
    }

    // The outer host owns the actual native poll.  XiE only synchronizes the
    // already-pumped state and applies the declared close policy.  Copying the
    // session list also makes Destroy close policy safe while iterating.
    const auto sessions = m_State->sessions;
    for (const auto& state : sessions) {
        if (!state || state->closed || !state->resources.windows) continue;
        SyncContext(state);
        WindowContext* context = ResolveContext(state);
        if (!context || !context->closeRequested || state->closeHandled) {
            continue;
        }
        const std::string backend = state->resources.backend;
        if (!HandleRuntimeClose(state)) {
            const auto error = MakeError(ApplicationErrorCode::BackendFailure,
                "ProcessWindowEvents", "window",
                "window close request could not be handled", backend);
            RecordError(state, error);
            return Failure<void>(error);
        }
    }
    RemoveClosedSessions(m_State);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ApplicationContext::Run(
    const ApplicationRunOptions& options) {
    if (const auto error = CheckThread(m_State, "Run", "application")) {
        return Failure<void>(*error);
    }
    if (m_State->shuttingDown) {
        return Failure<void>(MakeError(ApplicationErrorCode::ShuttingDown,
            "Run", "application", "application is shutting down"));
    }
    if (m_State->running) {
        return Failure<void>(MakeError(ApplicationErrorCode::Busy,
            "Run", "application", "application loop is already running"));
    }
    if (m_State->runtime == nullptr) {
        return Failure<void>(MakeError(ApplicationErrorCode::Unsupported,
            "Run", "standard_window", "application runtime is unavailable"));
    }

    m_State->running = true;
    ApplicationWindowCallbacks callbacks;
    callbacks.update = [this, &options](WindowContext& context, double delta) {
        const auto state = FindSession(m_State, context.window);
        if (!state) return;
        auto view = MakeSessionView(state);
        if (options.update) options.update(view, delta);
        (void)ProcessWindowFrame(view, delta);
    };
    callbacks.render = [this, &options](WindowContext& context) {
        const auto state = FindSession(m_State, context.window);
        if (!state) return;
        auto view = MakeSessionView(state);
        if (options.render) options.render(view);
        (void)RenderWindow(view);
    };
    callbacks.queryActivity = [this, &options](const WindowContext& context) {
        const auto state = FindSession(m_State, context.window);
        if (!state) return ApplicationWindowCallbacks::Activity{};
        auto view = MakeSessionView(state);
        WindowFrameActivity activity = ReadActivity(state);
        if (options.queryActivity) {
            const auto custom = options.queryActivity(view);
            activity.hasActiveAnimation |= custom.hasActiveAnimation;
            activity.hasPendingWork |= custom.hasPendingWork;
            activity.needsRender |= custom.needsRender;
        }
        return ApplicationWindowCallbacks::Activity{
            activity.hasActiveAnimation,
            activity.hasPendingWork || activity.needsRender};
    };
    callbacks.close = [this](WindowContext& context) {
        return HandleRuntimeClose(FindSession(m_State, context.window));
    };
    callbacks.pollExternal = [this, state = m_State] {
        (void)m_Desktop.Pump();
#ifdef _WIN32
        if (state->win32Host) state->win32Host->PollEvents();
#else
        (void)state;
#endif
    };
    callbacks.hasExternalWindows = [state = m_State] {
        return std::any_of(state->sessions.begin(), state->sessions.end(),
            [](const auto& value) {
                return value && !value->closed &&
                    value->kind == WindowKind::Transparent;
            });
    };
    callbacks.queryExternalActivity = [state = m_State] {
        WindowFrameActivity total;
        for (const auto& value : state->sessions) {
            if (!value || value->closed || value->kind != WindowKind::Transparent)
                continue;
            const auto current = ReadActivity(value);
            total.hasActiveAnimation |= current.hasActiveAnimation;
            total.hasPendingWork |= current.hasPendingWork;
            total.needsRender |= current.needsRender;
        }
        return ApplicationWindowCallbacks::Activity{
            total.hasActiveAnimation,
            total.hasPendingWork || total.needsRender};
    };
    callbacks.updateExternal = [this, &options](double delta) {
        const auto sessions = m_State->sessions;
        for (const auto& value : sessions) {
            if (!value || value->closed || value->kind != WindowKind::Transparent)
                continue;
            auto view = MakeSessionView(value);
            if (options.update) options.update(view, delta);
            (void)ProcessWindowFrame(view, delta);
            SyncContext(value);
            if (WindowContext* context = ResolveContext(value);
                context != nullptr && context->closeRequested &&
                !value->closeHandled) {
                if (value->closePolicy == WindowClosePolicy::Hide) {
                    (void)view.Hide();
                    value->closeHandled = true;
                } else if (value->closePolicy ==
                    WindowClosePolicy::RequestApplicationExit) {
                    (void)RequestExit();
                    (void)view.Close();
                } else {
                    (void)view.Close();
                }
            }
        }
    };
    callbacks.renderExternal = [this, &options] {
        const auto sessions = m_State->sessions;
        for (const auto& value : sessions) {
            if (!value || value->closed || value->kind != WindowKind::Transparent)
                continue;
            auto view = MakeSessionView(value);
            if (options.render) options.render(view);
            (void)RenderWindowInternal(view, true);
        }
    };

    try {
        m_State->runtime->Run(callbacks);
    } catch (const std::exception& exception) {
        m_State->running = false;
        return Failure<void>(MakeError(ApplicationErrorCode::UnknownFailure,
            "Run", "application", exception.what()));
    }
    m_State->running = false;
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ApplicationContext::Wait(double timeoutSeconds) {
    if (const auto error = CheckThread(m_State, "Wait", "application")) {
        return Failure<void>(*error);
    }
    if (m_State->shuttingDown) {
        return Failure<void>(MakeError(ApplicationErrorCode::ShuttingDown,
            "Wait", "application", "application is shutting down"));
    }
    if (!m_State->runtime) {
        return Failure<void>(MakeError(ApplicationErrorCode::Unsupported,
            "Wait", "application", "application runtime is unavailable"));
    }
    if (timeoutSeconds < 0.0) {
        m_State->runtime->Services().host.WaitForEvents();
    } else {
        m_State->runtime->Services().host.WaitForEventsTimeout(timeoutSeconds);
    }
    return m_Desktop.Pump();
}

ApplicationResult<void> ApplicationContext::Wake() {
    if (const auto error = CheckThread(m_State, "Wake", "application")) {
        return Failure<void>(*error);
    }
    if (m_State->shuttingDown) {
        return Failure<void>(MakeError(ApplicationErrorCode::ShuttingDown,
            "Wake", "application", "application is shutting down"));
    }
    if (!m_State->runtime) {
        return Failure<void>(MakeError(ApplicationErrorCode::Unsupported,
            "Wake", "application", "application runtime is unavailable"));
    }
    m_State->runtime->Services().host.WakeEventLoop();
#ifdef _WIN32
    if (m_State->win32Host) m_State->win32Host->WakeEventLoop();
#endif
    std::function<void()> externalWake;
    {
        std::lock_guard lock(m_State->externalWakeMutex);
        externalWake = m_State->externalWakeCallback;
    }
    if (externalWake) externalWake();
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ApplicationContext::SetExternalWakeCallback(
    std::function<void()> callback) {
    if (const auto error = CheckThread(m_State, "SetExternalWakeCallback",
            "application")) return Failure<void>(*error);
    if (m_State->shuttingDown) {
        return Failure<void>(MakeError(ApplicationErrorCode::ShuttingDown,
            "SetExternalWakeCallback", "application",
            "application is shutting down"));
    }
    {
        std::lock_guard lock(m_State->externalWakeMutex);
        m_State->externalWakeCallback = std::move(callback);
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ApplicationContext::RequestExit() {
    if (const auto error = CheckThread(m_State, "RequestExit", "application")) {
        return Failure<void>(*error);
    }
    if (m_State->shuttingDown) {
        return Failure<void>(MakeError(ApplicationErrorCode::ShuttingDown,
            "RequestExit", "application", "application is shutting down"));
    }
    if (!m_State->runtime) {
        return Failure<void>(MakeError(ApplicationErrorCode::Unsupported,
            "RequestExit", "application", "application runtime is unavailable"));
    }
    m_State->runtime->RequestExit();
#ifdef _WIN32
    if (m_State->win32Host) m_State->win32Host->RequestExit();
#endif
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ApplicationContext::Shutdown() {
    if (!m_State) return ApplicationResult<void>::Success();
    if (m_State->shuttingDown) return ApplicationResult<void>::Success();
    if (const auto error = CheckThread(m_State, "Shutdown", "application")) {
        return Failure<void>(*error);
    }
    const auto close = m_Windows ? m_Windows->CloseAll()
                                 : ApplicationResult<void>::Success();
    if (!close) return close;
    // Shutdown is also a valid direct Context API.  In that path the caller
    // may still hold the move-only registration token; invalidate the
    // borrowed binding before the context becomes unusable so a later token
    // destructor cannot observe a live provider through a dead context.
    if (const auto binding = m_State->providerBinding.lock()) {
        binding->active = false;
        m_State->providerBinding.reset();
    }
    {
        std::lock_guard lock(m_State->externalWakeMutex);
        m_State->externalWakeCallback = {};
    }
    m_State->shuttingDown = true;
    const auto images = m_State->imageServices
        ? m_State->imageServices->Shutdown()
        : ApplicationResult<void>::Success();
    if (!images) return images;
    const auto desktop = m_Desktop.Shutdown();
    if (!desktop) return desktop;
    if (m_State->ownsRuntime && m_State->runtime) {
        m_State->runtime->RequestExit();
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<WindowFrameResult> ApplicationContext::ProcessWindowFrame(
    WindowSession& session, double deltaSeconds) {
    if (const auto error = CheckThread(m_State, "ProcessWindowFrame",
            "window")) return Failure<WindowFrameResult>(*error);
    if (!session.m_State || session.m_State->owner.lock() != m_State) {
        return Failure<WindowFrameResult>(MakeError(
            ApplicationErrorCode::LifecycleViolation, "ProcessWindowFrame",
            "window", "window session belongs to another application"));
    }
    if (const auto error = CheckSession(session.m_State,
            "ProcessWindowFrame")) return Failure<WindowFrameResult>(*error);
    SyncContext(session.m_State);
    WindowContext* context = ResolveContext(session.m_State);
    if (!context) {
        return Failure<WindowFrameResult>(MakeError(
            ApplicationErrorCode::BackendFailure, "ProcessWindowFrame",
            "window", "window context is unavailable",
            session.m_State->resources.backend));
    }
    WindowFrameResult result;
    if (session.m_State->ui) {
        session.m_State->ui->SetVisible(context->visible);
        if (context->visible) {
            const auto update = session.m_State->ui->Update(deltaSeconds,
                {static_cast<float>(session.m_State->logicalSize.width),
                 static_cast<float>(session.m_State->logicalSize.height)},
                context->dpiScale);
            result.updated = true;
            session.m_State->dirty |= update.drawListRebuilt;
        }
    }
    result.activity = ReadActivity(session.m_State);
    context->dirty = result.activity.needsRender;
    context->framePolicy = !context->visible ? FramePolicy::Hidden
        : (result.activity.hasActiveAnimation || result.activity.hasPendingWork
            ? FramePolicy::Active : FramePolicy::Static);
    ClearError(session.m_State);
    return ApplicationResult<WindowFrameResult>::Success(result);
}

ApplicationResult<WindowFrameActivity> ApplicationContext::QueryWindowActivity(
    const WindowSession& session) const {
    if (const auto error = CheckThread(m_State, "QueryWindowActivity",
            "window")) return Failure<WindowFrameActivity>(*error);
    if (!session.m_State || session.m_State->owner.lock() != m_State) {
        return Failure<WindowFrameActivity>(MakeError(
            ApplicationErrorCode::LifecycleViolation, "QueryWindowActivity",
            "window", "window session belongs to another application"));
    }
    if (const auto error = CheckSession(session.m_State,
            "QueryWindowActivity")) return Failure<WindowFrameActivity>(*error);
    return ApplicationResult<WindowFrameActivity>::Success(
        ReadActivity(session.m_State));
}

ApplicationResult<Render2DStats> ApplicationContext::RenderWindowInternal(
    WindowSession& session, bool present) {
    if (!session.m_State || session.m_State->owner.lock() != m_State) {
        return Failure<Render2DStats>(MakeError(
            ApplicationErrorCode::LifecycleViolation, "RenderWindow",
            "window", "window session belongs to another application"));
    }
    if (const auto error = CheckSession(session.m_State, "RenderWindow")) {
        return Failure<Render2DStats>(*error);
    }
    WindowContext* context = ResolveContext(session.m_State);
    if (!context || !context->renderPipeline) {
        return Failure<Render2DStats>(MakeError(
            ApplicationErrorCode::Unsupported, "RenderWindow",
            "opengl_presentation", "render pipeline is unavailable",
            session.m_State->resources.backend));
    }
    SyncContext(session.m_State);
    const float dpi = std::max(0.01f, context->dpiScale);
    if (session.m_State->lastDpiScale != dpi ||
        session.m_State->lastLogicalWidth != session.m_State->logicalSize.width ||
        session.m_State->lastLogicalHeight != session.m_State->logicalSize.height) {
        if (context->surface) context->surface->MakeCurrent();
        if (session.m_State->openGlBackend) {
            session.m_State->openGlBackend->SetViewProjection(
                Projection(session.m_State->logicalSize.width,
                    session.m_State->logicalSize.height).data());
        }
        context->renderPipeline->Resize(session.m_State->logicalSize.width,
            session.m_State->logicalSize.height, dpi);
        session.m_State->lastDpiScale = dpi;
        session.m_State->lastLogicalWidth = session.m_State->logicalSize.width;
        session.m_State->lastLogicalHeight = session.m_State->logicalSize.height;
    }
    RenderSurfaceSize framebuffer = context->surface
        ? context->surface->GetFramebufferSize() : context->framebufferSize;
    if (framebuffer.width <= 0 || framebuffer.height <= 0) {
        framebuffer.width = static_cast<int>(std::lround(
            static_cast<float>(session.m_State->logicalSize.width) * dpi));
        framebuffer.height = static_cast<int>(std::lround(
            static_cast<float>(session.m_State->logicalSize.height) * dpi));
    }
    context->framebufferSize = framebuffer;
    static const DrawList2D emptyDrawList;
    const DrawList2D& drawList = session.m_State->ui
        ? session.m_State->ui->DrawList() : emptyDrawList;
    session.m_State->lastRenderStats = context->renderPipeline->Render(
        drawList, {static_cast<float>(framebuffer.width),
            static_cast<float>(framebuffer.height), dpi});
    if (present) context->renderPipeline->Present();
    session.m_State->dirty = false;
    context->dirty = false;
    ClearError(session.m_State);
    return ApplicationResult<Render2DStats>::Success(
        session.m_State->lastRenderStats);
}

ApplicationResult<Render2DStats> ApplicationContext::RenderWindow(
    WindowSession& session) {
    if (const auto error = CheckThread(m_State, "RenderWindow",
            "window")) return Failure<Render2DStats>(*error);
    return RenderWindowInternal(session, false);
}

ApplicationResult<Render2DStats> ApplicationContext::RenderWindow(
    WindowSession& session, bool present) {
    if (const auto error = CheckThread(m_State, "RenderWindow",
            "window")) return Failure<Render2DStats>(*error);
    return RenderWindowInternal(session, present);
}

bool ApplicationContext::IsRunning() const noexcept {
    return m_State != nullptr && m_State->running;
}

bool ApplicationContext::IsShuttingDown() const noexcept {
    return m_State == nullptr || m_State->shuttingDown;
}

} // namespace Engine
