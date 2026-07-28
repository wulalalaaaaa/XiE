#include "World2DDrawBuilder.h"

namespace Engine {

void World2DDrawBuilder::Build(const Runtime2D::World2D& world, const Camera2D& camera, DrawList2D& output) {
    (void)camera;
    output.Clear();
    output.Reserve(world.Entities().size());

    for (const Runtime2D::Entity2D& entity : world.Entities()) {
        if (!entity.renderable.visible) {
            continue;
        }

        SpriteCommand command{};
        command.dst = {
            entity.transform.position.x - entity.collider.halfExtent.x,
            entity.transform.position.y - entity.collider.halfExtent.y,
            entity.collider.halfExtent.x * 2.0f,
            entity.collider.halfExtent.y * 2.0f
        };
        command.texture = {1, 1};
        command.alphaMode = AlphaMode::Straight;
        command.blendMode = BlendMode::Alpha;
        output.AddSprite(command);
    }
}

} // namespace Engine
