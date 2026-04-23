#include "Renderer.h"

#include "Core/Log.h"
#include "Feature/IRenderFeature.h"
#include "Feature/Renderer2DFeature.h"
#include "Feature/Renderer3DFeature.h"
#include "OpenGLBackend.h"

namespace Engine {

template <bool Debug>
bool BasicRenderer<Debug>::Init(GLFWwindow* windowHandle, const std::filesystem::path& meshFilePath) {
    m_WindowHandle = windowHandle;
    m_MeshFilePath = meshFilePath;

    m_Backend = std::make_unique<OpenGLBackend>();
    if (!m_Backend->Init(windowHandle)) {
        XLOG_ERROR("Failed to initialize render backend");
        return false;
    }

    return InitFeature();
}

template <bool Debug>
bool BasicRenderer<Debug>::InitFeature() {
    switch (m_Mode) {
    case Mode::Mode2D: {
        auto feature2D = std::make_unique<Renderer2DFeature>();
        feature2D->SetMeshAssetPath(m_MeshFilePath);
        m_Feature = std::move(feature2D);
        break;
    }
    case Mode::Mode3D:
        m_Feature = std::make_unique<Renderer3DFeature>();
        break;
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

    if (m_Backend) {
        m_Backend->Shutdown();
        m_Backend.reset();
    }

    m_WindowHandle = nullptr;
}

// explicit template instantiations for DLL build
template class BasicRenderer<true>;
template class BasicRenderer<false>;

} // namespace Engine
