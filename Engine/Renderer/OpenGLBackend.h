#pragma once

#include "IRenderBackend.h"

#include <cstdint>
#include <unordered_map>

namespace Engine {

class OpenGLBackend final : public IRenderBackend {
public:
    bool Init() override;
    void SetViewport(int width, int height) override;
    void SetScissorEnabled(bool enabled) override;
    void SetScissorRect(int x, int y, int width, int height) override;
    void SetBlendMode(BlendMode blendMode, AlphaMode alphaMode) override;
    void UploadMesh(
        const float* vertices,
        int vertexCount,
        int vertexDimension,
        const float* uvs,
        int uvCount,
        const unsigned int* indices,
        int indexCount
    ) override;
    void UploadTextureRGBA8(const unsigned char* pixels, int width, int height) override;
    void BindTextureRGBA8(
        std::uint64_t key,
        const unsigned char* pixels,
        int width,
        int height
    ) override;
    void SetTextureEnabled(bool enabled) override;
    void ClearTexture() override;
    void SetMaterialTint(const float* rgba) override;
    void SetViewProjection(const float* matrix4x4) override;
    void BeginFrame() override;
    void DrawMesh() override;
    void EndFrame() override;
    void Shutdown() override;

private:
    bool CreateTrianglePipeline();
    unsigned int CompileShader(unsigned int type, const char* source);

private:
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_EBO = 0;
    unsigned int m_MainTexture = 0;
    unsigned int m_TransientTexture = 0;
    std::unordered_map<std::uint64_t, unsigned int> m_TextureCache;
    unsigned int m_ShaderProgram = 0;
    int m_ViewProjLocation = -1;
    int m_HasTextureLocation = -1;
    int m_TintLocation = -1;
    int m_IndexCount = 0;
    bool m_HasTexture = false;
    float m_Tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    int m_ViewportHeight = 0;
};

} // namespace Engine
