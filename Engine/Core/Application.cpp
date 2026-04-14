#include "Application.h"

#include "Log.h"
#include "Platform/Window.h"
#include "Renderer/Renderer.h"

#include <exception>

#include <GLFW/glfw3.h>

namespace Engine {

Application::Application() = default;

Application::~Application() {
    Shutdown();
}

bool Application::Init() {
    Log::Init();

    if (glfwInit() != GLFW_TRUE) {
        Log::Error("Failed to initialize GLFW");
        return false;
    }

    m_Window = std::make_unique<Window>(1280, 720, "XiE Engine");
    m_Renderer = std::make_unique<Renderer>();
    m_Renderer->Init();

    m_Running = true;
    Log::Info("Application initialized");
    return true;
}

void Application::Run() {
    try {
        if (!Init()) {
            return;
        }

        while (m_Running && !m_Window->ShouldClose()) {
            m_Renderer->BeginFrame();

            m_Window->SwapBuffers();
            m_Window->PollEvents();

            m_Renderer->EndFrame();
        }
    } catch (const std::exception& e) {
        Log::Error(e.what());
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
    Log::Info("Application shutdown");
}

} // namespace Engine
