#pragma once

#include "Platform/WindowHandle.h"

#include <memory>

namespace Engine {

class DrawList2D;
class IInputState;
class IRuntimeRender2D {
public:
    virtual ~IRuntimeRender2D() = default;

    virtual bool SubmitRuntimeMesh2D(
        const float* vertices,
        int vertexCount,
        int vertexDimension,
        const float* uvs,
        int uvCount,
        const unsigned int* indices,
        int indexCount
    ) = 0;
    virtual void SubmitDrawList2D(const DrawList2D& drawList) = 0;
    virtual void ClearRuntimeMesh2D() = 0;
    virtual void SetCamera2D(float centerX, float centerY, float zoom) = 0;
};

struct FrameContext {
    WindowHandle Window = {};
    const IInputState* Input = nullptr;
    IRuntimeRender2D* RuntimeRender2D = nullptr;
};

class IGameApp {
public:
    virtual ~IGameApp() = default;

    virtual void OnInit(const FrameContext& context) = 0;
    virtual void OnUpdate(float dt, const FrameContext& context) = 0;
    virtual void OnRender(const FrameContext& context) = 0;
    virtual void OnShutdown(const FrameContext& context) = 0;
};

struct GameStartupDesc {
    const char* GameName = "XiE Game";
    const char* ProjectRoot = "";
    const char* AssetRoot = "";
    int WindowWidth = 1280;
    int WindowHeight = 720;
    // 0 = AutoGenerate, 1 = PreferAsset, 2 = RequireAsset.
    int UVMode = 1;
};

} // namespace Engine
