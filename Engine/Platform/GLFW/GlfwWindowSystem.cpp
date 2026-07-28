#include "GlfwWindowSystem.h"

#include "Input/InputEventQueue.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace Engine {
namespace {

InputEventQueue* GetInputQueue(GLFWwindow* window) {
    return static_cast<InputEventQueue*>(glfwGetWindowUserPointer(window));
}

InputModifiers ToInputModifiers(int modifiers) {
    std::uint8_t result = 0;
    if ((modifiers & GLFW_MOD_SHIFT) != 0) result |= static_cast<std::uint8_t>(InputModifiers::Shift);
    if ((modifiers & GLFW_MOD_CONTROL) != 0) result |= static_cast<std::uint8_t>(InputModifiers::Control);
    if ((modifiers & GLFW_MOD_ALT) != 0) result |= static_cast<std::uint8_t>(InputModifiers::Alt);
    if ((modifiers & GLFW_MOD_SUPER) != 0) result |= static_cast<std::uint8_t>(InputModifiers::Super);
    return static_cast<InputModifiers>(result);
}

InputKeyCode ToInputKeyCode(int key) {
    switch (key) {
    case GLFW_KEY_ESCAPE: return InputKeyCode::Escape;
    case GLFW_KEY_ENTER: return InputKeyCode::Enter;
    case GLFW_KEY_TAB: return InputKeyCode::Tab;
    case GLFW_KEY_BACKSPACE: return InputKeyCode::Backspace;
    case GLFW_KEY_SPACE: return InputKeyCode::Space;
    case GLFW_KEY_LEFT: return InputKeyCode::Left;
    case GLFW_KEY_RIGHT: return InputKeyCode::Right;
    case GLFW_KEY_UP: return InputKeyCode::Up;
    case GLFW_KEY_DOWN: return InputKeyCode::Down;
    default: return InputKeyCode::Unknown;
    }
}

PointerButton ToPointerButton(int button) {
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT: return PointerButton::Primary;
    case GLFW_MOUSE_BUTTON_RIGHT: return PointerButton::Secondary;
    case GLFW_MOUSE_BUTTON_MIDDLE: return PointerButton::Middle;
    default: return PointerButton::Other;
    }
}

void InstallInputCallbacks(GLFWwindow* window) {
    glfwSetCursorPosCallback(window, [](GLFWwindow* native, double x, double y) {
        if (InputEventQueue* queue = GetInputQueue(native)) {
            (void)queue->Push(PointerMoveEvent{
                queue->Window(), {static_cast<float>(x), static_cast<float>(y)},
                InputModifiers::None, 0, glfwGetTime()});
        }
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* native, int button, int action, int modifiers) {
        if (InputEventQueue* queue = GetInputQueue(native)) {
            double x = 0.0;
            double y = 0.0;
            glfwGetCursorPos(native, &x, &y);
            (void)queue->Push(PointerButtonEvent{
                queue->Window(),
                {static_cast<float>(x), static_cast<float>(y)},
                ToPointerButton(button),
                action != GLFW_RELEASE,
                ToInputModifiers(modifiers), 0, glfwGetTime()});
        }
    });
    glfwSetScrollCallback(window, [](GLFWwindow* native, double xOffset, double yOffset) {
        if (InputEventQueue* queue = GetInputQueue(native)) {
            double x = 0.0;
            double y = 0.0;
            glfwGetCursorPos(native, &x, &y);
            (void)queue->Push(PointerWheelEvent{
                queue->Window(),
                {static_cast<float>(x), static_cast<float>(y)},
                {static_cast<float>(xOffset), static_cast<float>(yOffset)},
                InputModifiers::None, 0, glfwGetTime()});
        }
    });
    glfwSetCursorEnterCallback(window, [](GLFWwindow* native, int entered) {
        if (entered == GLFW_FALSE) {
            if (InputEventQueue* queue = GetInputQueue(native)) {
                (void)queue->Push(PointerLeaveEvent{queue->Window(), 0, glfwGetTime()});
            }
        }
    });
    glfwSetKeyCallback(window, [](GLFWwindow* native, int key, int, int action, int modifiers) {
        if (InputEventQueue* queue = GetInputQueue(native)) {
            (void)queue->Push(KeyEvent{
                queue->Window(),
                ToInputKeyCode(key),
                action != GLFW_RELEASE,
                action == GLFW_REPEAT,
                ToInputModifiers(modifiers), glfwGetTime()});
        }
    });
    glfwSetCharCallback(window, [](GLFWwindow* native, unsigned int codepoint) {
        if (InputEventQueue* queue = GetInputQueue(native)) {
            (void)queue->Push(TextInputEvent{queue->Window(), static_cast<char32_t>(codepoint), glfwGetTime()});
        }
    });
    glfwSetWindowFocusCallback(window, [](GLFWwindow* native, int focused) {
        if (InputEventQueue* queue = GetInputQueue(native)) {
            (void)queue->Push(WindowFocusEvent{queue->Window(), focused == GLFW_TRUE, glfwGetTime()});
        }
    });
}

void RemoveInputCallbacks(GLFWwindow* window) {
    glfwSetCursorPosCallback(window, nullptr);
    glfwSetMouseButtonCallback(window, nullptr);
    glfwSetScrollCallback(window, nullptr);
    glfwSetCursorEnterCallback(window, nullptr);
    glfwSetKeyCallback(window, nullptr);
    glfwSetCharCallback(window, nullptr);
    glfwSetWindowFocusCallback(window, nullptr);
    glfwSetWindowUserPointer(window, nullptr);
}

} // namespace

GlfwWindowSystem::~GlfwWindowSystem() {
    for (Slot& slot : m_Slots) {
        if (slot.window != nullptr) {
            glfwDestroyWindow(slot.window);
            slot.window = nullptr;
            ++slot.generation;
        }
    }
}

WindowHandle GlfwWindowSystem::CreateWindow(const WindowDesc& desc) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, desc.decorated ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, desc.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, desc.alwaysOnTop ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, desc.transparent ? GLFW_TRUE : GLFW_FALSE);

    const char* title = desc.title.empty() ? "XiE Window" : desc.title.c_str();
    GLFWwindow* window = glfwCreateWindow(desc.width, desc.height, title, nullptr, nullptr);
    if (window == nullptr) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    auto freeIt = std::find_if(m_Slots.begin(), m_Slots.end(), [](const Slot& slot) {
        return slot.window == nullptr;
    });

    std::uint32_t slotIndex = 0;
    Slot* slot = nullptr;
    if (freeIt == m_Slots.end()) {
        m_Slots.push_back({});
        slotIndex = static_cast<std::uint32_t>(m_Slots.size());
        slot = &m_Slots.back();
    } else {
        slotIndex = static_cast<std::uint32_t>(std::distance(m_Slots.begin(), freeIt) + 1);
        slot = &(*freeIt);
    }

    slot->window = window;
    slot->visible = desc.visible;
    if (slot->generation == 0) {
        slot->generation = 1;
    }

    return WindowHandle{slotIndex, slot->generation};
}

void GlfwWindowSystem::DestroyWindow(WindowHandle window) {
    Slot* slot = Resolve(window);
    if (slot == nullptr) {
        return;
    }

    RemoveInputCallbacks(slot->window);
    glfwDestroyWindow(slot->window);
    slot->window = nullptr;
    slot->visible = false;
    ++slot->generation;
    if (slot->generation == 0) {
        slot->generation = 1;
    }
}

void GlfwWindowSystem::ShowWindow(WindowHandle window) {
    Slot* slot = Resolve(window);
    if (slot == nullptr) {
        return;
    }
    glfwShowWindow(slot->window);
    slot->visible = true;
}

void GlfwWindowSystem::HideWindow(WindowHandle window) {
    Slot* slot = Resolve(window);
    if (slot == nullptr) {
        return;
    }
    glfwHideWindow(slot->window);
    slot->visible = false;
}

void GlfwWindowSystem::SetWindowPosition(WindowHandle window, int x, int y) {
    if (GLFWwindow* native = GetGlfwWindow(window)) {
        glfwSetWindowPos(native, x, y);
    }
}

void GlfwWindowSystem::SetWindowSize(WindowHandle window, int width, int height) {
    if (GLFWwindow* native = GetGlfwWindow(window)) {
        glfwSetWindowSize(native, width, height);
    }
}

bool GlfwWindowSystem::IsVisible(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    return slot != nullptr && slot->visible;
}

bool GlfwWindowSystem::IsCloseRequested(WindowHandle window) const {
    if (GLFWwindow* native = GetGlfwWindow(window)) {
        return glfwWindowShouldClose(native) == GLFW_TRUE;
    }
    return false;
}

float GlfwWindowSystem::GetDpiScale(WindowHandle window) const {
    if (GLFWwindow* native = GetGlfwWindow(window)) {
        float xScale = 1.0f;
        float yScale = 1.0f;
        glfwGetWindowContentScale(native, &xScale, &yScale);
        return (xScale + yScale) * 0.5f;
    }
    return 1.0f;
}

NativeWindowHandle GlfwWindowSystem::GetNativeHandle(WindowHandle window) const {
    return reinterpret_cast<NativeWindowHandle>(GetGlfwWindow(window));
}

bool GlfwWindowSystem::SetInputEventQueue(WindowHandle window, InputEventQueue* queue) {
    GLFWwindow* native = GetGlfwWindow(window);
    if (native == nullptr || (queue != nullptr && queue->Window() != window)) {
        return false;
    }
    if (queue == nullptr) {
        RemoveInputCallbacks(native);
    } else {
        glfwSetWindowUserPointer(native, queue);
        InstallInputCallbacks(native);
    }
    return true;
}

GLFWwindow* GlfwWindowSystem::GetGlfwWindow(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    return slot != nullptr ? slot->window : nullptr;
}

GlfwWindowSystem::Slot* GlfwWindowSystem::Resolve(WindowHandle window) {
    return const_cast<Slot*>(static_cast<const GlfwWindowSystem*>(this)->Resolve(window));
}

const GlfwWindowSystem::Slot* GlfwWindowSystem::Resolve(WindowHandle window) const {
    if (!window.IsValid() || window.index > m_Slots.size()) {
        return nullptr;
    }

    const Slot& slot = m_Slots[window.index - 1];
    if (slot.window == nullptr || slot.generation != window.generation) {
        return nullptr;
    }
    return &slot;
}

} // namespace Engine
