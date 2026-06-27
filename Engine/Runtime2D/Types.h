#pragma once

#include <cstdint>

namespace Engine::Runtime2D {

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

struct Renderable2D {
    bool visible = true;
    int layer = 0;
};

using EntityId = std::uint32_t;
constexpr EntityId kInvalidEntityId = 0;

} // namespace Engine::Runtime2D
