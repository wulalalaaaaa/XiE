#include "Application.h"

#include "Log.h"
#include "Platform/Window.h"
#include "Renderer/Renderer.h"

#include <exception>
#include <GLFW/glfw3.h>
#include <utility>

namespace Engine {

Application::Application(GameStartupDesc startupDesc, std::unique_ptr<IGameApp> gameApp)
    : m_GameApp(std::move(gameApp))
    , m_StartupDesc(startupDesc) {}

Application::~Application() {
    Shutdown();
}

std::filesystem::path Application::ResolveAssetRoot() const {
    if (m_StartupDesc.AssetRoot != nullptr && m_StartupDesc.AssetRoot[0] != '\0') {
        return std::filesystem::path(m_StartupDesc.AssetRoot);
    }
    return "Assets";
}

std::filesystem::path Application::ResolveMeshAssetPath() const {
    return ResolveAssetRoot() / "Mesh" / "main.xmesh";
}

bool Application::Init() {
    Log::Init();

    if (glfwInit() != GLFW_TRUE) {
        XLOG_ERROR("Failed to initialize GLFW");
        return false;
    }

    const int width = (m_StartupDesc.WindowWidth > 0) ? m_StartupDesc.WindowWidth : 1280;
    const int height = (m_StartupDesc.WindowHeight > 0) ? m_StartupDesc.WindowHeight : 720;
    const char* title = (m_StartupDesc.GameName != nullptr && m_StartupDesc.GameName[0] != '\0') ? m_StartupDesc.GameName : "XiE Game";

    m_Window = std::make_unique<Window>(width, height, title);
    m_Renderer = std::make_unique<Renderer>();

    if (!m_Renderer->Init(m_Window->GetNativeHandle(), ResolveMeshAssetPath())) {
        XLOG_ERROR("Renderer initialization failed");
        return false;
    }

    m_Running = true;
    currentTime = static_cast<float>(glfwGetTime());

    XLOG_INFO("Application initialized");
    return true;
}

void Application::TickPlatform(float dt) {
    (void)dt;
    m_Window->SwapBuffers();
    m_Window->PollEvents();
}

void Application::Run() {
    try {
        if (!Init()) {
            return;
        }

        if (m_GameApp) {
            m_GameApp->OnInit();
        }

        while (m_Running && !m_Window->ShouldClose()) {
            const float lastTime = static_cast<float>(glfwGetTime());
            const float deltaTime = lastTime - currentTime;
            currentTime = lastTime;

            m_Renderer->Tick(deltaTime);
            if (m_GameApp) {
                m_GameApp->OnUpdate(deltaTime);
            }
            m_Renderer->BeginFrame();
            if (m_GameApp) {
                m_GameApp->OnRender();
            }
            TickPlatform(deltaTime);

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

    if (m_GameApp) {
        m_GameApp->OnShutdown();
        m_GameApp.reset();
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
