#include "MovementSystem2D.h"

namespace Engine::Runtime2D {

void MovementSystem2D::Integrate(World2D& world, float dt) const {
    for (Entity2D& entity : world.Entities()) {
        if (entity.collider.isStatic) {
            continue;
        }

        entity.transform.position.x += entity.velocity.linear.x * dt;
        entity.transform.position.y += entity.velocity.linear.y * dt;
    }
}

} // namespace Engine::Runtime2D
