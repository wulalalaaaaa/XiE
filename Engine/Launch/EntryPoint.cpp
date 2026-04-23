#include "Core/Application.h"
#include "GameBootstrap.h"

#include <cstdio>
#include <utility>

int main() {
    const Engine::GameStartupDesc startup = Engine::Launch::LoadGameStartupDesc();
    std::unique_ptr<Engine::IGameApp> gameApp = Engine::Launch::CreateGameApp();
    if (!gameApp) {
        std::fprintf(stderr, "CreateGameApplication returned null.\n");
        return 1;
    }

    Engine::Application app(startup, std::move(gameApp));
    app.Run();
    return 0;
}
