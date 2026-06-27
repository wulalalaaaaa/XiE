#pragma once

#include "Runtime2D/Types.h"

#include <vector>

namespace Engine::Runtime2D {

struct Entity2D {
    EntityId id = kInvalidEntityId;
    Transform2D transform{};
    Velocity2D velocity{};
    Collider2D collider{};
    SpriteRef2D sprite{};
    Renderable2D renderable{};
};

class World2D {
public:
    EntityId CreateEntity(const Entity2D& templateData = {});
    Entity2D* FindEntity(EntityId id);
    const Entity2D* FindEntity(EntityId id) const;
    void Clear();

    void SetPlayer(EntityId id);
    EntityId GetPlayer() const;

    std::vector<Entity2D>& Entities();
    const std::vector<Entity2D>& Entities() const;

private:
    EntityId m_NextId = 1;
    EntityId m_Player = kInvalidEntityId;
    std::vector<Entity2D> m_Entities;
};

} // namespace Engine::Runtime2D
