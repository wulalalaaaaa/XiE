#pragma once

#include "IRenderFeature.h"
#include "Assets/AssetManager.h"
#include "Assets/AtlasAsset.h"
#include "Assets/MaterialAsset.h"
#include "Assets/SpriteAsset.h"
#include "Assets/TextureAsset.h"
#include "Renderer/Camera/Camera2D.h"
#include "Renderer/Scene/Canvas2DDesc.h"
#include "Renderer/MeshAsset.h"
#include "Renderer2D/Backends/OpenGL/OpenGL2DRenderer.h"
#include "Renderer2D/DrawList2D.h"

#include <filesystem>
#include <vector>

namespace Engine {

class Renderer2DFeature final : public IRenderFeature {
public:
    enum class UVControlMode {
        AutoGenerate = 0,
        PreferAsset,
        RequireAsset
    };

    void SetMeshAssetPath(const std::filesystem::path& path);
    void SetTextureAssetPath(const std::filesystem::path& path);
    void SetMaterialAssetPath(const std::filesystem::path& path);
    void SetSpriteAssetPath(const std::filesystem::path& path);
    void SetUVControlMode(UVControlMode mode);

    bool Init(IRenderBackend& backend, IRenderSurface& surface) override;
    void OnUpdate(IRenderBackend& backend, IRenderSurface& surface, float dt) override;
    void OnRender(IRenderBackend& backend, IRenderSurface& surface) override;
    void Shutdown(IRenderBackend& backend) override;
    bool SubmitRuntimeMesh(
        const float* vertices,
        int vertexCount,
        int vertexDimension,
        const float* uvs,
        int uvCount,
        const unsigned int* indices,
        int indexCount
    );
    void SubmitDrawList(const DrawList2D& drawList);
    void ClearRuntimeMesh();
    void SetCamera(float centerX, float centerY, float zoom);

private:
    bool LoadRenderAssets(IRenderBackend& backend);
    bool LoadAtlasAssetByPath(const std::filesystem::path& path);
    bool LoadTextureAssetByPath(const std::filesystem::path& path);
    std::filesystem::path ResolveAtlasAssetPath() const;
    std::filesystem::path ResolveTextureAssetPath() const;
    void ApplyMaterialAndTexture(IRenderBackend& backend);
    void UploadMesh(IRenderBackend& backend);

private:
    bool m_HasRuntimeMesh = false;
    bool m_RuntimeMeshDirty = false;
    bool m_HasDrawList = false;
    DrawList2D m_DrawList;
    OpenGL2DRenderer m_OpenGL2DRenderer;
    std::vector<float> m_RuntimeVertices;
    int m_RuntimeVertexCount = 0;
    int m_RuntimeVertexDimension = 2;
    std::vector<float> m_RuntimeUVs;
    int m_RuntimeUVCount = 0;
    std::vector<unsigned int> m_RuntimeIndices;
    int m_RuntimeIndexCount = 0;

private:
    std::filesystem::path m_MeshAssetPath;
    std::filesystem::path m_TextureAssetPath;
    std::filesystem::path m_MaterialAssetPath;
    std::filesystem::path m_SpriteAssetPath;
    std::filesystem::path m_ResolvedAtlasAssetPath;
    std::filesystem::path m_ResolvedTextureAssetPath;

    AssetManager m_AssetManager;
    Canvas2DDesc m_Canvas{};
    Camera2D m_Camera{};
    UVControlMode m_UVControlMode = UVControlMode::PreferAsset;
};

} // namespace Engine
