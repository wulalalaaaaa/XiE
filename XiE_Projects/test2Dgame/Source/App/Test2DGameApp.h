#pragma once

#include "Runtime2D/AssetRuntime2D.h"
#include "Runtime2D/CameraRuntime2D.h"
#include "Runtime2D/CollisionSystem2D.h"
#include "Core/GameApp.h"
#include "Runtime2D/InputSystem2D.h"
#include "Runtime2D/MovementSystem2D.h"
#include "Runtime2D/RenderSync2D.h"
#include "Runtime2D/SceneRuntime2D.h"
#include "Runtime2D/World2D.h"
#include "Story/LoopbackStoryTransport.h"
#include "Story/StoryFileCatalog.h"
#include "Story/StoryRuntime.h"

#include <cstdint>
#include <string>

class Test2DGameApp final : public Engine::IGameApp {
public:
    void OnInit(const Engine::FrameContext& context) override;
    void OnUpdate(float dt, const Engine::FrameContext& context) override;
    void OnRender(const Engine::FrameContext& context) override;
    void OnShutdown(const Engine::FrameContext& context) override;

private:
    void InitializeStory();
    void TickStoryInput(const Engine::FrameContext& context);
    void ConnectLateStoryReplica();
    void DrainStoryEvents(const char* runtimeName, Engine::Story::IStoryRuntime& runtime);
    Engine::Story::StoryResult SendStoryCommand(Engine::Story::StoryCommand command);

private:
    Test2D::AssetRuntime2D m_Assets{};
    Test2D::SceneRuntime2D m_SceneRuntime{};
    Test2D::World2D m_World{};
    Test2D::InputSystem2D m_InputSystem{};
    Test2D::MovementSystem2D m_MovementSystem{};
    Test2D::CollisionSystem2D m_CollisionSystem{};
    Test2D::CameraRuntime2D m_CameraRuntime{};
    Test2D::RenderSync2D m_RenderSync{};
    std::string m_LastSceneRuntimeWarning{};

    Engine::Story::StoryFileCatalog m_StoryCatalog{};
    Engine::Story::StoryRuntime m_StoryAuthority{
        m_StoryCatalog,
        Engine::Story::StoryRuntimeRole::Authority};
    Engine::Story::StoryRuntime m_StoryReplicaOne{
        m_StoryCatalog,
        Engine::Story::StoryRuntimeRole::Replica};
    Engine::Story::StoryRuntime m_StoryReplicaTwo{
        m_StoryCatalog,
        Engine::Story::StoryRuntimeRole::Replica};
    Engine::Story::LoopbackStoryTransport m_StoryTransport{};
    Engine::Story::StorySessionId m_DemoStorySessionId = 1;
    Engine::Story::StoryCommandId m_NextStoryCommandId = 100;
    bool m_SecondStoryReplicaConnected = false;
    bool m_StartStoryKeyWasDown = false;
    bool m_AdvanceStoryKeyWasDown = false;
    bool m_AbortStoryKeyWasDown = false;
};
