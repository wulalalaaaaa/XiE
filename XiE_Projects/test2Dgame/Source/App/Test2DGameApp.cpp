#include "Test2DGameApp.h"

#include "Core/Log.h"
#include "Runtime2D/SceneLoader2D.h"

#include <string>

namespace {

void SpawnFallbackScene(Test2D::World2D& world) {
    world.Clear();

    Test2D::Entity2D player{};
    player.transform.position = {200.0f, 200.0f};
    player.collider.halfExtent = {24.0f, 24.0f};
    player.collider.isStatic = false;
    player.sprite.spritePath = "Assets/Sprite/main.xsprite";
    const Test2D::EntityId playerId = world.CreateEntity(player);
    world.SetPlayer(playerId);

    Test2D::Entity2D obstacle{};
    obstacle.transform.position = {480.0f, 260.0f};
    obstacle.collider.halfExtent = {64.0f, 64.0f};
    obstacle.collider.isStatic = true;
    obstacle.sprite.spritePath = "Assets/Sprite/main.xsprite";
    (void)world.CreateEntity(obstacle);
}

} // namespace

void Test2DGameApp::OnInit(const Engine::FrameContext& context) {
    (void)context;
    m_Assets.SetAssetRoot("Assets");
    (void)m_Assets.Initialize();

    m_SceneRuntime.SetScenePath("Assets/Scene/main.xscene2d");
    if (!m_SceneRuntime.Initialize(m_World)) {
        const std::string message = "Scene load failed, using fallback spawn: " + m_SceneRuntime.LastError();
        XLOG_WARN(message.c_str());
        SpawnFallbackScene(m_World);
    }

    (void)m_InputSystem.LoadConfig("Config/Input.toml");

    XLOG_INFO("Test2DGameApp initialized");
}

void Test2DGameApp::OnUpdate(float dt, const Engine::FrameContext& context) {
    m_Assets.TickHotReload();

    if (m_SceneRuntime.TickHotReload(m_World)) {
        XLOG_INFO("Scene hot-reloaded: Assets/Scene/main.xscene2d");
        m_LastSceneRuntimeWarning.clear();
    } else if (!m_SceneRuntime.LastError().empty() && m_SceneRuntime.LastError() != m_LastSceneRuntimeWarning) {
        m_LastSceneRuntimeWarning = m_SceneRuntime.LastError();
        XLOG_WARN(m_LastSceneRuntimeWarning.c_str());
    }
    m_Assets.EnsureSpriteRefs(m_World);

    m_InputSystem.Tick(context.WindowHandle, m_World);
    m_MovementSystem.Integrate(m_World, dt);
    m_CollisionSystem.Solve(m_World);
    m_RenderSync.Sync(m_World, m_Assets.GetSnapshot(), context.RuntimeRender2D);
}

void Test2DGameApp::OnRender(const Engine::FrameContext& context) {
    (void)context;
}

void Test2DGameApp::OnShutdown(const Engine::FrameContext& context) {
    if (context.RuntimeRender2D != nullptr) {
        context.RuntimeRender2D->ClearRuntimeMesh2D();
    }
    XLOG_INFO("Test2DGameApp shutdown");
}
