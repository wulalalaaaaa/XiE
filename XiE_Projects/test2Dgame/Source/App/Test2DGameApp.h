#pragma once

#include "Core/GameApp.h"

class Test2DGameApp final : public Engine::IGameApp {
public:
    void OnInit() override;
    void OnUpdate(float dt) override;
    void OnRender() override;
    void OnShutdown() override;
};

