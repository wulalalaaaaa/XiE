#pragma once

#include "IRenderFeature.h"
#include "Assets/AssetManager.h"
#include "Renderer/Camera/Camera2D.h"
#include "Renderer/Camera/Camera2DController.h"
#include "Renderer/Scene/Canvas2DDesc.h"
#include "Renderer/MeshAsset.h"

#include <filesystem>

namespace Engine {

class Renderer2DFeature final : public IRenderFeature {
public:
    void SetMeshAssetPath(const std::filesystem::path& path);

    bool Init(IRenderBackend& backend, GLFWwindow* windowHandle) override;
    void OnUpdate(IRenderBackend& backend, GLFWwindow* windowHandle, float dt) override;
    void OnRender(IRenderBackend& backend, GLFWwindow* windowHandle) override;
    void Shutdown(IRenderBackend& backend) override;

private:
    void UploadMesh(IRenderBackend& backend, GLFWwindow* windowHandle);

private:
    std::filesystem::path m_MeshAssetPath;

    AssetManager m_AssetManager;
    Canvas2DDesc m_Canvas{};
    Camera2D m_Camera{};
    Camera2DController m_Controller{};
};

} // namespace Engine
