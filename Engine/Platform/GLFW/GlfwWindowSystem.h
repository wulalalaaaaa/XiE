#pragma once

#include "Platform/IWindowSystem.h"

#include <vector>

struct GLFWwindow;

namespace Engine {

class InputEventQueue;

class GlfwWindowSystem final : public IWindowSystem {
public:
    GlfwWindowSystem() = default;
    ~GlfwWindowSystem() override;

    GlfwWindowSystem(const GlfwWindowSystem&) = delete;
    GlfwWindowSystem& operator=(const GlfwWindowSystem&) = delete;

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

    GLFWwindow* GetGlfwWindow(WindowHandle window) const;

private:
    struct Slot {
        GLFWwindow* window = nullptr;
        std::uint32_t generation = 1;
        bool visible = false;
    };

    Slot* Resolve(WindowHandle window);
    const Slot* Resolve(WindowHandle window) const;

private:
    std::vector<Slot> m_Slots;
};

} // namespace Engine
