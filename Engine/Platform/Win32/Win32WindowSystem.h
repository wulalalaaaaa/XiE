#pragma once

#include "Platform/IWindowSystem.h"

#include <memory>
#include <vector>

namespace Engine {

class InputEventQueue;

class Win32WindowSystem final : public IWindowSystem {
public:
    struct Slot;

    Win32WindowSystem();
    ~Win32WindowSystem() override;

    Win32WindowSystem(const Win32WindowSystem&) = delete;
    Win32WindowSystem& operator=(const Win32WindowSystem&) = delete;

    WindowHandle CreateWindow(const WindowDesc& desc) override;
    void DestroyWindow(WindowHandle window) override;

    void ShowWindow(WindowHandle window) override;
    void HideWindow(WindowHandle window) override;

    void SetWindowPosition(WindowHandle window, int x, int y) override;
    void SetWindowSize(WindowHandle window, int width, int height) override;

    bool IsVisible(WindowHandle window) const override;
    bool IsCloseRequested(WindowHandle window) const override;

    float GetDpiScale(WindowHandle window) const override;
    NativeWindowHandle GetNativeHandle(WindowHandle window) const override;

    // Minimal event-stream adapter for UI2D composition. The non-owning queue
    // must outlive the attachment and must belong to the same WindowHandle.
    bool SetInputEventQueue(WindowHandle window, InputEventQueue* queue);

private:
    Slot* Resolve(WindowHandle window);
    const Slot* Resolve(WindowHandle window) const;
    void EnsureWindowClass();

private:
    std::vector<std::unique_ptr<Slot>> m_Slots;
    bool m_ClassRegistered = false;
};

} // namespace Engine
