#pragma once

#include "WindowHandle.h"

namespace Engine {

class Window {
public:
    Window() = default;
    explicit Window(WindowHandle handle);

    [[nodiscard]] WindowHandle GetHandle() const;

private:
    WindowHandle m_Handle{};
};

} // namespace Engine
