#pragma once

#include <cstdint>
#include <vector>

namespace Test2D {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Transform2D {
    Vec2 position{};
};

struct Velocity2D {
    Vec2 linear{};
};

struct Collider2D {
    Vec2 halfExtent{16.0f, 16.0f};
    bool isStatic = false;
};

struct SpriteRef2D {
    const char* spritePath = "Assets/Sprite/main.xsprite";
};

using EntityId = std::uint32_t;
constexpr EntityId kInvalidEntityId = 0;

struct Entity2D {
    EntityId id = kInvalidEntityId;
    Transform2D transform{};
    Velocity2D velocity{};
    Collider2D collider{};
    SpriteRef2D sprite{};
};

class World2D {
public:
    EntityId CreateEntity(const Entity2D& templateData = {});
    Entity2D* FindEntity(EntityId id);
    const Entity2D* FindEntity(EntityId id) const;

    void SetPlayer(EntityId id);
    EntityId GetPlayer() const;

    std::vector<Entity2D>& Entities();
    const std::vector<Entity2D>& Entities() const;

private:
    EntityId m_NextId = 1;
    EntityId m_Player = kInvalidEntityId;
    std::vector<Entity2D> m_Entities;
};

} // namespace Test2D
