#pragma once

#include "Core/GameApp.h"
#include "IRenderBackend.h"
#include "Feature/IRenderFeature.h"

#include <filesystem>
#include <memory>

struct GLFWwindow;

namespace Engine {

class Renderer2DFeature;

template <bool Debug>
class BasicRenderer : public IRuntimeRender2D {
public:
    enum class Mode {
        Mode2D,
        Mode3D
    };

    bool Init(
        GLFWwindow* windowHandle,
        const std::filesystem::path& meshFilePath,
        const std::filesystem::path& textureFilePath,
        const std::filesystem::path& materialFilePath,
        const std::filesystem::path& spriteFilePath,
        int uvMode
    );
    void Tick(float dt);
    void BeginFrame();
    void EndFrame();
    void Shutdown();
    bool SubmitRuntimeMesh2D(
        const float* vertices,
        int vertexCount,
        int vertexDimension,
        const float* uvs,
        int uvCount,
        const unsigned int* indices,
        int indexCount
    ) override;
    void ClearRuntimeMesh2D() override;

private:
    bool InitFeature();

private:
    GLFWwindow* m_WindowHandle = nullptr;
    Mode m_Mode = Mode::Mode2D;

    std::filesystem::path m_MeshFilePath;
    std::filesystem::path m_TextureFilePath;
    std::filesystem::path m_MaterialFilePath;
    std::filesystem::path m_SpriteFilePath;
    int m_UVMode = 1;

    std::unique_ptr<IRenderBackend> m_Backend;
    std::unique_ptr<IRenderFeature> m_Feature;
    Renderer2DFeature* m_Feature2D = nullptr;
};

using Renderer = BasicRenderer<(XIE_DEBUG != 0)>;

} // namespace Engine
