#pragma once

#include "AssetRuntime2D.h"
#include "Core/GameApp.h"
#include "World2D.h"

#include <string>
#include <unordered_set>

namespace Test2D {

class RenderSync2D {
public:
    void Sync(const World2D& world, const AssetRuntime2DSnapshot& assets, Engine::IRuntimeRender2D* runtimeRender2D);

private:
    std::unordered_set<std::string> m_WarnedSpriteIssues;
};

} // namespace Test2D
