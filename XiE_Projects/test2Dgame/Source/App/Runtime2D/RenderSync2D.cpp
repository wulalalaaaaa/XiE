#include "RenderSync2D.h"

#include "Core/Log.h"
#include "Renderer2D/DrawList2D.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace Test2D {

namespace {

struct RenderEntity2D {
    const Entity2D* entity = nullptr;
    std::array<float, 4> uvRect{0.0f, 0.0f, 1.0f, 1.0f};
};

std::string PathKey(const std::filesystem::path& path) {
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonical.generic_string();
    }
    return path.lexically_normal().generic_string();
}

bool SameAssetPath(const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
    if (lhs.empty() || rhs.empty()) {
        return false;
    }
    return PathKey(lhs) == PathKey(rhs);
}

void WarnOnce(std::unordered_set<std::string>& warnedIssues, const std::string& key, const std::string& message) {
    if (warnedIssues.insert(key).second) {
        XLOG_WARN(message.c_str());
    }
}

std::array<float, 4> GetSpriteUVRect(const Engine::SpriteAsset* sprite, const Engine::AtlasAsset* atlas) {
    std::array<float, 4> uvRect{0.0f, 0.0f, 1.0f, 1.0f};
    if (sprite == nullptr) {
        return uvRect;
    }

    const float* spriteUVRect = sprite->GetUVRect();
    if (spriteUVRect != nullptr) {
        uvRect = {spriteUVRect[0], spriteUVRect[1], spriteUVRect[2], spriteUVRect[3]};
    }

    if (sprite->UsesAtlasRegion() && atlas != nullptr) {
        std::array<float, 4> atlasUVRect{};
        if (atlas->TryGetRegionUVRect(sprite->GetAtlasRegionName(), atlasUVRect)) {
            uvRect = atlasUVRect;
        }
    }

    return uvRect;
}

std::array<float, 4> ResolveEntityUVRect(
    const Entity2D& entity,
    const AssetRuntime2DSnapshot& assets,
    std::unordered_set<std::string>& warnedIssues
) {
    const std::array<float, 4> fallbackUVRect = GetSpriteUVRect(assets.sprite, assets.atlas);
    if (entity.sprite.spritePath == nullptr || entity.sprite.spritePath[0] == '\0') {
        return fallbackUVRect;
    }

    const std::filesystem::path spritePath = std::filesystem::path(entity.sprite.spritePath).lexically_normal();
    const Engine::SpriteAsset* sprite = assets.FindSprite(spritePath);
    if (sprite == nullptr) {
        WarnOnce(
            warnedIssues,
            "missing-sprite:" + PathKey(spritePath),
            "RenderSync2D: missing sprite '" + spritePath.generic_string() + "', falling back to active sprite UV");
        return fallbackUVRect;
    }

    if (sprite->UsesAtlasRegion()) {
        const std::filesystem::path atlasPath = sprite->GetAtlasPath();
        const Engine::AtlasAsset* atlas = assets.FindAtlas(atlasPath);
        if (atlas == nullptr) {
            WarnOnce(
                warnedIssues,
                "missing-atlas:" + PathKey(atlasPath),
                "RenderSync2D: missing atlas '" + atlasPath.generic_string() + "' for sprite '" +
                    spritePath.generic_string() + "', falling back to active sprite UV");
            return fallbackUVRect;
        }

        if (!assets.texturePath.empty() && atlas->HasTexture() && !SameAssetPath(atlas->GetTexturePath(), assets.texturePath)) {
            WarnOnce(
                warnedIssues,
                "different-atlas-texture:" + PathKey(spritePath),
                "RenderSync2D: sprite '" + spritePath.generic_string() +
                    "' uses a different atlas texture, falling back to active sprite UV");
            return fallbackUVRect;
        }

        std::array<float, 4> atlasUVRect{};
        if (!atlas->TryGetRegionUVRect(sprite->GetAtlasRegionName(), atlasUVRect)) {
            WarnOnce(
                warnedIssues,
                "missing-region:" + PathKey(spritePath) + ":" + sprite->GetAtlasRegionName(),
                "RenderSync2D: atlas region '" + sprite->GetAtlasRegionName() + "' missing for sprite '" +
                    spritePath.generic_string() + "', falling back to active sprite UV");
            return fallbackUVRect;
        }

        return atlasUVRect;
    }

    if (sprite->HasTexture() && !assets.texturePath.empty() && !SameAssetPath(sprite->GetTexturePath(), assets.texturePath)) {
        WarnOnce(
            warnedIssues,
            "different-sprite-texture:" + PathKey(spritePath),
            "RenderSync2D: sprite '" + spritePath.generic_string() +
                "' uses a different texture, falling back to active sprite UV");
        return fallbackUVRect;
    }

    return GetSpriteUVRect(sprite, nullptr);
}

float RemapUV(float value, float minValue, float maxValue) {
    return minValue + value * (maxValue - minValue);
}

void BuildFallbackSprites(
    const std::vector<RenderEntity2D>& entities,
    Engine::DrawList2D& drawList
) {
    for (const RenderEntity2D& renderEntity : entities) {
        const Entity2D& entity = *renderEntity.entity;
        const float left = entity.transform.position.x - entity.collider.halfExtent.x;
        const float right = entity.transform.position.x + entity.collider.halfExtent.x;
        const float top = entity.transform.position.y - entity.collider.halfExtent.y;
        const float bottom = entity.transform.position.y + entity.collider.halfExtent.y;
        Engine::SpriteCommand command{};
        command.dst = {left, top, right - left, bottom - top};
        command.uv = {
            renderEntity.uvRect[0],
            renderEntity.uvRect[1],
            renderEntity.uvRect[2] - renderEntity.uvRect[0],
            renderEntity.uvRect[3] - renderEntity.uvRect[1]
        };
        command.texture = {1, 1};
        command.alphaMode = Engine::AlphaMode::Straight;
        command.blendMode = Engine::BlendMode::Alpha;
        drawList.AddSprite(command);
    }
}

} // namespace

void RenderSync2D::Sync(const World2D& world, const AssetRuntime2DSnapshot& assets, Engine::IRuntimeRender2D* runtimeRender2D) {
    if (runtimeRender2D == nullptr) {
        return;
    }

    const std::vector<Entity2D>& entities = world.Entities();
    std::vector<RenderEntity2D> visibleEntities;
    visibleEntities.reserve(entities.size());
    for (const Entity2D& entity : entities) {
        if (entity.renderable.visible) {
            visibleEntities.push_back({&entity, ResolveEntityUVRect(entity, assets, m_WarnedSpriteIssues)});
        }
    }

    if (visibleEntities.empty()) {
        runtimeRender2D->ClearRuntimeMesh2D();
        return;
    }

    std::stable_sort(visibleEntities.begin(), visibleEntities.end(), [](const RenderEntity2D& lhs, const RenderEntity2D& rhs) {
        return lhs.entity->renderable.layer < rhs.entity->renderable.layer;
    });

    const Engine::MeshAsset* mesh = assets.mesh;
    Engine::DrawList2D drawList;
    drawList.Reserve(visibleEntities.size());
    if (mesh == nullptr) {
        BuildFallbackSprites(visibleEntities, drawList);
        runtimeRender2D->SubmitDrawList2D(drawList);
        return;
    }

    const float* meshVertices = mesh->GetVertices();
    const int vertexDimension = mesh->GetVertexDimension();
    const int vertexCount = mesh->GetVertexCount();
    const unsigned int* meshIndices = mesh->GetIndices();
    const int meshIndexCount = mesh->GetIndexCount();
    if (meshVertices == nullptr || meshIndices == nullptr || vertexCount < 3 || meshIndexCount < 3 ||
        (meshIndexCount % 3) != 0 || (vertexDimension != 2 && vertexDimension != 3)) {
        return;
    }
    for (int i = 0; i < meshIndexCount; ++i) {
        if (meshIndices[i] >= static_cast<unsigned int>(vertexCount)) {
            return;
        }
    }

    std::vector<float> baseUVs(static_cast<std::size_t>(vertexCount) * 2, 0.0f);
    if (mesh->HasExplicitUVs() && mesh->GetUVs() != nullptr && mesh->GetUVCount() == vertexCount) {
        const float* meshUVs = mesh->GetUVs();
        for (int i = 0; i < vertexCount; ++i) {
            baseUVs[static_cast<std::size_t>(i) * 2] = meshUVs[static_cast<std::size_t>(i) * 2];
            baseUVs[static_cast<std::size_t>(i) * 2 + 1] = meshUVs[static_cast<std::size_t>(i) * 2 + 1];
        }
    } else {
        float minX = meshVertices[0];
        float maxX = meshVertices[0];
        float minY = meshVertices[1];
        float maxY = meshVertices[1];
        for (int i = 1; i < vertexCount; ++i) {
            const int base = i * vertexDimension;
            minX = std::min(minX, meshVertices[base]);
            maxX = std::max(maxX, meshVertices[base]);
            minY = std::min(minY, meshVertices[base + 1]);
            maxY = std::max(maxY, meshVertices[base + 1]);
        }

        float rangeX = maxX - minX;
        float rangeY = maxY - minY;
        if (std::fabs(rangeX) < 1e-6f) {
            rangeX = 1.0f;
        }
        if (std::fabs(rangeY) < 1e-6f) {
            rangeY = 1.0f;
        }

        for (int i = 0; i < vertexCount; ++i) {
            const int base = i * vertexDimension;
            baseUVs[static_cast<std::size_t>(i) * 2] = (meshVertices[base] - minX) / rangeX;
            baseUVs[static_cast<std::size_t>(i) * 2 + 1] = (meshVertices[base + 1] - minY) / rangeY;
        }
    }

    std::vector<float> vertices;
    std::vector<float> uvs;
    std::vector<unsigned int> indices;

    vertices.reserve(visibleEntities.size() * static_cast<std::size_t>(vertexCount * vertexDimension));
    uvs.reserve(visibleEntities.size() * static_cast<std::size_t>(vertexCount * 2));
    indices.reserve(visibleEntities.size() * static_cast<std::size_t>(meshIndexCount));

    float minX = meshVertices[0];
    float maxX = meshVertices[0];
    float minY = meshVertices[1];
    float maxY = meshVertices[1];
    for (int i = 1; i < vertexCount; ++i) {
        const int base = i * vertexDimension;
        minX = std::min(minX, meshVertices[base]);
        maxX = std::max(maxX, meshVertices[base]);
        minY = std::min(minY, meshVertices[base + 1]);
        maxY = std::max(maxY, meshVertices[base + 1]);
    }
    const float centerX = 0.5f * (minX + maxX);
    const float centerY = 0.5f * (minY + maxY);
    float meshHalfWidth = 0.5f * (maxX - minX);
    float meshHalfHeight = 0.5f * (maxY - minY);
    if (meshHalfWidth < 1e-6f) {
        meshHalfWidth = 1.0f;
    }
    if (meshHalfHeight < 1e-6f) {
        meshHalfHeight = 1.0f;
    }

    for (const RenderEntity2D& renderEntity : visibleEntities) {
        const Entity2D& entity = *renderEntity.entity;
        const unsigned int baseVertex = static_cast<unsigned int>(vertices.size() / static_cast<std::size_t>(vertexDimension));
        const float scaleX = entity.collider.halfExtent.x / meshHalfWidth;
        const float scaleY = entity.collider.halfExtent.y / meshHalfHeight;
        const float uMin = renderEntity.uvRect[0];
        const float vMin = renderEntity.uvRect[1];
        const float uMax = renderEntity.uvRect[2];
        const float vMax = renderEntity.uvRect[3];

        for (int i = 0; i < vertexCount; ++i) {
            const int sourceBase = i * vertexDimension;
            const float sourceX = meshVertices[sourceBase];
            const float sourceY = meshVertices[sourceBase + 1];
            const float transformedX = (sourceX - centerX) * scaleX + entity.transform.position.x;
            const float transformedY = (sourceY - centerY) * scaleY + entity.transform.position.y;
            vertices.push_back(transformedX);
            vertices.push_back(transformedY);
            if (vertexDimension == 3) {
                vertices.push_back(meshVertices[sourceBase + 2]);
            }

            uvs.push_back(RemapUV(baseUVs[static_cast<std::size_t>(i) * 2], uMin, uMax));
            uvs.push_back(RemapUV(baseUVs[static_cast<std::size_t>(i) * 2 + 1], vMin, vMax));
        }

        for (int i = 0; i < meshIndexCount; ++i) {
            indices.push_back(baseVertex + meshIndices[i]);
        }
    }

    Engine::CustomMeshCommand command{};
    command.vertices = std::move(vertices);
    command.vertexDimension = vertexDimension;
    command.uvs = std::move(uvs);
    command.indices = std::move(indices);
    command.texture = {1, 1};
    command.alphaMode = Engine::AlphaMode::Straight;
    command.blendMode = Engine::BlendMode::Alpha;
    drawList.AddCustomMesh(command);
    runtimeRender2D->SubmitDrawList2D(drawList);
}

} // namespace Test2D
