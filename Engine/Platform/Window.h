#pragma once

struct GLFWwindow;

namespace Engine {

class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ShouldClose() const;
    void PollEvents() const;
    void SwapBuffers() const;

    GLFWwindow* GetNativeHandle() const;

private:
    GLFWwindow* m_Handle = nullptr;
};

} // namespace Engine
