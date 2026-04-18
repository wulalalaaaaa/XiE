#pragma once

#include "TriangleAsset.h"
#include "IRenderBackend.h"
#include <filesystem>
#include <memory>

struct GLFWwindow;

namespace Engine {

template <bool Debug>
class BasicRenderer {
    public:
        bool Init(GLFWwindow* windowHandle, const std::filesystem::path& triangleFilePath);
        void BeginFrame();
        void EndFrame();
        void Shutdown();

    private:
        void UpdateViewport();
        void UpdateTriangleBuffer();

    private:
        GLFWwindow* m_WindowHandle = nullptr;
        TriangleAsset m_TriangleAsset;
        std::unique_ptr<IRenderBackend> m_Backend;
};

using Renderer = BasicRenderer<(XIE_DEBUG != 0)>;

} // namespace Engine
