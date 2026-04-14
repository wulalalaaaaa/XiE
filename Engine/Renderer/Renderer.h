#pragma once

namespace Engine {

class Renderer {
public:
    void Init();
    void BeginFrame();
    void EndFrame();
    void Shutdown();
};

} // namespace Engine
