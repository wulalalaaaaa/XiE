#include "Application.h"

#include "Log.h"
#include "Platform/GLFW/GlfwApplicationHost.h"
#include "Platform/GLFW/GlfwInputState.h"
#include "Platform/GLFW/GlfwOpenGLSurface.h"
#include "Platform/GLFW/GlfwWindowSystem.h"
#include "Platform/WindowDesc.h"
#include "Renderer/Renderer.h"

#include <chrono>
#include <exception>
<<<<<<< Updated upstream
// #include <chrono>
#include <GLFW/glfw3.h>
=======
#include <utility>
>>>>>>> Stashed changes

namespace Engine 
{

<<<<<<< Updated upstream
    Application::Application() = default;

    Application::~Application() 
    {
        Shutdown();
=======
namespace {

double NowSeconds() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

} // namespace

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

std::filesystem::path Application::ResolveTextureAssetPath() const {
    return ResolveAssetRoot() / "Texture" / "main.xtexture";
}

std::filesystem::path Application::ResolveMaterialAssetPath() const {
    return ResolveAssetRoot() / "Material" / "main.xmat";
}

std::filesystem::path Application::ResolveSpriteAssetPath() const {
    return ResolveAssetRoot() / "Sprite" / "main.xsprite";
}

bool Application::Init() {
    Log::Init();

    auto host = std::make_unique<GlfwApplicationHost>();
    if (!host->IsInitialized()) {
        return false;
>>>>>>> Stashed changes
    }
    m_Host = std::move(host);
    auto glfwWindowSystem = std::make_unique<GlfwWindowSystem>();
    GlfwWindowSystem* glfwWindows = glfwWindowSystem.get();
    m_WindowSystem = std::move(glfwWindowSystem);

    bool Application::Init() 
    {
        Log::Init();

<<<<<<< Updated upstream
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
=======
    WindowDesc windowDesc{};
    windowDesc.title = title;
    windowDesc.width = width;
    windowDesc.height = height;
    windowDesc.visible = true;
    windowDesc.resizable = true;
    windowDesc.decorated = true;

    m_WindowContext.window = m_WindowSystem->CreateWindow(windowDesc);
    m_WindowContext.surface = std::make_unique<GlfwOpenGLSurface>(*glfwWindows, m_WindowContext.window);
    m_WindowContext.framePolicy = FramePolicy::Active;
    RefreshWindowContext();

    m_Input = std::make_unique<GlfwInputState>(*glfwWindows, m_WindowContext.window);
    m_FrameContext.Window = m_WindowContext.window;
    m_FrameContext.Input = m_Input.get();
    m_Renderer = std::make_unique<Renderer>();

    if (!m_Renderer->Init(
            *m_WindowContext.surface,
            ResolveMeshAssetPath(),
            ResolveTextureAssetPath(),
            ResolveMaterialAssetPath(),
            ResolveSpriteAssetPath(),
            m_StartupDesc.UVMode)) {
        XLOG_ERROR("Renderer initialization failed");
        return false;
>>>>>>> Stashed changes
    }

<<<<<<< Updated upstream
    void Application::Tick(float dt)
    {
        m_Window->SwapBuffers();
        m_Window->PollEvents();
=======
    m_Running = true;
    m_CurrentTime = NowSeconds();
>>>>>>> Stashed changes

    }

<<<<<<< Updated upstream
    void Application::Run() 
    {
        try {
            if (!Init()) {
                return;
            }
=======
void Application::RefreshWindowContext() {
    if (!m_WindowSystem || !m_WindowContext.window.IsValid()) {
        return;
    }
    m_WindowContext.visible = m_WindowSystem->IsVisible(m_WindowContext.window);
    m_WindowContext.closeRequested = m_WindowSystem->IsCloseRequested(m_WindowContext.window);
    m_WindowContext.dpiScale = m_WindowSystem->GetDpiScale(m_WindowContext.window);
}
>>>>>>> Stashed changes

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

<<<<<<< Updated upstream
        m_Window.reset();
        glfwTerminate();

        m_Running = false;
        XLOG_INFO("Application shutdown");
    }
=======
        while (m_Running && m_Host && !m_Host->ShouldExit()) {
            m_Host->PollEvents();
            RefreshWindowContext();
            if (m_WindowContext.closeRequested) {
                m_Host->RequestExit();
                break;
            }

            if (m_Input && m_Input->IsKeyDown(KeyCode::Escape)) {
                m_Host->RequestExit();
                break;
            }

            const double now = NowSeconds();
            const FrameDecision decision = m_FrameScheduler.Evaluate(
                m_WindowContext,
                false,
                false,
                now
            );

            if (!decision.update && !decision.render) {
                if (decision.waitSeconds > 0.0) {
                    m_Host->WaitForEventsTimeout(decision.waitSeconds);
                } else {
                    m_Host->WaitForEvents();
                }
                continue;
            }

            const float deltaTime = static_cast<float>(now - m_CurrentTime);
            m_CurrentTime = now;

            if (decision.update) {
                m_Renderer->Tick(deltaTime);
            }
            if (m_GameApp) {
                m_GameApp->OnUpdate(deltaTime, m_FrameContext);
            }

            if (decision.render) {
                m_Renderer->BeginFrame();
                if (m_GameApp) {
                    m_GameApp->OnRender(m_FrameContext);
                }
                if (m_WindowContext.surface) {
                    m_WindowContext.surface->Present();
                }
                m_Renderer->EndFrame();
                m_WindowContext.dirty = false;
            }
        }
    } catch (const std::exception& e) {
        XLOG_ERROR(e.what());
    }
}

void Application::Shutdown() {
    if (!m_Running && !m_Renderer && !m_WindowContext.window.IsValid()) {
        return;
    }

    if (m_GameApp) {
        m_GameApp->OnShutdown(m_FrameContext);
        m_GameApp.reset();
    }

    if (m_Renderer) {
        m_Renderer->Shutdown();
        m_Renderer.reset();
    }
    m_FrameContext.RuntimeRender2D = nullptr;

    m_WindowContext.surface.reset();
    if (m_WindowSystem && m_WindowContext.window.IsValid()) {
        m_WindowSystem->DestroyWindow(m_WindowContext.window);
    }
    m_WindowContext = {};
    m_WindowSystem.reset();
    m_Input.reset();
    m_FrameContext.Window = {};
    m_FrameContext.Input = nullptr;
    m_Host.reset();

    m_Running = false;
    XLOG_INFO("Application shutdown");
}
>>>>>>> Stashed changes

} // namespace Engine
