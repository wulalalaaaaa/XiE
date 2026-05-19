#pragma once

#include "World2D.h"

namespace Test2D {

class CollisionSystem2D {
public:
    void Solve(World2D& world) const;
};

} // namespace Test2D
