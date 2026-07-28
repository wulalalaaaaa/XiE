#pragma once

<<<<<<< Updated upstream
struct GLFWwindow;
=======
#include "Core/GameApp.h"
#include "IRenderBackend.h"
#include "Feature/IRenderFeature.h"
#include "Platform/IRenderSurface.h"

#include <filesystem>
#include <memory>
>>>>>>> Stashed changes

namespace Engine {

class Renderer {
public:
<<<<<<< Updated upstream
    bool Init(GLFWwindow* windowHandle);
    void BeginFrame();
    void EndFrame();
    void Shutdown();
=======
    enum class Mode {
        Mode2D,
        Mode3D
    };

    bool Init(
        IRenderSurface& surface,
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
    void SubmitDrawList2D(const DrawList2D& drawList) override;
    void ClearRuntimeMesh2D() override;
    void SetCamera2D(float centerX, float centerY, float zoom) override;
>>>>>>> Stashed changes

private:
    bool InitOpenGLFunctions(GLFWwindow* windowHandle);
    bool CreateTrianglePipeline();

private:
<<<<<<< Updated upstream
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_ShaderProgram = 0;
=======
    IRenderSurface* m_Surface = nullptr;
    Mode m_Mode = Mode::Mode2D;

    std::filesystem::path m_MeshFilePath;
    std::filesystem::path m_TextureFilePath;
    std::filesystem::path m_MaterialFilePath;
    std::filesystem::path m_SpriteFilePath;
    int m_UVMode = 1;

    std::unique_ptr<IRenderBackend> m_Backend;
    std::unique_ptr<IRenderFeature> m_Feature;
    Renderer2DFeature* m_Feature2D = nullptr;
>>>>>>> Stashed changes
};

} // namespace Engine
