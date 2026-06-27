#pragma once

#include "Runtime2D/AssetRuntime2D.h"
#include "Runtime2D/CollisionSystem2D.h"
#include "Core/GameApp.h"
#include "Runtime2D/InputSystem2D.h"
#include "Runtime2D/MovementSystem2D.h"
#include "Runtime2D/RenderSync2D.h"
#include "Runtime2D/SceneRuntime2D.h"
#include "Runtime2D/World2D.h"

#include <string>

class Test2DGameApp final : public Engine::IGameApp {
public:
    void OnInit(const Engine::FrameContext& context) override;
    void OnUpdate(float dt, const Engine::FrameContext& context) override;
    void OnRender(const Engine::FrameContext& context) override;
    void OnShutdown(const Engine::FrameContext& context) override;

private:
    Test2D::AssetRuntime2D m_Assets{};
    Test2D::SceneRuntime2D m_SceneRuntime{};
    Test2D::World2D m_World{};
    Test2D::InputSystem2D m_InputSystem{};
    Test2D::MovementSystem2D m_MovementSystem{};
    Test2D::CollisionSystem2D m_CollisionSystem{};
    Test2D::RenderSync2D m_RenderSync{};
    std::string m_LastSceneRuntimeWarning{};
};
