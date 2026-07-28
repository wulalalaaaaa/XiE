#pragma once

namespace Engine {

class IRenderBackend;
class IRenderSurface;

class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;

    virtual bool Init(IRenderBackend& backend, IRenderSurface& surface) = 0;
    virtual void OnUpdate(IRenderBackend& backend, IRenderSurface& surface, float dt) = 0;
    virtual void OnRender(IRenderBackend& backend, IRenderSurface& surface) = 0;
    virtual void Shutdown(IRenderBackend& backend) = 0;
};

} // namespace Engine
