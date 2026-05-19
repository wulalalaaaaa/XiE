#pragma once

#include "IRenderFeature.h"
#include "Assets/AssetManager.h"
#include "Renderer/MeshAsset.h"

#include <filesystem>

namespace Engine {

class Renderer3DFeature final : public IRenderFeature {
public:
    void SetMeshAssetPath(const std::filesystem::path& path);

    bool Init(IRenderBackend& backend, GLFWwindow* windowHandle) override;
    void OnUpdate(IRenderBackend& backend, GLFWwindow* windowHandle, float dt) override;
    void OnRender(IRenderBackend& backend, GLFWwindow* windowHandle) override;
    void Shutdown(IRenderBackend& backend) override;

private:
    void UploadMesh(IRenderBackend& backend);

private:
    std::filesystem::path m_MeshAssetPath;
    AssetManager m_AssetManager;
};

} // namespace Engine
