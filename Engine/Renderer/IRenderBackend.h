#pragma once

#include "Renderer2D/DrawTypes2D.h"

namespace Engine {

class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual bool Init() = 0;
    virtual void SetViewport(int width, int height) = 0;
    virtual void SetScissorEnabled(bool) {}
    virtual void SetScissorRect(int, int, int, int) {}
    virtual void SetBlendMode(BlendMode blendMode, AlphaMode alphaMode) = 0;
    virtual void UploadMesh(
        const float* vertices,
        int vertexCount,
        int vertexDimension,
        const float* uvs,
        int uvCount,
        const unsigned int* indices,
        int indexCount
    ) = 0;
    virtual void UploadTextureRGBA8(const unsigned char* pixels, int width, int height) = 0;
    virtual void BindTextureRGBA8(
        std::uint64_t key,
        const unsigned char* pixels,
        int width,
        int height
    ) {
        (void)key;
        UploadTextureRGBA8(pixels, width, height);
    }
    virtual void SetTextureEnabled(bool enabled) = 0;
    virtual void ClearTexture() = 0;
    virtual void SetMaterialTint(const float* rgba) = 0;
    virtual void SetViewProjection(const float* matrix4x4) = 0;
    virtual void BeginFrame() = 0;
    virtual void DrawMesh() = 0;
    virtual void EndFrame() = 0;
    virtual void Shutdown() = 0;
};

} // namespace Engine
