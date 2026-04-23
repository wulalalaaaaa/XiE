#pragma once

#include "IRenderBackend.h"
#include "Feature/IRenderFeature.h"

#include <filesystem>
#include <memory>

struct GLFWwindow;

namespace Engine {

template <bool Debug>
class BasicRenderer {
public:
    enum class Mode {
        Mode2D,
        Mode3D
    };

    bool Init(GLFWwindow* windowHandle, const std::filesystem::path& meshFilePath);
    void Tick(float dt);
    void BeginFrame();
    void EndFrame();
    void Shutdown();

private:
    bool InitFeature();

private:
    GLFWwindow* m_WindowHandle = nullptr;
    Mode m_Mode = Mode::Mode2D;

    std::filesystem::path m_MeshFilePath;

    std::unique_ptr<IRenderBackend> m_Backend;
    std::unique_ptr<IRenderFeature> m_Feature;
};

using Renderer = BasicRenderer<(XIE_DEBUG != 0)>;

} // namespace Engine
