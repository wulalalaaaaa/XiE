#pragma once

#include <memory>

namespace Engine {
    template <bool Debug> class BasicRenderer;
    using Renderer = BasicRenderer<(XIE_DEBUG != 0)>;
class Window;

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Run();

private:
    bool Init();
    void Tick(float dt);
    void Shutdown();

private:
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    bool m_Running = false;
    float currentTime = 0.0f;
};

} // namespace Engine
