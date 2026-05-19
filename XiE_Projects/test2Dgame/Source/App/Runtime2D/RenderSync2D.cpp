#include "RenderSync2D.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Test2D {

namespace {

void BuildFallbackQuadMesh(
    const std::vector<Entity2D>& entities,
    std::vector<float>& outVertices,
    std::vector<float>& outUVs,
    std::vector<unsigned int>& outIndices
) {
    outVertices.reserve(entities.size() * 8);
    outUVs.reserve(entities.size() * 8);
    outIndices.reserve(entities.size() * 6);

    for (const Entity2D& entity : entities) {
        const float left = entity.transform.position.x - entity.collider.halfExtent.x;
        const float right = entity.transform.position.x + entity.collider.halfExtent.x;
        const float top = entity.transform.position.y - entity.collider.halfExtent.y;
        const float bottom = entity.transform.position.y + entity.collider.halfExtent.y;
        const unsigned int baseVertex = static_cast<unsigned int>(outVertices.size() / 2);

        outVertices.push_back(left);
        outVertices.push_back(top);
        outVertices.push_back(right);
        outVertices.push_back(top);
        outVertices.push_back(right);
        outVertices.push_back(bottom);
        outVertices.push_back(left);
        outVertices.push_back(bottom);

        outUVs.push_back(0.0f);
        outUVs.push_back(0.0f);
        outUVs.push_back(1.0f);
        outUVs.push_back(0.0f);
        outUVs.push_back(1.0f);
        outUVs.push_back(1.0f);
        outUVs.push_back(0.0f);
        outUVs.push_back(1.0f);

        outIndices.push_back(baseVertex + 0);
        outIndices.push_back(baseVertex + 1);
        outIndices.push_back(baseVertex + 2);
        outIndices.push_back(baseVertex + 0);
        outIndices.push_back(baseVertex + 2);
        outIndices.push_back(baseVertex + 3);
    }
}

} // namespace

void RenderSync2D::Sync(const World2D& world, const AssetRuntime2DSnapshot& assets, Engine::IRuntimeRender2D* runtimeRender2D) {
    if (runtimeRender2D == nullptr) {
        return;
    }

    const std::vector<Entity2D>& entities = world.Entities();
    if (entities.empty()) {
        runtimeRender2D->ClearRuntimeMesh2D();
        return;
    }

    const Engine::MeshAsset* mesh = assets.mesh;
    if (mesh == nullptr) {
        std::vector<float> vertices;
        std::vector<float> uvs;
        std::vector<unsigned int> indices;
        BuildFallbackQuadMesh(entities, vertices, uvs, indices);
        runtimeRender2D->SubmitRuntimeMesh2D(
            vertices.data(),
            static_cast<int>(vertices.size() / 2),
            2,
            uvs.data(),
            static_cast<int>(uvs.size() / 2),
            indices.data(),
            static_cast<int>(indices.size()));
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

    vertices.reserve(entities.size() * static_cast<std::size_t>(vertexCount * vertexDimension));
    uvs.reserve(entities.size() * static_cast<std::size_t>(vertexCount * 2));
    indices.reserve(entities.size() * static_cast<std::size_t>(meshIndexCount));

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

    for (const Entity2D& entity : entities) {
        const unsigned int baseVertex = static_cast<unsigned int>(vertices.size() / static_cast<std::size_t>(vertexDimension));
        const float scaleX = entity.collider.halfExtent.x / meshHalfWidth;
        const float scaleY = entity.collider.halfExtent.y / meshHalfHeight;

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

            uvs.push_back(baseUVs[static_cast<std::size_t>(i) * 2]);
            uvs.push_back(baseUVs[static_cast<std::size_t>(i) * 2 + 1]);
        }

        for (int i = 0; i < meshIndexCount; ++i) {
            indices.push_back(baseVertex + meshIndices[i]);
        }
    }

    runtimeRender2D->SubmitRuntimeMesh2D(
        vertices.data(),
        static_cast<int>(vertices.size() / static_cast<std::size_t>(vertexDimension)),
        vertexDimension,
        uvs.data(),
        static_cast<int>(uvs.size() / 2),
        indices.data(),
        static_cast<int>(indices.size()));
}

} // namespace Test2D
