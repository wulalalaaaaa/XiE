#include "Application.h"

#include "Log.h"
#include "Platform/Window.h"
#include "Renderer/Renderer.h"

#include <exception>
// #include <chrono>
#include <GLFW/glfw3.h>

namespace Engine 
{

    Application::Application() = default;

    Application::~Application() 
    {
        Shutdown();
    }

    bool Application::Init() 
    {
        Log::Init();

        if (glfwInit() != GLFW_TRUE) {
            XLOG_ERROR("Failed to initialize GLFW");
            return false;
        }

        m_Window = std::make_unique<Window>(1280, 720, "XiE Engine");
        m_Renderer = std::make_unique<Renderer>();

        if (!m_Renderer->Init(m_Window->GetNativeHandle())) {
            XLOG_ERROR("Renderer initialization failed");
            return false;
        }

        m_Running = true;

        currentTime = glfwGetTime();

        XLOG_INFO("Application initialized");
        return true;
    }

    void Application::Tick(float dt)
    {
        m_Window->SwapBuffers();
        m_Window->PollEvents();

    }

    void Application::Run() 
    {
        try {
            if (!Init()) {
                return;
            }

            while (m_Running && !m_Window->ShouldClose()) {
                m_Renderer->BeginFrame();
               
                float lastTime = glfwGetTime();
                float DeltaTime = lastTime - this->currentTime;
                this->currentTime = lastTime;
                Tick(DeltaTime);

                m_Renderer->EndFrame();
            }
        } catch (const std::exception& e) {
            XLOG_ERROR(e.what());
        }
    }

    void Application::Shutdown() {
        if (!m_Running && !m_Renderer && !m_Window) {
            return;
        }

        if (m_Renderer) {
            m_Renderer->Shutdown();
            m_Renderer.reset();
        }

        m_Window.reset();
        glfwTerminate();

        m_Running = false;
        XLOG_INFO("Application shutdown");
    }

} // namespace Engine
