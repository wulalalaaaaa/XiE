#pragma once

#include "AssetRuntime2D.h"
#include "Core/GameApp.h"
#include "World2D.h"

namespace Test2D {

class RenderSync2D {
public:
    void Sync(const World2D& world, const AssetRuntime2DSnapshot& assets, Engine::IRuntimeRender2D* runtimeRender2D);
};

} // namespace Test2D
