#pragma once

#include "DrawList2D.h"
#include "Renderer/Camera/Camera2D.h"
#include "Runtime2D/World2D.h"

namespace Engine {

class World2DDrawBuilder {
public:
    void Build(const Runtime2D::World2D& world, const Camera2D& camera, DrawList2D& output);
};

} // namespace Engine
