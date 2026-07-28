#include "Renderer3DFeature.h"

#include "Core/Log.h"
#include "Renderer/IRenderBackend.h"
#include "Platform/IRenderSurface.h"

#include <string>

namespace Engine {

void Renderer3DFeature::SetMeshAssetPath(const std::filesystem::path& path) {
    m_MeshAssetPath = path;
}

bool Renderer3DFeature::Init(IRenderBackend& backend, IRenderSurface& surface) {
    (void)surface;

    const bool loaded = m_AssetManager.Load<MeshAsset>(
        m_MeshAssetPath,
        AssetType::Mesh,
        [](const std::filesystem::path& path, MeshAsset& asset) {
            return asset.LoadFromFile(path);
        }
    );

    if (!loaded) {
        const std::string* reason = m_AssetManager.GetFailureReason(m_MeshAssetPath);
        if (reason != nullptr && !reason->empty()) {
            const std::string message = "Renderer3DFeature: failed to load mesh asset: " + *reason;
            XLOG_ERROR(message.c_str());
        } else {
            XLOG_ERROR("Renderer3DFeature: failed to load mesh asset");
        }
        return false;
    }

    UploadMesh(backend);
    return true;
}

void Renderer3DFeature::OnUpdate(IRenderBackend& backend, IRenderSurface& surface, float dt) {
    (void)surface;
    (void)dt;

    if constexpr (XIE_DEBUG != 0) {
        if (m_AssetManager.ReloadIfChanged<MeshAsset>(
                m_MeshAssetPath,
                [](const std::filesystem::path& path, MeshAsset& asset) {
                    return asset.LoadFromFile(path);
                })) {
            UploadMesh(backend);
        }
    }
}

void Renderer3DFeature::OnRender(IRenderBackend& backend, IRenderSurface& surface) {
    const RenderSurfaceSize size = surface.GetFramebufferSize();
    backend.SetViewport(size.width, size.height);

    constexpr float kIdentityViewProjection[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    backend.SetViewProjection(kIdentityViewProjection);

    backend.BeginFrame();
    backend.DrawMesh();
    backend.EndFrame();
}

void Renderer3DFeature::Shutdown(IRenderBackend& backend) {
    (void)backend;
}

void Renderer3DFeature::UploadMesh(IRenderBackend& backend) {
    const MeshAsset* mesh = m_AssetManager.GetHandle<MeshAsset>(m_MeshAssetPath);
    if (mesh == nullptr) {
        return;
    }

    const float* vertices = mesh->GetVertices();
    const int vertexCount = mesh->GetVertexCount();
    const int vertexDimension = mesh->GetVertexDimension();
    const unsigned int* indices = mesh->GetIndices();
    const int indexCount = mesh->GetIndexCount();

    if (vertices == nullptr || indices == nullptr || vertexCount < 3 || (vertexDimension != 2 && vertexDimension != 3) || indexCount < 3 || (indexCount % 3) != 0) {
        return;
    }

    const float* uvs = (mesh->HasExplicitUVs() ? mesh->GetUVs() : nullptr);
    const int uvCount = (mesh->HasExplicitUVs() ? mesh->GetUVCount() : 0);
    backend.UploadMesh(vertices, vertexCount, vertexDimension, uvs, uvCount, indices, indexCount);
}

} // namespace Engine
