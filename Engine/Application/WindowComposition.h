#pragma once

#include "Application/ApplicationResult.h"
#include "Application/ApplicationUIBinding.h"
#include "Application/WindowSession.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

namespace Engine {

struct ApplicationWindowResources;
class IApplicationWindowProvider;

struct WindowSize {
    int width = 1280;
    int height = 720;
};

struct WindowPlacement {
    std::optional<int> x;
    std::optional<int> y;
    bool maximized = false;
};

enum class WindowFocusPolicy {
    Preserve,
    Activate,
    NoActivate,
};

enum class WindowClosePolicy {
    Hide,
    RequestApplicationExit,
    Destroy,
};

enum class WindowDpiPolicy {
    PerMonitor,
    System,
    Logical,
};

struct StandardWindowOptions {
    std::string title = "XiE Application";
    WindowSize size{};
    bool visible = true;
    bool resizable = true;
    bool decorated = true;
    bool alwaysOnTop = false;
    bool toolWindow = false;
    WindowPlacement placement{};
    WindowFocusPolicy focusPolicy = WindowFocusPolicy::Activate;
    WindowClosePolicy closePolicy = WindowClosePolicy::Hide;
    WindowDpiPolicy dpiPolicy = WindowDpiPolicy::PerMonitor;
};

struct TransparentWindowOptions {
    std::string title = "XiE Transparent Overlay";
    WindowSize size{640, 480};
    bool visible = true;
    bool transparent = true;
    bool alwaysOnTop = true;
    bool toolWindow = true;
    WindowFocusPolicy focusPolicy = WindowFocusPolicy::NoActivate;
    bool clickThrough = false;
    float opacity = 1.0f;
    WindowPlacement placement{};
    WindowClosePolicy closePolicy = WindowClosePolicy::Hide;
    WindowDpiPolicy dpiPolicy = WindowDpiPolicy::PerMonitor;
};

namespace Detail { struct ApplicationCompositionState; }

class WindowComposition final {
public:
    ~WindowComposition() noexcept;
    WindowComposition(const WindowComposition&) = delete;
    WindowComposition& operator=(const WindowComposition&) = delete;
    WindowComposition(WindowComposition&&) noexcept;
    WindowComposition& operator=(WindowComposition&&) noexcept;

    static ApplicationResult<void> ValidateStandardOptions(
        const StandardWindowOptions& options);
    static ApplicationResult<void> ValidateTransparentOptions(
        const TransparentWindowOptions& options);

    ApplicationResult<WindowSession> CreateStandard(
        const StandardWindowOptions& options = {});
    ApplicationResult<WindowSession> CreateTransparent(
        const TransparentWindowOptions& options = {});

    ApplicationResult<void> CloseAll();
    [[nodiscard]] std::size_t Size() const noexcept;

private:
    explicit WindowComposition(
        std::shared_ptr<Detail::ApplicationCompositionState> state);
    ApplicationResult<WindowSession> CreateSession(
        WindowKind kind, const StandardWindowOptions* standard,
        const TransparentWindowOptions* transparent);
    friend class ApplicationContext;

    std::shared_ptr<Detail::ApplicationCompositionState> m_State;
};

} // namespace Engine
