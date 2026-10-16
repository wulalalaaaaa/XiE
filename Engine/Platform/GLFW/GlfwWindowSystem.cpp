#include "GlfwWindowSystem.h"

#include "Input/InputEventQueue.h"
#include "Input/InputDiagnostics.h"

#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <GLFW/glfw3native.h>
#include <Windows.h>
#include <imm.h>
#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef DestroyWindow
#undef DestroyWindow
#endif
#endif

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

namespace Engine {
namespace {

InputEventQueue* GetInputQueue(GLFWwindow* window) {
    return static_cast<InputEventQueue*>(glfwGetWindowUserPointer(window));
}

struct GlfwPointerState {
    Vec2F logicalPosition{};
    bool hasPosition = false;
};

std::unordered_map<GLFWwindow*, GlfwPointerState> g_PointerStates;

Vec2F CurrentPointerPosition(GLFWwindow* window) {
    const auto found = g_PointerStates.find(window);
    if (found != g_PointerStates.end() && found->second.hasPosition)
        return found->second.logicalPosition;
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    return {static_cast<float>(x), static_cast<float>(y)};
}

template <typename T>
InputDiagnosticKind NativeKind(const T& value) {
    if constexpr (std::is_same_v<T, PointerMoveEvent>)
        return InputDiagnosticKind::PointerMove;
    else if constexpr (std::is_same_v<T, PointerButtonEvent>)
        return value.pressed ? InputDiagnosticKind::PointerDown
                             : InputDiagnosticKind::PointerUp;
    else if constexpr (std::is_same_v<T, PointerWheelEvent>)
        return InputDiagnosticKind::PointerWheel;
    else if constexpr (std::is_same_v<T, PointerLeaveEvent>)
        return InputDiagnosticKind::PointerLeave;
    else if constexpr (std::is_same_v<T, KeyEvent>)
        return value.pressed ? InputDiagnosticKind::KeyDown
                             : InputDiagnosticKind::KeyUp;
    else if constexpr (std::is_same_v<T, TextInputEvent>)
        return InputDiagnosticKind::TextInput;
    else if constexpr (std::is_same_v<T, TextCompositionStartEvent>)
        return InputDiagnosticKind::CompositionStart;
    else if constexpr (std::is_same_v<T, TextCompositionUpdateEvent>)
        return InputDiagnosticKind::CompositionUpdate;
    else if constexpr (std::is_same_v<T, TextCompositionCommitEvent>)
        return InputDiagnosticKind::CompositionCommit;
    else if constexpr (std::is_same_v<T, TextCompositionEndEvent>)
        return InputDiagnosticKind::CompositionEnd;
    else
        return value.focused ? InputDiagnosticKind::FocusGained
                             : InputDiagnosticKind::FocusLost;
}

template <typename T>
void PushNative(InputEventQueue& queue, T event) {
    EmitInputDiagnostic({InputDiagnosticStage::Native,
        NativeKind(event), queue.Window()});
    (void)queue.Push(InputEvent{std::move(event)});
}

#ifdef _WIN32
struct GlfwImeHook {
    GLFWwindow* window = nullptr;
    WNDPROC previous = nullptr;
    std::size_t suppressCommittedScalars = 0;
};
std::unordered_map<HWND, GlfwImeHook> g_ImeHooks;

double WinInputTimestampSeconds() {
    return static_cast<double>(GetTickCount64()) / 1000.0;
}

void RecordWin32PointerPosition(
    GLFWwindow* window, HWND hwnd, UINT message, LPARAM lParam) {
    POINT point{
        static_cast<short>(LOWORD(lParam)),
        static_cast<short>(HIWORD(lParam))};
    if (message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL)
        (void)ScreenToClient(hwnd, &point);
    auto& pointer = g_PointerStates[window];
    pointer.logicalPosition = {
        static_cast<float>(point.x), static_cast<float>(point.y)};
    pointer.hasPosition = true;
}

std::string WideToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), result.data(), count, nullptr, nullptr);
    return result;
}

std::wstring ReadImeString(HIMC context, DWORD kind) {
    const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
    if (bytes <= 0) return {};
    std::wstring result(
        static_cast<std::size_t>(bytes) / sizeof(wchar_t), L'\0');
    const LONG read = ImmGetCompositionStringW(
        context, kind, result.data(), bytes);
    if (read < 0) return {};
    result.resize(static_cast<std::size_t>(read) / sizeof(wchar_t));
    return result;
}

std::size_t WideScalarIndex(
    std::wstring_view text, std::size_t codeUnitIndex) {
    std::size_t scalar = 0;
    for (std::size_t i = 0;
         i < std::min(codeUnitIndex, text.size()); ++i) {
        if (text[i] >= 0xD800 && text[i] <= 0xDBFF &&
            i + 1 < text.size() &&
            text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) ++i;
        ++scalar;
    }
    return scalar;
}

LRESULT CALLBACK GlfwImeWindowProc(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    const auto found = g_ImeHooks.find(hwnd);
    if (found == g_ImeHooks.end())
        return DefWindowProcW(hwnd, message, wParam, lParam);
    switch (message) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
        RecordWin32PointerPosition(
            found->second.window, hwnd, message, lParam);
        break;
    default:
        break;
    }
    if (InputEventQueue* queue = GetInputQueue(found->second.window)) {
        if (message == WM_IME_STARTCOMPOSITION) {
            found->second.suppressCommittedScalars = 0;
            PushNative(*queue, TextCompositionStartEvent{
                queue->Window(), WinInputTimestampSeconds()});
        } else if (message == WM_IME_COMPOSITION) {
            if (HIMC context = ImmGetContext(hwnd)) {
                if ((lParam & GCS_RESULTSTR) != 0) {
                    const std::wstring result =
                        ReadImeString(context, GCS_RESULTSTR);
                    found->second.suppressCommittedScalars =
                        WideScalarIndex(result, result.size());
                    PushNative(*queue, TextCompositionCommitEvent{
                        queue->Window(),
                        WideToUtf8(result),
                        WinInputTimestampSeconds()});
                }
                if ((lParam & GCS_COMPSTR) != 0) {
                    const std::wstring value =
                        ReadImeString(context, GCS_COMPSTR);
                    const LONG cursor = ImmGetCompositionStringW(
                        context, GCS_CURSORPOS, nullptr, 0);
                    PushNative(*queue, TextCompositionUpdateEvent{
                        queue->Window(), WideToUtf8(value),
                        WideScalarIndex(value, cursor >= 0
                            ? static_cast<std::size_t>(cursor) : 0),
                        std::nullopt, WinInputTimestampSeconds()});
                }
                ImmReleaseContext(hwnd, context);
            }
        } else if (message == WM_IME_ENDCOMPOSITION) {
            PushNative(*queue, TextCompositionEndEvent{
                queue->Window(), WinInputTimestampSeconds()});
        }
    }
    const WNDPROC previous = found->second.previous;
    const LRESULT result =
        CallWindowProcW(previous, hwnd, message, wParam, lParam);
    return result;
}

void InstallImeHook(GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd || g_ImeHooks.contains(hwnd)) return;
    const auto previous = reinterpret_cast<WNDPROC>(
        GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
    g_ImeHooks.emplace(hwnd, GlfwImeHook{window, previous, 0});
    SetWindowLongPtrW(
        hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(GlfwImeWindowProc));
}

void RemoveImeHook(GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    const auto found = g_ImeHooks.find(hwnd);
    if (found == g_ImeHooks.end()) return;
    SetWindowLongPtrW(
        hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(found->second.previous));
    g_ImeHooks.erase(found);
}
#endif

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
    case GLFW_KEY_DELETE: return InputKeyCode::Delete;
    case GLFW_KEY_SPACE: return InputKeyCode::Space;
    case GLFW_KEY_INSERT: return InputKeyCode::Insert;
    case GLFW_KEY_HOME: return InputKeyCode::Home;
    case GLFW_KEY_END: return InputKeyCode::End;
    case GLFW_KEY_PAGE_UP: return InputKeyCode::PageUp;
    case GLFW_KEY_PAGE_DOWN: return InputKeyCode::PageDown;
    case GLFW_KEY_LEFT: return InputKeyCode::Left;
    case GLFW_KEY_RIGHT: return InputKeyCode::Right;
    case GLFW_KEY_UP: return InputKeyCode::Up;
    case GLFW_KEY_DOWN: return InputKeyCode::Down;
    case GLFW_KEY_0: return InputKeyCode::Digit0;
    case GLFW_KEY_1: return InputKeyCode::Digit1;
    case GLFW_KEY_2: return InputKeyCode::Digit2;
    case GLFW_KEY_3: return InputKeyCode::Digit3;
    case GLFW_KEY_4: return InputKeyCode::Digit4;
    case GLFW_KEY_5: return InputKeyCode::Digit5;
    case GLFW_KEY_6: return InputKeyCode::Digit6;
    case GLFW_KEY_7: return InputKeyCode::Digit7;
    case GLFW_KEY_8: return InputKeyCode::Digit8;
    case GLFW_KEY_9: return InputKeyCode::Digit9;
    case GLFW_KEY_F1: return InputKeyCode::F1;
    case GLFW_KEY_F2: return InputKeyCode::F2;
    case GLFW_KEY_F3: return InputKeyCode::F3;
    case GLFW_KEY_F4: return InputKeyCode::F4;
    case GLFW_KEY_F5: return InputKeyCode::F5;
    case GLFW_KEY_F6: return InputKeyCode::F6;
    case GLFW_KEY_F7: return InputKeyCode::F7;
    case GLFW_KEY_F8: return InputKeyCode::F8;
    case GLFW_KEY_F9: return InputKeyCode::F9;
    case GLFW_KEY_F10: return InputKeyCode::F10;
    case GLFW_KEY_F11: return InputKeyCode::F11;
    case GLFW_KEY_F12: return InputKeyCode::F12;
    case GLFW_KEY_A: return InputKeyCode::A;
    case GLFW_KEY_B: return InputKeyCode::B;
    case GLFW_KEY_C: return InputKeyCode::C;
    case GLFW_KEY_D: return InputKeyCode::D;
    case GLFW_KEY_E: return InputKeyCode::E;
    case GLFW_KEY_F: return InputKeyCode::F;
    case GLFW_KEY_G: return InputKeyCode::G;
    case GLFW_KEY_H: return InputKeyCode::H;
    case GLFW_KEY_I: return InputKeyCode::I;
    case GLFW_KEY_J: return InputKeyCode::J;
    case GLFW_KEY_K: return InputKeyCode::K;
    case GLFW_KEY_L: return InputKeyCode::L;
    case GLFW_KEY_M: return InputKeyCode::M;
    case GLFW_KEY_N: return InputKeyCode::N;
    case GLFW_KEY_O: return InputKeyCode::O;
    case GLFW_KEY_P: return InputKeyCode::P;
    case GLFW_KEY_Q: return InputKeyCode::Q;
    case GLFW_KEY_R: return InputKeyCode::R;
    case GLFW_KEY_S: return InputKeyCode::S;
    case GLFW_KEY_T: return InputKeyCode::T;
    case GLFW_KEY_U: return InputKeyCode::U;
    case GLFW_KEY_V: return InputKeyCode::V;
    case GLFW_KEY_W: return InputKeyCode::W;
    case GLFW_KEY_X: return InputKeyCode::X;
    case GLFW_KEY_Y: return InputKeyCode::Y;
    case GLFW_KEY_Z: return InputKeyCode::Z;
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
    g_PointerStates[window] = {};
    glfwSetCursorPosCallback(window, [](GLFWwindow* native, double x, double y) {
        auto& pointer = g_PointerStates[native];
        pointer.logicalPosition = {
            static_cast<float>(x), static_cast<float>(y)};
        pointer.hasPosition = true;
        if (InputEventQueue* queue = GetInputQueue(native)) {
            PushNative(*queue, PointerMoveEvent{
                queue->Window(), pointer.logicalPosition,
                InputModifiers::None, 0, glfwGetTime()});
        }
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* native, int button, int action, int modifiers) {
        if (InputEventQueue* queue = GetInputQueue(native)) {
            PushNative(*queue, PointerButtonEvent{
                queue->Window(),
                CurrentPointerPosition(native),
                ToPointerButton(button),
                action != GLFW_RELEASE,
                ToInputModifiers(modifiers), 0, glfwGetTime()});
        }
    });
    glfwSetScrollCallback(window, [](GLFWwindow* native, double xOffset, double yOffset) {
        if (InputEventQueue* queue = GetInputQueue(native)) {
            PushNative(*queue, PointerWheelEvent{
                queue->Window(),
                CurrentPointerPosition(native),
                {static_cast<float>(xOffset), static_cast<float>(yOffset)},
                InputModifiers::None, 0, glfwGetTime()});
        }
    });
    glfwSetCursorEnterCallback(window, [](GLFWwindow* native, int entered) {
        if (entered == GLFW_FALSE) {
            g_PointerStates[native].hasPosition = false;
            if (InputEventQueue* queue = GetInputQueue(native)) {
                PushNative(*queue, PointerLeaveEvent{
                    queue->Window(), 0, glfwGetTime()});
            }
        }
    });
    glfwSetKeyCallback(window, [](GLFWwindow* native, int key, int, int action, int modifiers) {
#ifdef _WIN32
        // A committed IME result may be followed by duplicate WM_CHAR
        // messages. If no duplicate arrives, the next ordinary key press
        // starts a new input sequence and must not inherit that suppression.
        if (action != GLFW_RELEASE && key != GLFW_KEY_UNKNOWN) {
            if (HWND hwnd = glfwGetWin32Window(native)) {
                const auto hook = g_ImeHooks.find(hwnd);
                if (hook != g_ImeHooks.end())
                    hook->second.suppressCommittedScalars = 0;
            }
        }
#endif
        if (InputEventQueue* queue = GetInputQueue(native)) {
            PushNative(*queue, KeyEvent{
                queue->Window(),
                ToInputKeyCode(key),
                action != GLFW_RELEASE,
                action == GLFW_REPEAT,
                ToInputModifiers(modifiers), glfwGetTime()});
        }
    });
    glfwSetCharCallback(window, [](GLFWwindow* native, unsigned int codepoint) {
#ifdef _WIN32
        if (HWND hwnd = glfwGetWin32Window(native)) {
            const auto hook = g_ImeHooks.find(hwnd);
            if (hook != g_ImeHooks.end() &&
                hook->second.suppressCommittedScalars > 0) {
                --hook->second.suppressCommittedScalars;
                return;
            }
        }
#endif
        if (InputEventQueue* queue = GetInputQueue(native)) {
            PushNative(*queue, TextInputEvent{queue->Window(),
                static_cast<char32_t>(codepoint), glfwGetTime()});
        }
    });
    glfwSetWindowFocusCallback(window, [](GLFWwindow* native, int focused) {
        if (InputEventQueue* queue = GetInputQueue(native)) {
            PushNative(*queue, WindowFocusEvent{queue->Window(),
                focused == GLFW_TRUE, glfwGetTime()});
        }
    });
#ifdef _WIN32
    InstallImeHook(window);
#endif
}

void RemoveInputCallbacks(GLFWwindow* window) {
#ifdef _WIN32
    RemoveImeHook(window);
#endif
    glfwSetCursorPosCallback(window, nullptr);
    glfwSetMouseButtonCallback(window, nullptr);
    glfwSetScrollCallback(window, nullptr);
    glfwSetCursorEnterCallback(window, nullptr);
    glfwSetKeyCallback(window, nullptr);
    glfwSetCharCallback(window, nullptr);
    glfwSetWindowFocusCallback(window, nullptr);
    glfwSetWindowUserPointer(window, nullptr);
    g_PointerStates.erase(window);
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

bool GlfwWindowSystem::ActivateWindow(WindowHandle window) {
    GLFWwindow* native = GetGlfwWindow(window);
    if (native == nullptr) return false;
    glfwShowWindow(native);
    glfwFocusWindow(native);
    return true;
}

bool GlfwWindowSystem::RequestClose(WindowHandle window) {
    GLFWwindow* native = GetGlfwWindow(window);
    if (native == nullptr) return false;
    glfwSetWindowShouldClose(native, GLFW_TRUE);
    return true;
}

bool GlfwWindowSystem::ClearCloseRequest(WindowHandle window) {
    GLFWwindow* native = GetGlfwWindow(window);
    if (native == nullptr) return false;
    glfwSetWindowShouldClose(native, GLFW_FALSE);
    return true;
}

bool GlfwWindowSystem::IsVisible(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    return slot != nullptr && slot->visible;
}

bool GlfwWindowSystem::IsMinimized(WindowHandle window) const {
    if (GLFWwindow* native = GetGlfwWindow(window)) {
        return glfwGetWindowAttrib(native, GLFW_ICONIFIED) == GLFW_TRUE;
    }
    return true;
}

bool GlfwWindowSystem::IsMaximized(WindowHandle window) const {
    if (GLFWwindow* native = GetGlfwWindow(window)) {
        return glfwGetWindowAttrib(native, GLFW_MAXIMIZED) == GLFW_TRUE;
    }
    return false;
}

bool GlfwWindowSystem::IsFocused(WindowHandle window) const {
    if (GLFWwindow* native = GetGlfwWindow(window)) {
        return glfwGetWindowAttrib(native, GLFW_FOCUSED) == GLFW_TRUE;
    }
    return false;
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

RenderSurfaceSize GlfwWindowSystem::GetFramebufferSize(WindowHandle window) const {
    RenderSurfaceSize size{};
    if (GLFWwindow* native = GetGlfwWindow(window)) {
        glfwGetFramebufferSize(native, &size.width, &size.height);
    }
    return size;
}

NativeWindowHandle GlfwWindowSystem::GetNativeHandle(WindowHandle window) const {
#ifdef _WIN32
    GLFWwindow* native = GetGlfwWindow(window);
    return native
        ? reinterpret_cast<NativeWindowHandle>(glfwGetWin32Window(native))
        : 0;
#else
    return reinterpret_cast<NativeWindowHandle>(GetGlfwWindow(window));
#endif
}

Vec2F GlfwWindowSystem::GetWindowScreenPositionPhysical(
    WindowHandle window) const {
    int x = 0;
    int y = 0;
    if (GLFWwindow* native = GetGlfwWindow(window))
        glfwGetWindowPos(native, &x, &y);
    return {static_cast<float>(x), static_cast<float>(y)};
}

bool GlfwWindowSystem::SetInputEventQueue(WindowHandle window, InputEventQueue* queue) {
    GLFWwindow* native = GetGlfwWindow(window);
    if (native == nullptr || (queue != nullptr && queue->Window() != window)) {
        return false;
    }
    InputEventQueue* previous = GetInputQueue(native);
    if (previous == queue) return true;
    if (queue == nullptr) {
        RemoveInputCallbacks(native);
    } else {
        glfwSetWindowUserPointer(native, queue);
        InstallInputCallbacks(native);
        PushNative(*queue, WindowFocusEvent{
            window,
            glfwGetWindowAttrib(native, GLFW_FOCUSED) == GLFW_TRUE,
            glfwGetTime()});
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
