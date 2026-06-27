#include "CollisionSystem2D.h"

#include <cmath>

namespace Engine::Runtime2D {

namespace {

float SignOrOne(float value) {
    return (value < 0.0f) ? -1.0f : 1.0f;
}

} // namespace

void CollisionSystem2D::Solve(World2D& world) const {
    std::vector<Entity2D>& entities = world.Entities();

    for (Entity2D& dynamicEntity : entities) {
        if (dynamicEntity.collider.isStatic) {
            continue;
        }

        for (const Entity2D& staticEntity : entities) {
            if (!staticEntity.collider.isStatic || staticEntity.id == dynamicEntity.id) {
                continue;
            }

            const float deltaX = dynamicEntity.transform.position.x - staticEntity.transform.position.x;
            const float deltaY = dynamicEntity.transform.position.y - staticEntity.transform.position.y;

            const float overlapX = (dynamicEntity.collider.halfExtent.x + staticEntity.collider.halfExtent.x) - std::fabs(deltaX);
            const float overlapY = (dynamicEntity.collider.halfExtent.y + staticEntity.collider.halfExtent.y) - std::fabs(deltaY);

            if (overlapX <= 0.0f || overlapY <= 0.0f) {
                continue;
            }

            if (overlapX < overlapY) {
                dynamicEntity.transform.position.x += SignOrOne(deltaX) * overlapX;
                dynamicEntity.velocity.linear.x = 0.0f;
            } else {
                dynamicEntity.transform.position.y += SignOrOne(deltaY) * overlapY;
                dynamicEntity.velocity.linear.y = 0.0f;
            }
        }
    }
}

} // namespace Engine::Runtime2D
