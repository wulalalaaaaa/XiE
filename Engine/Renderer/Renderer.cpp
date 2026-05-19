#include "Renderer.h"

#include "Core/Log.h"
#include "Feature/IRenderFeature.h"
#include "Feature/Renderer2DFeature.h"
#include "Feature/Renderer3DFeature.h"
#include "OpenGLBackend.h"

namespace Engine {

template <bool Debug>
bool BasicRenderer<Debug>::Init(
    GLFWwindow* windowHandle,
    const std::filesystem::path& meshFilePath,
    const std::filesystem::path& textureFilePath,
    const std::filesystem::path& materialFilePath,
    const std::filesystem::path& spriteFilePath,
    int uvMode
) {
    m_WindowHandle = windowHandle;
    m_MeshFilePath = meshFilePath;
    m_TextureFilePath = textureFilePath;
    m_MaterialFilePath = materialFilePath;
    m_SpriteFilePath = spriteFilePath;
    m_UVMode = uvMode;

    m_Backend = std::make_unique<OpenGLBackend>();
    if (!m_Backend->Init(windowHandle)) {
        XLOG_ERROR("Failed to initialize render backend");
        return false;
    }

    return InitFeature();
}

template <bool Debug>
bool BasicRenderer<Debug>::InitFeature() {
    m_Feature2D = nullptr;

    switch (m_Mode) {
    case Mode::Mode2D: {
        auto feature2D = std::make_unique<Renderer2DFeature>();
        feature2D->SetMeshAssetPath(m_MeshFilePath);
        feature2D->SetTextureAssetPath(m_TextureFilePath);
        feature2D->SetMaterialAssetPath(m_MaterialFilePath);
        feature2D->SetSpriteAssetPath(m_SpriteFilePath);
        switch (m_UVMode) {
        case 0:
            feature2D->SetUVControlMode(Renderer2DFeature::UVControlMode::AutoGenerate);
            break;
        case 2:
            feature2D->SetUVControlMode(Renderer2DFeature::UVControlMode::RequireAsset);
            break;
        case 1:
        default:
            feature2D->SetUVControlMode(Renderer2DFeature::UVControlMode::PreferAsset);
            break;
        }
        m_Feature2D = feature2D.get();
        m_Feature = std::move(feature2D);
        break;
    }
    case Mode::Mode3D: {
        auto feature3D = std::make_unique<Renderer3DFeature>();
        feature3D->SetMeshAssetPath(m_MeshFilePath);
        m_Feature = std::move(feature3D);
        break;
    }
    default:
        XLOG_ERROR("Unknown renderer mode");
        return false;
    }

    if (!m_Feature->Init(*m_Backend, m_WindowHandle)) {
        XLOG_ERROR("Failed to initialize render feature");
        return false;
    }

    return true;
}

template <bool Debug>
void BasicRenderer<Debug>::Tick(float dt) {
    if (m_Feature) {
        m_Feature->OnUpdate(*m_Backend, m_WindowHandle, dt);
    }
}

template <bool Debug>
void BasicRenderer<Debug>::BeginFrame() {
    if (m_Feature) {
        m_Feature->OnRender(*m_Backend, m_WindowHandle);
    }
}

template <bool Debug>
void BasicRenderer<Debug>::EndFrame() {}

template <bool Debug>
void BasicRenderer<Debug>::Shutdown() {
    if (m_Feature && m_Backend) {
        m_Feature->Shutdown(*m_Backend);
        m_Feature.reset();
    }
    m_Feature2D = nullptr;

    if (m_Backend) {
        m_Backend->Shutdown();
        m_Backend.reset();
    }

    m_WindowHandle = nullptr;
}

template <bool Debug>
bool BasicRenderer<Debug>::SubmitRuntimeMesh2D(
    const float* vertices,
    int vertexCount,
    int vertexDimension,
    const float* uvs,
    int uvCount,
    const unsigned int* indices,
    int indexCount
) {
    if (m_Feature2D == nullptr) {
        return false;
    }

    return m_Feature2D->SubmitRuntimeMesh(vertices, vertexCount, vertexDimension, uvs, uvCount, indices, indexCount);
}

template <bool Debug>
void BasicRenderer<Debug>::ClearRuntimeMesh2D() {
    if (m_Feature2D == nullptr) {
        return;
    }

    m_Feature2D->ClearRuntimeMesh();
}

// explicit template instantiations for DLL build
template class BasicRenderer<true>;
template class BasicRenderer<false>;

} // namespace Engine
