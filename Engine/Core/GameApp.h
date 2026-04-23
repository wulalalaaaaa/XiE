#pragma once

#include <memory>

namespace Engine {

class IGameApp {
public:
    virtual ~IGameApp() = default;

    virtual void OnInit() = 0;
    virtual void OnUpdate(float dt) = 0;
    virtual void OnRender() = 0;
    virtual void OnShutdown() = 0;
};

struct GameStartupDesc {
    const char* GameName = "XiE Game";
    const char* ProjectRoot = "";
    const char* AssetRoot = "";
    int WindowWidth = 1280;
    int WindowHeight = 720;
};

} // namespace Engine

