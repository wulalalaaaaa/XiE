#include "Test2DGameApp.h"

#include "Core/Log.h"
#include "Platform/IInputState.h"
#include "Runtime2D/SceneLoader2D.h"
#include "Story/StorySelfTest.h"

#include <sstream>
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

bool KeyPressedOnce(const Engine::IInputState* input, const Engine::KeyCode key, bool& wasDown) {
    const bool isDown = input != nullptr && input->IsKeyDown(key);
    const bool pressed = isDown && !wasDown;
    wasDown = isDown;
    return pressed;
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
    InitializeStory();

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

    m_InputSystem.Tick(context.Input, m_World);
    TickStoryInput(context);
    m_MovementSystem.Integrate(m_World, dt);
    m_CollisionSystem.Solve(m_World);
    m_CameraRuntime.Sync(m_World, context.RuntimeRender2D);
    m_RenderSync.Sync(m_World, m_Assets.GetSnapshot(), context.RuntimeRender2D);

    DrainStoryEvents("Authority", m_StoryAuthority);
    DrainStoryEvents("ReplicaOne", m_StoryReplicaOne);
    DrainStoryEvents("ReplicaTwo", m_StoryReplicaTwo);
}

void Test2DGameApp::OnRender(const Engine::FrameContext& context) {
    (void)context;
}

void Test2DGameApp::OnShutdown(const Engine::FrameContext& context) {
    m_StoryTransport.DisconnectReplica(101);
    m_StoryTransport.DisconnectReplica(202);
    m_StoryTransport.SetAuthorityReceiver({});
    if (context.RuntimeRender2D != nullptr) {
        context.RuntimeRender2D->ClearRuntimeMesh2D();
    }
    XLOG_INFO("Test2DGameApp shutdown");
}

void Test2DGameApp::InitializeStory() {
    using namespace Engine::Story;

    if (!m_StoryCatalog.Load("Assets/Story")) {
        const std::string message =
            "Story catalog load failed: " + m_StoryCatalog.LastError();
        XLOG_ERROR(message);
        return;
    }

    {
        std::ostringstream message;
        message << "Story catalog loaded: stories=" << m_StoryCatalog.StoryKeys().size()
                << " contentVersion=" << m_StoryCatalog.ContentVersion();
        XLOG_INFO(message.str());
    }

    const Test2D::StorySelfTestResult selfTest =
        Test2D::RunStorySelfTest(m_StoryCatalog);
    {
        std::ostringstream message;
        message << "Story self-test " << (selfTest.passed ? "PASSED" : "FAILED")
                << ": checks=" << selfTest.checksPassed
                << " message=" << selfTest.message;
        if (selfTest.passed) {
            XLOG_INFO(message.str());
        } else {
            XLOG_ERROR(message.str());
        }
    }

    m_StoryTransport.SetAuthorityReceiver(
        [this](const StoryCommand& command, const StoryCommandContext& context) {
            StoryResult result = m_StoryAuthority.Apply(command, context);
            if (result.Succeeded()) {
                if (const StorySessionState* state =
                        m_StoryAuthority.FindSession(command.sessionId)) {
                    m_StoryTransport.PublishState(*state);
                }
            }
            return result;
        });

    (void)m_StoryTransport.ConnectReplica(
        101,
        [this](const StorySessionState& state) {
            return m_StoryReplicaOne.Synchronize(state);
        });

    XLOG_INFO("Story controls: F5=Start, F6=Advance, F7=Abort");
}

void Test2DGameApp::TickStoryInput(const Engine::FrameContext& context) {
    using namespace Engine::Story;

    if (KeyPressedOnce(context.Input, Engine::KeyCode::F5, m_StartStoryKeyWasDown)) {
        const std::vector<StoryKey> keys = m_StoryCatalog.StoryKeys();
        if (keys.empty()) {
            XLOG_ERROR("Story Start ignored: catalog is empty");
        } else {
            StoryStartRequest request{};
            request.sessionId = m_DemoStorySessionId;
            request.story = keys.front();
            request.contentVersion = m_StoryCatalog.ContentVersion();
            const StoryResult result = SendStoryCommand(
                StoryCommand::Start(m_NextStoryCommandId++, request));
            if (result.Succeeded()) {
                ConnectLateStoryReplica();
            }
        }
    }

    if (KeyPressedOnce(context.Input, Engine::KeyCode::F6, m_AdvanceStoryKeyWasDown)) {
        const StorySessionState* state =
            m_StoryAuthority.FindSession(m_DemoStorySessionId);
        if (state == nullptr) {
            XLOG_WARN("Story Advance ignored: press F5 to start first");
        } else {
            StoryCommand command{};
            command.commandId = m_NextStoryCommandId++;
            command.type = StoryCommandType::Advance;
            command.sessionId = state->sessionId;
            command.expectedRevision = state->revision;
            command.contentVersion = m_StoryCatalog.ContentVersion();
            (void)SendStoryCommand(std::move(command));
        }
    }

    if (KeyPressedOnce(context.Input, Engine::KeyCode::F7, m_AbortStoryKeyWasDown)) {
        const StorySessionState* state =
            m_StoryAuthority.FindSession(m_DemoStorySessionId);
        if (state == nullptr) {
            XLOG_WARN("Story Abort ignored: no active demo session");
        } else {
            StoryCommand command{};
            command.commandId = m_NextStoryCommandId++;
            command.type = StoryCommandType::Abort;
            command.sessionId = state->sessionId;
            command.expectedRevision = state->revision;
            command.contentVersion = m_StoryCatalog.ContentVersion();
            (void)SendStoryCommand(std::move(command));
        }
    }
}

void Test2DGameApp::ConnectLateStoryReplica() {
    using namespace Engine::Story;

    if (m_SecondStoryReplicaConnected) {
        return;
    }
    m_SecondStoryReplicaConnected = m_StoryTransport.ConnectReplica(
        202,
        [this](const StorySessionState& state) {
            return m_StoryReplicaTwo.Synchronize(state);
        });
    if (m_SecondStoryReplicaConnected) {
        XLOG_INFO("Story ReplicaTwo connected late and replayed current state");
    } else {
        XLOG_ERROR("Story ReplicaTwo late connection failed");
    }
}

void Test2DGameApp::DrainStoryEvents(
    const char* runtimeName,
    Engine::Story::IStoryRuntime& runtime
) {
    using namespace Engine::Story;

    for (const StoryEvent& event : runtime.DrainEvents()) {
        std::ostringstream message;
        message << "Story[" << runtimeName << "] "
                << ToString(event.type)
                << " session=" << event.state.sessionId
                << " revision=" << event.state.revision;
        if (event.line.has_value()) {
            message << " speaker="
                    << (event.line->isNarration ? "<narration>" : event.line->speaker)
                    << " content=" << event.line->content;
        }
        if (event.type == StoryEventType::Error) {
            message << " code=" << ToString(event.result.code)
                    << " message=" << event.result.message;
            XLOG_WARN(message.str());
        } else {
            XLOG_INFO(message.str());
        }
    }
}

Engine::Story::StoryResult Test2DGameApp::SendStoryCommand(
    Engine::Story::StoryCommand command
) {
    using namespace Engine::Story;

    StoryResult result = m_StoryTransport.SendCommand(101, command);
    if (!result.Succeeded()) {
        const std::string message =
            "Story command rejected: code=" +
            std::string(ToString(result.code)) +
            " message=" + result.message;
        XLOG_WARN(message);
    }
    return result;
}
