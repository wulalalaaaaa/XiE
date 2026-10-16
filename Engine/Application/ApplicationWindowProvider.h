#pragma once

#include "Application/ApplicationResult.h"
#include "Platform/WindowHandle.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace Engine {

struct StandardWindowOptions;
struct TransparentWindowOptions;
class ApplicationRuntime;
class ITextLayoutService;
class IWindowSystem;
class IUIInputRouter;
class InputEventQueue;
class TextureRegistry2D;
class ApplicationContext;
struct WindowContext;

namespace Detail {
struct ApplicationCompositionState;
struct ApplicationProviderBinding;
}

// A provider is borrowed by ApplicationContext.  This move-only token is the
// explicit lifetime boundary: the provider must outlive the token, and the
// token should be unregistered after all sessions created from it are closed.
// The token also keeps the binding object alive, so the context never retains
// a dangling provider pointer if a caller drops the token prematurely.
class ApplicationProviderRegistration final {
public:
    ApplicationProviderRegistration() = default;
    ~ApplicationProviderRegistration() noexcept;

    ApplicationProviderRegistration(const ApplicationProviderRegistration&) = delete;
    ApplicationProviderRegistration& operator=(
        const ApplicationProviderRegistration&) = delete;
    ApplicationProviderRegistration(ApplicationProviderRegistration&& other) noexcept;
    ApplicationProviderRegistration& operator=(
        ApplicationProviderRegistration&& other) noexcept;

    ApplicationResult<void> Unregister();
    [[nodiscard]] bool IsRegistered() const noexcept;

private:
    friend class ApplicationContext;
    ApplicationProviderRegistration(
        std::weak_ptr<Detail::ApplicationCompositionState> context,
        std::shared_ptr<Detail::ApplicationProviderBinding> binding);

    std::weak_ptr<Detail::ApplicationCompositionState> m_Context;
    std::shared_ptr<Detail::ApplicationProviderBinding> m_Binding;
};

// Optional bridge for an application that already owns an XiE runtime.  It
// is intentionally made of XiE contracts and callbacks only, so a provider
// can hide a legacy host without making native window types public.
struct ApplicationWindowResources {
    WindowHandle window{};
    IWindowSystem* windows = nullptr;
    ApplicationRuntime* runtime = nullptr;
    WindowContext* context = nullptr;
    std::string backend;
    std::function<bool(InputEventQueue*)> attachInput;
    std::function<void(TextureRegistry2D*, ITextLayoutService*)>
        bindRenderResources;
    std::function<void()> close;

    [[nodiscard]] bool IsValid() const noexcept {
        return window.IsValid() && windows != nullptr && context != nullptr;
    }
};

class IApplicationWindowProvider {
public:
    virtual ~IApplicationWindowProvider() = default;

    [[nodiscard]] virtual bool SupportsStandard() const noexcept = 0;
    [[nodiscard]] virtual bool SupportsTransparent() const noexcept {
        return false;
    }
    [[nodiscard]] virtual const char* BackendName() const noexcept = 0;

    virtual ApplicationResult<ApplicationWindowResources> CreateStandard(
        const StandardWindowOptions& options) = 0;
    virtual ApplicationResult<ApplicationWindowResources> CreateTransparent(
        const TransparentWindowOptions& options) {
        (void)options;
        return ApplicationResult<ApplicationWindowResources>::Failure({
            ApplicationErrorCode::Unsupported,
            "CreateTransparentWindow",
            "transparent_window",
            "window provider does not expose a transparent window",
            std::nullopt,
            BackendName() ? BackendName() : "provider"});
    }
};

} // namespace Engine
