#include "Renderer.h"

#include "Core/Log.h"
#include "OpenGLBackend.h"

#include <GLFW/glfw3.h>

namespace Engine {

template <bool Debug>
bool BasicRenderer<Debug>::Init(GLFWwindow* windowHandle, const std::filesystem::path& triangleFilePath) {
    m_WindowHandle = windowHandle;

    m_Backend = std::make_unique<OpenGLBackend>();
    if (!m_Backend->Init(windowHandle)) {
        XLOG_ERROR("Failed to initialize render backend");
        return false;
    }

    if (!m_TriangleAsset.LoadFromFile(triangleFilePath)) {
        return false;
    }

    UpdateViewport();
    UpdateTriangleBuffer();
    return true;
}

template <bool Debug>
void BasicRenderer<Debug>::UpdateViewport() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_WindowHandle, &width, &height);
    m_Backend->SetViewport(width, height);
}

template <bool Debug>
void BasicRenderer<Debug>::UpdateTriangleBuffer() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_WindowHandle, &width, &height);

    const auto& verticesNdc = m_TriangleAsset.GetVerticesNdc(width, height);
    m_Backend->UploadTriangleVertices(verticesNdc.data(), static_cast<int>(verticesNdc.size()));
}

template <bool Debug>
void BasicRenderer<Debug>::BeginFrame() {
    if constexpr (Debug) {
        if (m_TriangleAsset.ReloadIfChanged()) {
            UpdateTriangleBuffer();
        }
    }

    UpdateViewport();
    m_Backend->BeginFrame();
    m_Backend->DrawTriangle();
}

template <bool Debug>
void BasicRenderer<Debug>::EndFrame() {
    m_Backend->EndFrame();
}

template <bool Debug>
void BasicRenderer<Debug>::Shutdown() {
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
