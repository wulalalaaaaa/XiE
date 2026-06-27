#pragma once

#include "Runtime2D/World2D.h"

namespace Engine::Runtime2D {

class MovementSystem2D {
public:
    void Integrate(World2D& world, float dt) const;
};

} // namespace Engine::Runtime2D
