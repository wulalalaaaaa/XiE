#include "App/Test2DGameApp.h"

#include "Core/GameApp.h"

#include <memory>

Engine::GameStartupDesc GetGameStartupDesc() {
    Engine::GameStartupDesc desc{};
    desc.GameName = "XiE test2Dgame";
    desc.ProjectRoot = ".";
    desc.AssetRoot = "Assets";
    desc.WindowWidth = 1280;
    desc.WindowHeight = 720;
    return desc;
}

std::unique_ptr<Engine::IGameApp> CreateGameApplication() {
    return std::make_unique<Test2DGameApp>();
}
