#include "Window.h"

namespace Engine {

Window::Window(WindowHandle handle)
    : m_Handle(handle) {}

WindowHandle Window::GetHandle() const {
    return m_Handle;
}

} // namespace Engine
