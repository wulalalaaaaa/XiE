#pragma once

#include "Application/ApplicationResult.h"
#include "Application/ApplicationUIBinding.h"
#include "Platform/IRenderSurface.h"
#include "Platform/WindowHandle.h"
#include "Renderer2D/DrawTypes2D.h"

#include <memory>
#include <string>

namespace Engine {

class InputEventQueue;
class ITextClipboard;
class ITextInputMethod;
class ITextLayoutService;
class TextureRegistry2D;
class WindowComposition;

namespace UI2D { class UIWindowRuntime; }

enum class WindowKind {
    Standard,
    Transparent,
};

enum class WindowSessionStatus {
    Created,
    Visible,
    Hidden,
    CloseRequested,
    Closed,
    Failed,
};

struct WindowStateSnapshot {
    WindowSessionStatus status = WindowSessionStatus::Closed;
    bool visible = false;
    bool minimized = false;
    bool focused = false;
    bool closeRequested = false;
    float dpiScale = 1.0f;
    RenderSurfaceSize framebufferSize{};
};

// A portable snapshot used by application policy such as persisted window
// placement. Coordinates follow the window-system screen coordinate contract;
// width and height are logical client dimensions.
struct WindowPlacementSnapshot {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool maximized = false;
};

struct WindowFrameActivity {
    bool hasActiveAnimation = false;
    bool hasPendingWork = false;
    bool needsRender = false;
};

struct WindowFrameResult {
    WindowFrameActivity activity{};
    bool updated = false;
    bool rendered = false;
    Render2DStats renderStats{};
};

namespace Detail { struct WindowSessionState; }

class WindowSession final {
public:
    WindowSession() = default;
    ~WindowSession() noexcept;

    WindowSession(const WindowSession&) = delete;
    WindowSession& operator=(const WindowSession&) = delete;
    WindowSession(WindowSession&& other) noexcept;
    WindowSession& operator=(WindowSession&& other) noexcept;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool IsClosed() const noexcept;
    [[nodiscard]] WindowKind Kind() const noexcept;
    [[nodiscard]] WindowHandle Handle() const noexcept;
    [[nodiscard]] WindowSessionStatus Status() const noexcept;
    [[nodiscard]] WindowStateSnapshot QueryState() const;
    ApplicationResult<WindowPlacementSnapshot> QueryPlacement() const;

    ApplicationResult<void> Show();
    ApplicationResult<void> Hide();
    ApplicationResult<void> Activate();
    ApplicationResult<void> SetPosition(int x, int y);
    ApplicationResult<void> SetSize(int width, int height);
    ApplicationResult<float> QueryDpi() const;
    ApplicationResult<bool> QueryFocus() const;
    ApplicationResult<void> RequestClose();
    ApplicationResult<void> Close();

    ApplicationResult<UI2D::UIWindowRuntime*> BindUI(
        const UI2DBindingOptions& options = {});
    [[nodiscard]] UI2D::UIWindowRuntime* UI() noexcept;
    [[nodiscard]] const UI2D::UIWindowRuntime* UI() const noexcept;
    [[nodiscard]] InputEventQueue* InputQueue() noexcept;
    [[nodiscard]] TextureRegistry2D* Textures() noexcept;
    [[nodiscard]] ITextLayoutService* TextLayouts() noexcept;
    [[nodiscard]] ITextClipboard* TextClipboard() noexcept;
    [[nodiscard]] ITextInputMethod* TextInputMethod() noexcept;
    [[nodiscard]] const std::string& BackendName() const noexcept;
    [[nodiscard]] const std::string& LastError() const noexcept;

private:
    explicit WindowSession(
        std::shared_ptr<Detail::WindowSessionState> state,
        bool closeOnDestruction = true);
    friend class WindowComposition;
    friend class ApplicationContext;

    std::shared_ptr<Detail::WindowSessionState> m_State;
    bool m_CloseOnDestruction = true;
};

} // namespace Engine
