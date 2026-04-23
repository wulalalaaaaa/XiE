#include "GameBootstrap.h"

namespace Engine::Launch {

GameStartupDesc LoadGameStartupDesc() {
    return GetGameStartupDesc();
}

std::unique_ptr<IGameApp> CreateGameApp() {
    return CreateGameApplication();
}

} // namespace Engine::Launch

