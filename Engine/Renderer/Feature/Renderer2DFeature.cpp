#include "Renderer2DFeature.h"

#include "Core/Log.h"
#include "Renderer/IRenderBackend.h"

#include <string>
#include <vector>

#include <GLFW/glfw3.h>

namespace Engine {

void Renderer2DFeature::SetMeshAssetPath(const std::filesystem::path& path) {
    m_MeshAssetPath = path;
}

bool Renderer2DFeature::Init(IRenderBackend& backend, GLFWwindow* windowHandle) {
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

    UploadMesh(backend, windowHandle);
    return true;
}

void Renderer2DFeature::OnUpdate(IRenderBackend& backend, GLFWwindow* windowHandle, float dt) {
    m_Controller.OnUpdate(windowHandle, m_Camera, dt);

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(windowHandle, &width, &height);
    m_Camera.SetViewportSize(width, height);

    if constexpr (XIE_DEBUG != 0) {
        if (m_AssetManager.ReloadIfChanged<MeshAsset>(
                m_MeshAssetPath,
                [](const std::filesystem::path& path, MeshAsset& asset) {
                    return asset.LoadFromFile(path);
                })) {
            UploadMesh(backend, windowHandle);
        }
    }
}

void Renderer2DFeature::OnRender(IRenderBackend& backend, GLFWwindow* windowHandle) {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(windowHandle, &width, &height);
    backend.SetViewport(width, height);

    const auto vp = m_Camera.GetViewProjection();
    backend.SetViewProjection(vp.data());

    backend.BeginFrame();
    backend.DrawMesh();
    backend.EndFrame();
}

void Renderer2DFeature::Shutdown(IRenderBackend& backend) {
    (void)backend;
}

void Renderer2DFeature::UploadMesh(IRenderBackend& backend, GLFWwindow* windowHandle) {
    (void)windowHandle;

    const MeshAsset* mesh = m_AssetManager.GetHandle<MeshAsset>(m_MeshAssetPath);
    if (mesh == nullptr) {
        return;
    }

    const float* pixels = mesh->GetVertices2D();
    const int vertexFloatCount = mesh->GetVertexFloatCount();
    const unsigned int* indices = mesh->GetIndices();
    const int indexCount = mesh->GetIndexCount();
    if (pixels == nullptr || indices == nullptr || vertexFloatCount < 6 || (vertexFloatCount % 2) != 0 || indexCount < 3 || (indexCount % 3) != 0) {
        return;
    }

    std::vector<float> world(static_cast<std::size_t>(vertexFloatCount), 0.0f);
    const int vertexCount = vertexFloatCount / 2;
    for (int i = 0; i < vertexCount; ++i) {
        const float x = pixels[2 * i];
        const float y = pixels[2 * i + 1];

        world[2 * i] = x;
        world[2 * i + 1] = (m_Canvas.originAtCenter ? -y : (m_Canvas.worldHeight - y));
    }

    backend.UploadMesh(world.data(), vertexFloatCount, indices, indexCount);
}

} // namespace Engine
