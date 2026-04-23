#pragma once

#include "Core/GameApp.h"

#include <memory>

namespace Engine::Launch {

GameStartupDesc LoadGameStartupDesc();
std::unique_ptr<IGameApp> CreateGameApp();

} // namespace Engine::Launch

Engine::GameStartupDesc GetGameStartupDesc();
std::unique_ptr<Engine::IGameApp> CreateGameApplication();
