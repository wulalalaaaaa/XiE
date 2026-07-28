#include "Renderer2DFeature.h"

#include "Core/Log.h"
#include "Renderer/IRenderBackend.h"
#include "Platform/IRenderSurface.h"
#include "Renderer2D/Backends/OpenGL/OpenGLWindowRenderPipeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace Engine {

void Renderer2DFeature::SetMeshAssetPath(const std::filesystem::path& path) {
    m_MeshAssetPath = path;
}

void Renderer2DFeature::SetTextureAssetPath(const std::filesystem::path& path) {
    m_TextureAssetPath = path;
}

void Renderer2DFeature::SetMaterialAssetPath(const std::filesystem::path& path) {
    m_MaterialAssetPath = path;
}

void Renderer2DFeature::SetSpriteAssetPath(const std::filesystem::path& path) {
    m_SpriteAssetPath = path;
}

void Renderer2DFeature::SetUVControlMode(UVControlMode mode) {
    m_UVControlMode = mode;
}

bool Renderer2DFeature::SubmitRuntimeMesh(
    const float* vertices,
    int vertexCount,
    int vertexDimension,
    const float* uvs,
    int uvCount,
    const unsigned int* indices,
    int indexCount
) {
    if (vertices == nullptr || indices == nullptr || vertexCount < 3 || indexCount < 3 || (indexCount % 3) != 0) {
        return false;
    }
    if (vertexDimension != 2 && vertexDimension != 3) {
        return false;
    }
    if (uvs != nullptr && uvCount != vertexCount) {
        return false;
    }

    m_RuntimeVertices.assign(vertices, vertices + static_cast<std::size_t>(vertexCount * vertexDimension));
    m_RuntimeVertexCount = vertexCount;
    m_RuntimeVertexDimension = vertexDimension;

    m_RuntimeUVs.clear();
    m_RuntimeUVCount = 0;
    if (uvs != nullptr && uvCount > 0) {
        m_RuntimeUVs.assign(uvs, uvs + static_cast<std::size_t>(uvCount * 2));
        m_RuntimeUVCount = uvCount;
    }

    m_RuntimeIndices.assign(indices, indices + indexCount);
    m_RuntimeIndexCount = indexCount;
    m_HasRuntimeMesh = true;
    m_HasDrawList = false;
    m_RuntimeMeshDirty = true;
    return true;
}

void Renderer2DFeature::SubmitDrawList(const DrawList2D& drawList) {
    m_DrawList = drawList;
    m_HasDrawList = !drawList.Empty();
    m_HasRuntimeMesh = false;
    m_RuntimeMeshDirty = false;
}

void Renderer2DFeature::ClearRuntimeMesh() {
    if (!m_HasRuntimeMesh && !m_HasDrawList) {
        return;
    }

    m_HasRuntimeMesh = false;
    m_HasDrawList = false;
    m_RuntimeMeshDirty = true;
    m_DrawList.Clear();
    m_RuntimeVertices.clear();
    m_RuntimeUVs.clear();
    m_RuntimeIndices.clear();
    m_RuntimeVertexCount = 0;
    m_RuntimeVertexDimension = 2;
    m_RuntimeUVCount = 0;
    m_RuntimeIndexCount = 0;
}

void Renderer2DFeature::SetCamera(float centerX, float centerY, float zoom) {
    const float safeZoom = (std::isfinite(zoom) && zoom > 0.0f) ? zoom : 1.0f;
    m_Camera.SetZoom(safeZoom);

    if (m_Canvas.originAtCenter) {
        m_Camera.SetPosition(centerX, centerY);
        return;
    }

    const float halfW = (m_Canvas.worldWidth * 0.5f) / safeZoom;
    const float halfH = (m_Canvas.worldHeight * 0.5f) / safeZoom;
    m_Camera.SetPosition(centerX - halfW, (m_Canvas.worldHeight - centerY) - halfH);
}

bool Renderer2DFeature::Init(IRenderBackend& backend, IRenderSurface& surface) {
    (void)surface;
    m_Canvas.worldWidth = 1280.0f;
    m_Canvas.worldHeight = 720.0f;
    m_Canvas.originAtCenter = false;

    m_Camera.SetCanvas(m_Canvas);

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
            const std::string message = "Renderer2DFeature: failed to load mesh asset: " + *reason;
            XLOG_ERROR(message.c_str());
        } else {
            XLOG_ERROR("Renderer2DFeature: failed to load mesh asset");
        }
        return false;
    }

    if (!LoadRenderAssets(backend)) {
        return false;
    }

    UploadMesh(backend);
    return true;
}

void Renderer2DFeature::OnUpdate(IRenderBackend& backend, IRenderSurface& surface, float dt) {
    (void)dt;

    const RenderSurfaceSize size = surface.GetFramebufferSize();
    m_Camera.SetViewportSize(size.width, size.height);

    if constexpr (XIE_DEBUG != 0) {
        bool renderAssetChanged = false;
        bool meshAssetChanged = false;

        if (!m_MaterialAssetPath.empty() && m_AssetManager.ReloadIfChanged<MaterialAsset>(
                m_MaterialAssetPath,
                [](const std::filesystem::path& path, MaterialAsset& asset) {
                    return asset.LoadFromFile(path);
                })) {
            renderAssetChanged = true;
        }

        if (!m_SpriteAssetPath.empty() && std::filesystem::exists(m_SpriteAssetPath) && m_AssetManager.ReloadIfChanged<SpriteAsset>(
                m_SpriteAssetPath,
                [](const std::filesystem::path& path, SpriteAsset& asset) {
                    return asset.LoadFromFile(path);
                })) {
            renderAssetChanged = true;
            meshAssetChanged = true;
        }

        if (!m_SpriteAssetPath.empty() && std::filesystem::exists(m_SpriteAssetPath) && m_AssetManager.GetHandle<SpriteAsset>(m_SpriteAssetPath) == nullptr) {
            if (m_AssetManager.Load<SpriteAsset>(
                    m_SpriteAssetPath,
                    AssetType::Sprite,
                    [](const std::filesystem::path& path, SpriteAsset& asset) {
                        return asset.LoadFromFile(path);
                    })) {
                renderAssetChanged = true;
                meshAssetChanged = true;
            }
        }

        const std::filesystem::path resolvedAtlasPath = ResolveAtlasAssetPath();
        if (resolvedAtlasPath != m_ResolvedAtlasAssetPath) {
            m_ResolvedAtlasAssetPath = resolvedAtlasPath;
            if (!m_ResolvedAtlasAssetPath.empty()) {
                if (!LoadAtlasAssetByPath(m_ResolvedAtlasAssetPath)) {
                    const std::string* reason = m_AssetManager.GetFailureReason(m_ResolvedAtlasAssetPath);
                    if (reason != nullptr && !reason->empty()) {
                        const std::string message = "Renderer2DFeature: failed to load resolved atlas asset: " + *reason;
                        XLOG_ERROR(message.c_str());
                    }
                }
            }
            renderAssetChanged = true;
            meshAssetChanged = true;
        }

        if (!m_ResolvedAtlasAssetPath.empty() && m_AssetManager.ReloadIfChanged<AtlasAsset>(
                m_ResolvedAtlasAssetPath,
                [](const std::filesystem::path& path, AtlasAsset& asset) {
                    return asset.LoadFromFile(path);
                })) {
            renderAssetChanged = true;
            meshAssetChanged = true;
        }

        if (!m_TextureAssetPath.empty() && m_AssetManager.ReloadIfChanged<TextureAsset>(
                m_TextureAssetPath,
                [](const std::filesystem::path& path, TextureAsset& asset) {
                    return asset.LoadFromFile(path);
                })) {
            renderAssetChanged = true;
        }

        const std::filesystem::path resolvedTexturePath = ResolveTextureAssetPath();
        if (resolvedTexturePath != m_ResolvedTextureAssetPath) {
            m_ResolvedTextureAssetPath = resolvedTexturePath;
            if (!m_ResolvedTextureAssetPath.empty()) {
                if (!LoadTextureAssetByPath(m_ResolvedTextureAssetPath)) {
                    const std::string* reason = m_AssetManager.GetFailureReason(m_ResolvedTextureAssetPath);
                    if (reason != nullptr && !reason->empty()) {
                        const std::string message = "Renderer2DFeature: failed to load resolved texture asset: " + *reason;
                        XLOG_ERROR(message.c_str());
                    }
                }
            }
            renderAssetChanged = true;
        }

        if (!m_ResolvedTextureAssetPath.empty() && m_AssetManager.ReloadIfChanged<TextureAsset>(
                m_ResolvedTextureAssetPath,
                [](const std::filesystem::path& path, TextureAsset& asset) {
                    return asset.LoadFromFile(path);
                })) {
            renderAssetChanged = true;
        }

        if (renderAssetChanged) {
            ApplyMaterialAndTexture(backend);
        }

        if (m_AssetManager.ReloadIfChanged<MeshAsset>(
                m_MeshAssetPath,
                [](const std::filesystem::path& path, MeshAsset& asset) {
                    return asset.LoadFromFile(path);
                })) {
            meshAssetChanged = true;
        }

        if (meshAssetChanged) {
            UploadMesh(backend);
        }
    }

    if (m_RuntimeMeshDirty) {
        UploadMesh(backend);
        m_RuntimeMeshDirty = false;
    }
}

void Renderer2DFeature::OnRender(IRenderBackend& backend, IRenderSurface& surface) {
    const RenderSurfaceSize size = surface.GetFramebufferSize();
    backend.SetViewport(size.width, size.height);

    const auto vp = m_Camera.GetViewProjection();
    backend.SetViewProjection(vp.data());

    if (m_HasDrawList) {
        Render2DView view{};
        view.framebufferWidth = static_cast<float>(size.width);
        view.framebufferHeight = static_cast<float>(size.height);
        view.dpiScale = 1.0f;
        OpenGLWindowRenderPipeline pipeline(surface, backend, m_OpenGL2DRenderer);
        (void)pipeline.Render(m_DrawList, view);
        return;
    }

    backend.BeginFrame();
    backend.DrawMesh();
    backend.EndFrame();
}

void Renderer2DFeature::Shutdown(IRenderBackend& backend) {
    (void)backend;
}

bool Renderer2DFeature::LoadRenderAssets(IRenderBackend& backend) {
    if (!m_MaterialAssetPath.empty()) {
        const bool loadedMaterial = m_AssetManager.Load<MaterialAsset>(
            m_MaterialAssetPath,
            AssetType::Material,
            [](const std::filesystem::path& path, MaterialAsset& asset) {
                return asset.LoadFromFile(path);
            }
        );

        if (!loadedMaterial) {
            const std::string* reason = m_AssetManager.GetFailureReason(m_MaterialAssetPath);
            if (reason != nullptr && !reason->empty()) {
                const std::string message = "Renderer2DFeature: failed to load material asset: " + *reason;
                XLOG_ERROR(message.c_str());
            } else {
                XLOG_ERROR("Renderer2DFeature: failed to load material asset");
            }
            return false;
        }
    }

    if (!m_SpriteAssetPath.empty() && std::filesystem::exists(m_SpriteAssetPath)) {
        const bool loadedSprite = m_AssetManager.Load<SpriteAsset>(
            m_SpriteAssetPath,
            AssetType::Sprite,
            [](const std::filesystem::path& path, SpriteAsset& asset) {
                return asset.LoadFromFile(path);
            }
        );

        if (!loadedSprite) {
            const std::string* reason = m_AssetManager.GetFailureReason(m_SpriteAssetPath);
            if (reason != nullptr && !reason->empty()) {
                const std::string message = "Renderer2DFeature: failed to load sprite asset: " + *reason;
                XLOG_ERROR(message.c_str());
            } else {
                XLOG_ERROR("Renderer2DFeature: failed to load sprite asset");
            }
            return false;
        }
    }

    m_ResolvedAtlasAssetPath = ResolveAtlasAssetPath();
    if (!m_ResolvedAtlasAssetPath.empty() && !LoadAtlasAssetByPath(m_ResolvedAtlasAssetPath)) {
        const std::string* reason = m_AssetManager.GetFailureReason(m_ResolvedAtlasAssetPath);
        if (reason != nullptr && !reason->empty()) {
            const std::string message = "Renderer2DFeature: failed to load resolved atlas asset: " + *reason;
            XLOG_ERROR(message.c_str());
        } else {
            XLOG_ERROR("Renderer2DFeature: failed to load resolved atlas asset");
        }
        return false;
    }

    if (!m_TextureAssetPath.empty() && !LoadTextureAssetByPath(m_TextureAssetPath)) {
        const std::string* reason = m_AssetManager.GetFailureReason(m_TextureAssetPath);
        if (reason != nullptr && !reason->empty()) {
            const std::string message = "Renderer2DFeature: failed to load texture asset: " + *reason;
            XLOG_ERROR(message.c_str());
        } else {
            XLOG_ERROR("Renderer2DFeature: failed to load texture asset");
        }
        return false;
    }

    m_ResolvedTextureAssetPath = ResolveTextureAssetPath();
    if (!m_ResolvedTextureAssetPath.empty() && !LoadTextureAssetByPath(m_ResolvedTextureAssetPath)) {
        const std::string* reason = m_AssetManager.GetFailureReason(m_ResolvedTextureAssetPath);
        if (reason != nullptr && !reason->empty()) {
            const std::string message = "Renderer2DFeature: failed to load resolved texture asset: " + *reason;
            XLOG_ERROR(message.c_str());
        } else {
            XLOG_ERROR("Renderer2DFeature: failed to load resolved texture asset");
        }
        return false;
    }

    ApplyMaterialAndTexture(backend);
    return true;
}

bool Renderer2DFeature::LoadTextureAssetByPath(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }

    return m_AssetManager.Load<TextureAsset>(
        path,
        AssetType::Texture,
        [](const std::filesystem::path& texturePath, TextureAsset& asset) {
            return asset.LoadFromFile(texturePath);
        }
    );
}

bool Renderer2DFeature::LoadAtlasAssetByPath(const std::filesystem::path& path) {
    if (path.empty()) {
        return false;
    }

    return m_AssetManager.Load<AtlasAsset>(
        path,
        AssetType::Atlas,
        [](const std::filesystem::path& atlasPath, AtlasAsset& asset) {
            return asset.LoadFromFile(atlasPath);
        }
    );
}

std::filesystem::path Renderer2DFeature::ResolveAtlasAssetPath() const {
    if (m_SpriteAssetPath.empty() || !std::filesystem::exists(m_SpriteAssetPath)) {
        return {};
    }

    const SpriteAsset* sprite = m_AssetManager.GetHandle<SpriteAsset>(m_SpriteAssetPath);
    if (sprite == nullptr || !sprite->UsesAtlasRegion()) {
        return {};
    }

    return sprite->GetAtlasPath();
}

std::filesystem::path Renderer2DFeature::ResolveTextureAssetPath() const {
    if (!m_ResolvedAtlasAssetPath.empty()) {
        const AtlasAsset* atlas = m_AssetManager.GetHandle<AtlasAsset>(m_ResolvedAtlasAssetPath);
        if (atlas != nullptr && atlas->HasTexture()) {
            return atlas->GetTexturePath();
        }
    }

    if (!m_SpriteAssetPath.empty() && std::filesystem::exists(m_SpriteAssetPath)) {
        const SpriteAsset* sprite = m_AssetManager.GetHandle<SpriteAsset>(m_SpriteAssetPath);
        if (sprite != nullptr && sprite->HasTexture()) {
            return sprite->GetTexturePath();
        }
    }

    const MaterialAsset* material = nullptr;
    if (!m_MaterialAssetPath.empty()) {
        material = m_AssetManager.GetHandle<MaterialAsset>(m_MaterialAssetPath);
    }

    if (material != nullptr && material->HasTexture()) {
        return material->GetTexturePath();
    }

    return m_TextureAssetPath;
}

void Renderer2DFeature::ApplyMaterialAndTexture(IRenderBackend& backend) {
    float tint[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    if (!m_MaterialAssetPath.empty()) {
        const MaterialAsset* material = m_AssetManager.GetHandle<MaterialAsset>(m_MaterialAssetPath);
        if (material != nullptr && material->GetTint() != nullptr) {
            const float* materialTint = material->GetTint();
            tint[0] = materialTint[0];
            tint[1] = materialTint[1];
            tint[2] = materialTint[2];
            tint[3] = materialTint[3];
        }
    }
    backend.SetMaterialTint(tint);

    const std::filesystem::path activeTexturePath = m_ResolvedTextureAssetPath.empty() ? m_TextureAssetPath : m_ResolvedTextureAssetPath;
    if (!activeTexturePath.empty()) {
        const TextureAsset* texture = m_AssetManager.GetHandle<TextureAsset>(activeTexturePath);
        if (texture != nullptr && texture->GetPixels() != nullptr && texture->GetWidth() > 0 && texture->GetHeight() > 0) {
            backend.UploadTextureRGBA8(texture->GetPixels(), texture->GetWidth(), texture->GetHeight());
            return;
        }
    }

    backend.ClearTexture();
}

void Renderer2DFeature::UploadMesh(IRenderBackend& backend) {
    const MeshAsset* mesh = nullptr;
    const float* vertices = nullptr;
    const float* runtimeUVs = nullptr;
    int runtimeUVCount = 0;
    int vertexDimension = 0;
    int vertexCount = 0;
    const unsigned int* indices = nullptr;
    int indexCount = 0;

    if (m_HasRuntimeMesh) {
        vertices = m_RuntimeVertices.data();
        vertexDimension = m_RuntimeVertexDimension;
        vertexCount = m_RuntimeVertexCount;
        indices = m_RuntimeIndices.data();
        indexCount = m_RuntimeIndexCount;
        runtimeUVs = m_RuntimeUVs.empty() ? nullptr : m_RuntimeUVs.data();
        runtimeUVCount = m_RuntimeUVCount;
    } else {
        mesh = m_AssetManager.GetHandle<MeshAsset>(m_MeshAssetPath);
        if (mesh == nullptr) {
            return;
        }

        vertices = mesh->GetVertices();
        vertexDimension = mesh->GetVertexDimension();
        vertexCount = mesh->GetVertexCount();
        indices = mesh->GetIndices();
        indexCount = mesh->GetIndexCount();
    }

    if (vertices == nullptr || indices == nullptr || vertexCount < 3 || (vertexDimension != 2 && vertexDimension != 3) || indexCount < 3 || (indexCount % 3) != 0) {
        return;
    }

    std::vector<float> world(static_cast<std::size_t>(vertexCount * vertexDimension), 0.0f);
    for (int i = 0; i < vertexCount; ++i) {
        const int base = i * vertexDimension;
        const float x = vertices[base];
        const float y = vertices[base + 1];

        world[base] = x;
        world[base + 1] = (m_Canvas.originAtCenter ? -y : (m_Canvas.worldHeight - y));
        if (vertexDimension == 3) {
            world[base + 2] = vertices[base + 2];
        }
    }

    const float* activeUVs = nullptr;
    int activeUVCount = 0;
    std::vector<float> generatedUVs;

    if (m_HasRuntimeMesh) {
        if (runtimeUVs != nullptr && runtimeUVCount == vertexCount) {
            activeUVs = runtimeUVs;
            activeUVCount = runtimeUVCount;
        } else if (m_UVControlMode == UVControlMode::RequireAsset) {
            XLOG_ERROR("Renderer2DFeature: runtime mesh requires explicit UVs but none were provided");
            return;
        }
    } else if (m_UVControlMode != UVControlMode::AutoGenerate) {
        const float* meshUVs = mesh->GetUVs();
        const int meshUVCount = mesh->GetUVCount();
        if (mesh->HasExplicitUVs() && meshUVs != nullptr && meshUVCount == vertexCount) {
            activeUVs = meshUVs;
            activeUVCount = meshUVCount;
        } else if (m_UVControlMode == UVControlMode::RequireAsset) {
            XLOG_ERROR("Renderer2DFeature: mesh requires explicit UVs but none were provided");
            return;
        }
    }

    const SpriteAsset* sprite = nullptr;
    if (!m_SpriteAssetPath.empty() && std::filesystem::exists(m_SpriteAssetPath)) {
        sprite = m_AssetManager.GetHandle<SpriteAsset>(m_SpriteAssetPath);
    }

    if (sprite != nullptr && !m_HasRuntimeMesh) {
        generatedUVs.resize(static_cast<std::size_t>(vertexCount * 2), 0.0f);

        if (activeUVs != nullptr && activeUVCount == vertexCount) {
            for (int i = 0; i < vertexCount; ++i) {
                generatedUVs[static_cast<std::size_t>(i) * 2] = activeUVs[static_cast<std::size_t>(i) * 2];
                generatedUVs[static_cast<std::size_t>(i) * 2 + 1] = activeUVs[static_cast<std::size_t>(i) * 2 + 1];
            }
        } else {
            float minX = world[0];
            float maxX = world[0];
            float minY = world[1];
            float maxY = world[1];
            for (int i = 1; i < vertexCount; ++i) {
                const int base = i * vertexDimension;
                const float x = world[base];
                const float y = world[base + 1];
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }

            float rangeX = maxX - minX;
            float rangeY = maxY - minY;
            if (rangeX == 0.0f) {
                rangeX = 1.0f;
            }
            if (rangeY == 0.0f) {
                rangeY = 1.0f;
            }

            for (int i = 0; i < vertexCount; ++i) {
                const int base = i * vertexDimension;
                const float x = world[base];
                const float y = world[base + 1];
                generatedUVs[static_cast<std::size_t>(i) * 2] = (x - minX) / rangeX;
                generatedUVs[static_cast<std::size_t>(i) * 2 + 1] = (y - minY) / rangeY;
            }
        }

        std::array<float, 4> atlasUVRect = {0.0f, 0.0f, 1.0f, 1.0f};
        const float* uvRect = sprite->GetUVRect();
        if (sprite->UsesAtlasRegion() && !m_ResolvedAtlasAssetPath.empty()) {
            const AtlasAsset* atlas = m_AssetManager.GetHandle<AtlasAsset>(m_ResolvedAtlasAssetPath);
            if (atlas != nullptr && atlas->TryGetRegionUVRect(sprite->GetAtlasRegionName(), atlasUVRect)) {
                uvRect = atlasUVRect.data();
            }
        }

        if (uvRect != nullptr) {
            const float uMin = uvRect[0];
            const float vMin = uvRect[1];
            const float uMax = uvRect[2];
            const float vMax = uvRect[3];
            const float uRange = uMax - uMin;
            const float vRange = vMax - vMin;

            for (int i = 0; i < vertexCount; ++i) {
                float& u = generatedUVs[static_cast<std::size_t>(i) * 2];
                float& v = generatedUVs[static_cast<std::size_t>(i) * 2 + 1];
                u = uMin + u * uRange;
                v = vMin + v * vRange;
            }
        }

        activeUVs = generatedUVs.data();
        activeUVCount = vertexCount;
    }

    backend.UploadMesh(world.data(), vertexCount, vertexDimension, activeUVs, activeUVCount, indices, indexCount);
}

} // namespace Engine
