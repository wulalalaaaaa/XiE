#pragma once

#include "Platform/IInputState.h"
#include "Platform/WindowHandle.h"

namespace Engine {

class GlfwWindowSystem;

class GlfwInputState final : public IInputState {
public:
    GlfwInputState(const GlfwWindowSystem& windows, WindowHandle window);

    bool IsKeyDown(KeyCode key) const override;

private:
    const GlfwWindowSystem* m_Windows = nullptr;
    WindowHandle m_Window;
};

} // namespace Engine
