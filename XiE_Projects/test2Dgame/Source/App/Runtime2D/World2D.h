#pragma once

#include <Runtime2D/World2D.h>

namespace Test2D {

using Vec2 = Engine::Runtime2D::Vec2;
using Transform2D = Engine::Runtime2D::Transform2D;
using Velocity2D = Engine::Runtime2D::Velocity2D;
using Collider2D = Engine::Runtime2D::Collider2D;
using SpriteRef2D = Engine::Runtime2D::SpriteRef2D;
using Renderable2D = Engine::Runtime2D::Renderable2D;
using EntityId = Engine::Runtime2D::EntityId;
constexpr EntityId kInvalidEntityId = Engine::Runtime2D::kInvalidEntityId;
using Entity2D = Engine::Runtime2D::Entity2D;
using World2D = Engine::Runtime2D::World2D;

} // namespace Test2D
