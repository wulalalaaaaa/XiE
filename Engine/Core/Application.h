#pragma once

<<<<<<< Updated upstream
=======
#include "GameApp.h"
#include "Application/FrameScheduler.h"
#include "Application/WindowContext.h"
#include "Platform/IApplicationHost.h"
#include "Platform/IInputState.h"
#include "Platform/IWindowSystem.h"

#include <filesystem>
>>>>>>> Stashed changes
#include <memory>

namespace Engine {

    class Renderer;
    class Window;

    class Application {
        public:
            Application();
            ~Application();

            Application(const Application&) = delete;
            Application& operator=(const Application&) = delete;

<<<<<<< Updated upstream
            void Run();
=======
private:
    bool Init();
    void Shutdown();
    void RefreshWindowContext();
>>>>>>> Stashed changes

        private:
            bool Init();
            void Tick(float dt);
            void Shutdown();

<<<<<<< Updated upstream
        private:
            std::unique_ptr<Window> m_Window;
            std::unique_ptr<Renderer> m_Renderer;
            bool m_Running = false;
            float currentTime;
    };
=======
private:
    std::unique_ptr<IApplicationHost> m_Host;
    std::unique_ptr<IWindowSystem> m_WindowSystem;
    WindowContext m_WindowContext{};
    std::unique_ptr<IInputState> m_Input;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<IGameApp> m_GameApp;
    FrameContext m_FrameContext{};
    GameStartupDesc m_StartupDesc{};
    bool m_Running = false;
    FrameScheduler m_FrameScheduler;
    double m_CurrentTime = 0.0;
};
>>>>>>> Stashed changes

} // namespace Engine
