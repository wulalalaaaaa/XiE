#pragma once

#include "Runtime2D/World2D.h"

namespace Engine::Runtime2D {

class CollisionSystem2D {
public:
    void Solve(World2D& world) const;
};

} // namespace Engine::Runtime2D
