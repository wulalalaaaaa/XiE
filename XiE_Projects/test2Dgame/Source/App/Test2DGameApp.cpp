#include "Test2DGameApp.h"

#include "Core/Log.h"

void Test2DGameApp::OnInit(const Engine::FrameContext& context) {
    (void)context;
    m_Assets.SetAssetRoot("Assets");
    (void)m_Assets.Initialize();

    Test2D::Entity2D player{};
    player.transform.position = {200.0f, 200.0f};
    player.collider.halfExtent = {24.0f, 24.0f};
    player.collider.isStatic = false;
    player.sprite.spritePath = "Assets/Sprite/main.xsprite";
    const Test2D::EntityId playerId = m_World.CreateEntity(player);
    m_World.SetPlayer(playerId);

    Test2D::Entity2D obstacle{};
    obstacle.transform.position = {480.0f, 260.0f};
    obstacle.collider.halfExtent = {64.0f, 64.0f};
    obstacle.collider.isStatic = true;
    obstacle.sprite.spritePath = "Assets/Sprite/main.xsprite";
    (void)m_World.CreateEntity(obstacle);

    m_InputSystem.SetMoveSpeed(260.0f);

    XLOG_INFO("Test2DGameApp initialized");
}

void Test2DGameApp::OnUpdate(float dt, const Engine::FrameContext& context) {
    m_Assets.TickHotReload();
    m_InputSystem.Tick(context.WindowHandle, m_World);

    for (Test2D::Entity2D& entity : m_World.Entities()) {
        if (entity.collider.isStatic) {
            continue;
        }

        entity.transform.position.x += entity.velocity.linear.x * dt;
        entity.transform.position.y += entity.velocity.linear.y * dt;
    }

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
