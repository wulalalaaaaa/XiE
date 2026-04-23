#pragma once

#include "GameApp.h"

#include <filesystem>
#include <memory>

namespace Engine {
    template <bool Debug> class BasicRenderer;
    using Renderer = BasicRenderer<(XIE_DEBUG != 0)>;
class Window;

class Application {
public:
    Application(GameStartupDesc startupDesc, std::unique_ptr<IGameApp> gameApp);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void Run();

private:
    bool Init();
    void TickPlatform(float dt);
    void Shutdown();

private:
    std::filesystem::path ResolveMeshAssetPath() const;
    std::filesystem::path ResolveAssetRoot() const;

private:
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<IGameApp> m_GameApp;
    GameStartupDesc m_StartupDesc{};
    bool m_Running = false;
    float currentTime = 0.0f;
};

} // namespace Engine
