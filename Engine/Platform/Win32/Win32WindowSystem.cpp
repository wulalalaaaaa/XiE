#include "Win32WindowSystem.h"

#include "Input/InputEventQueue.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <windowsx.h>
#ifdef CreateWindow
#undef CreateWindow
#endif

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace Engine {

namespace {

constexpr wchar_t kWindowClassName[] = L"XiE.Win32Window";

std::wstring ToWide(const std::string& text) {
    if (text.empty()) {
        return L"XiE Window";
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return L"XiE Window";
    }

    std::wstring wide(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), size);
    return wide;
}

} // namespace

struct Win32WindowSystem::Slot {
    HWND hwnd = nullptr;
    WindowHandle handle{};
    InputEventQueue* inputQueue = nullptr;
    std::uint32_t generation = 1;
    bool visible = false;
    bool closeRequested = false;
    bool toolWindow = false;
    bool trackingMouseLeave = false;
    wchar_t pendingHighSurrogate = 0;
};

static InputModifiers CurrentInputModifiers() {
    std::uint8_t result = 0;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) result |= static_cast<std::uint8_t>(InputModifiers::Shift);
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) result |= static_cast<std::uint8_t>(InputModifiers::Control);
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) result |= static_cast<std::uint8_t>(InputModifiers::Alt);
    if ((GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0) {
        result |= static_cast<std::uint8_t>(InputModifiers::Super);
    }
    return static_cast<InputModifiers>(result);
}

static InputKeyCode ToInputKeyCode(WPARAM key) {
    switch (key) {
    case VK_ESCAPE: return InputKeyCode::Escape;
    case VK_RETURN: return InputKeyCode::Enter;
    case VK_TAB: return InputKeyCode::Tab;
    case VK_BACK: return InputKeyCode::Backspace;
    case VK_SPACE: return InputKeyCode::Space;
    case VK_LEFT: return InputKeyCode::Left;
    case VK_RIGHT: return InputKeyCode::Right;
    case VK_UP: return InputKeyCode::Up;
    case VK_DOWN: return InputKeyCode::Down;
    default: return InputKeyCode::Unknown;
    }
}

static float WindowDpiScale(HWND hwnd) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    using GetDpiForWindowProc = UINT(WINAPI*)(HWND);
    const auto getDpiForWindow = reinterpret_cast<GetDpiForWindowProc>(
        GetProcAddress(user32, "GetDpiForWindow"));
    const UINT dpi = getDpiForWindow != nullptr ? getDpiForWindow(hwnd) : 96u;
    return std::max(0.01f, static_cast<float>(dpi) / 96.0f);
}

static double InputTimestampSeconds() {
    return static_cast<double>(GetTickCount64()) / 1000.0;
}

static Vec2F LogicalClientPosition(HWND hwnd, LPARAM lParam, bool screenCoordinates) {
    POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    if (screenCoordinates) ScreenToClient(hwnd, &point);
    const float scale = WindowDpiScale(hwnd);
    return {static_cast<float>(point.x) / scale, static_cast<float>(point.y) / scale};
}

static void PushTextInput(Win32WindowSystem::Slot& slot, wchar_t codeUnit) {
    if (slot.inputQueue == nullptr) return;
    char32_t codepoint = 0;
    if (codeUnit >= 0xD800 && codeUnit <= 0xDBFF) {
        slot.pendingHighSurrogate = codeUnit;
        return;
    }
    if (codeUnit >= 0xDC00 && codeUnit <= 0xDFFF && slot.pendingHighSurrogate != 0) {
        codepoint = 0x10000u +
            ((static_cast<char32_t>(slot.pendingHighSurrogate) - 0xD800u) << 10u) +
            (static_cast<char32_t>(codeUnit) - 0xDC00u);
    } else {
        codepoint = static_cast<char32_t>(codeUnit);
    }
    slot.pendingHighSurrogate = 0;
    (void)slot.inputQueue->Push(TextInputEvent{
        slot.handle, codepoint, static_cast<double>(GetTickCount64()) / 1000.0});
}

static LRESULT CALLBACK XiEWin32WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* slot = reinterpret_cast<Win32WindowSystem::Slot*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        slot = reinterpret_cast<Win32WindowSystem::Slot*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(slot));
    }

    switch (message) {
    case WM_MOUSEMOVE:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            if (!slot->trackingMouseLeave) {
                TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
                slot->trackingMouseLeave = TrackMouseEvent(&tracking) != FALSE;
            }
            (void)slot->inputQueue->Push(PointerMoveEvent{
                slot->handle, LogicalClientPosition(hwnd, lParam, false), CurrentInputModifiers(),
                0, InputTimestampSeconds()});
        }
        break;
    case WM_MOUSELEAVE:
        if (slot != nullptr) {
            slot->trackingMouseLeave = false;
            if (slot->inputQueue != nullptr) {
                (void)slot->inputQueue->Push(PointerLeaveEvent{slot->handle, 0, InputTimestampSeconds()});
            }
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            PointerButton button = PointerButton::Other;
            if (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) button = PointerButton::Primary;
            if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) button = PointerButton::Secondary;
            if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) button = PointerButton::Middle;
            const bool pressed = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
            (void)slot->inputQueue->Push(PointerButtonEvent{
                slot->handle, LogicalClientPosition(hwnd, lParam, false), button, pressed,
                CurrentInputModifiers(), 0, InputTimestampSeconds()});
        }
        break;
    case WM_MOUSEWHEEL:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            (void)slot->inputQueue->Push(PointerWheelEvent{
                slot->handle,
                LogicalClientPosition(hwnd, lParam, true),
                {0.0f, static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA)},
                CurrentInputModifiers(), 0, InputTimestampSeconds()});
        }
        break;
    case WM_KEYDOWN:
    case WM_KEYUP:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            const bool pressed = message == WM_KEYDOWN;
            const bool repeat = pressed && (lParam & (1ll << 30ll)) != 0;
            (void)slot->inputQueue->Push(KeyEvent{
                slot->handle, ToInputKeyCode(wParam), pressed, repeat,
                CurrentInputModifiers(), InputTimestampSeconds()});
        }
        break;
    case WM_CHAR:
        if (slot != nullptr) PushTextInput(*slot, static_cast<wchar_t>(wParam));
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            (void)slot->inputQueue->Push(WindowFocusEvent{
                slot->handle, message == WM_SETFOCUS, InputTimestampSeconds()});
        }
        break;
    case WM_CLOSE:
        if (slot != nullptr) {
            slot->closeRequested = true;
            if (!slot->toolWindow) {
                PostQuitMessage(0);
            }
        }
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

Win32WindowSystem::Win32WindowSystem() {
    EnsureWindowClass();
}

Win32WindowSystem::~Win32WindowSystem() {
    for (std::unique_ptr<Slot>& slot : m_Slots) {
        if (slot && slot->hwnd != nullptr) {
            ::DestroyWindow(slot->hwnd);
            slot->hwnd = nullptr;
            ++slot->generation;
        }
    }
}

void Win32WindowSystem::EnsureWindowClass() {
    if (m_ClassRegistered) {
        return;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = XiEWin32WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = kWindowClassName;

    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        throw std::runtime_error("Failed to register Win32 window class");
    }
    m_ClassRegistered = true;
}

WindowHandle Win32WindowSystem::CreateWindow(const WindowDesc& desc) {
    EnsureWindowClass();

    DWORD style = desc.decorated ? WS_OVERLAPPEDWINDOW : WS_POPUP;
    if (!desc.resizable && desc.decorated) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    DWORD exStyle = 0;
    if (desc.alwaysOnTop) {
        exStyle |= WS_EX_TOPMOST;
    }
    if (desc.toolWindow) {
        exStyle |= WS_EX_TOOLWINDOW;
    }
    if (desc.transparent) {
        exStyle |= WS_EX_LAYERED;
    }

    auto freeIt = std::find_if(m_Slots.begin(), m_Slots.end(), [](const std::unique_ptr<Slot>& slot) {
        return slot && slot->hwnd == nullptr;
    });

    std::uint32_t slotIndex = 0;
    Slot* slot = nullptr;
    if (freeIt == m_Slots.end()) {
        m_Slots.push_back(std::make_unique<Slot>());
        slotIndex = static_cast<std::uint32_t>(m_Slots.size());
        slot = m_Slots.back().get();
    } else {
        slotIndex = static_cast<std::uint32_t>(std::distance(m_Slots.begin(), freeIt) + 1);
        slot = freeIt->get();
    }

    slot->visible = desc.visible;
    slot->closeRequested = false;
    slot->toolWindow = desc.toolWindow;
    if (slot->generation == 0) {
        slot->generation = 1;
    }
    slot->handle = WindowHandle{slotIndex, slot->generation};
    slot->inputQueue = nullptr;
    slot->trackingMouseLeave = false;
    slot->pendingHighSurrogate = 0;

    RECT rect{0, 0, desc.width, desc.height};
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);

    const std::wstring title = ToWide(desc.title);
    HWND hwnd = CreateWindowExW(
        exStyle,
        kWindowClassName,
        title.c_str(),
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        slot
    );

    if (hwnd == nullptr) {
        throw std::runtime_error("Failed to create Win32 window");
    }

    slot->hwnd = hwnd;
    if (desc.transparent) {
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    }
    if (desc.visible) {
        ::ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }

    return slot->handle;
}

void Win32WindowSystem::DestroyWindow(WindowHandle window) {
    Slot* slot = Resolve(window);
    if (slot == nullptr) {
        return;
    }

    ::DestroyWindow(slot->hwnd);
    slot->hwnd = nullptr;
    slot->handle = {};
    slot->inputQueue = nullptr;
    slot->visible = false;
    slot->closeRequested = true;
    ++slot->generation;
    if (slot->generation == 0) {
        slot->generation = 1;
    }
}

void Win32WindowSystem::ShowWindow(WindowHandle window) {
    Slot* slot = Resolve(window);
    if (slot == nullptr) {
        return;
    }
    ::ShowWindow(slot->hwnd, SW_SHOWNOACTIVATE);
    slot->visible = true;
    slot->closeRequested = false;
}

void Win32WindowSystem::HideWindow(WindowHandle window) {
    Slot* slot = Resolve(window);
    if (slot == nullptr) {
        return;
    }
    ::ShowWindow(slot->hwnd, SW_HIDE);
    slot->visible = false;
}

void Win32WindowSystem::SetWindowPosition(WindowHandle window, int x, int y) {
    Slot* slot = Resolve(window);
    if (slot == nullptr) {
        return;
    }
    SetWindowPos(slot->hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32WindowSystem::SetWindowSize(WindowHandle window, int width, int height) {
    Slot* slot = Resolve(window);
    if (slot == nullptr) {
        return;
    }
    SetWindowPos(slot->hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

bool Win32WindowSystem::IsVisible(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    return slot != nullptr && slot->visible && IsWindowVisible(slot->hwnd) != FALSE;
}

bool Win32WindowSystem::IsCloseRequested(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    return slot == nullptr || slot->closeRequested;
}

float Win32WindowSystem::GetDpiScale(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    if (slot == nullptr) {
        return 1.0f;
    }

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    using GetDpiForWindowProc = UINT(WINAPI*)(HWND);
    auto getDpiForWindow = reinterpret_cast<GetDpiForWindowProc>(GetProcAddress(user32, "GetDpiForWindow"));
    const UINT dpi = getDpiForWindow != nullptr ? getDpiForWindow(slot->hwnd) : 96u;
    return static_cast<float>(dpi) / 96.0f;
}

NativeWindowHandle Win32WindowSystem::GetNativeHandle(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    return slot != nullptr ? reinterpret_cast<NativeWindowHandle>(slot->hwnd) : 0;
}

bool Win32WindowSystem::SetInputEventQueue(WindowHandle window, InputEventQueue* queue) {
    Slot* slot = Resolve(window);
    if (slot == nullptr || (queue != nullptr && queue->Window() != window)) {
        return false;
    }
    slot->inputQueue = queue;
    slot->pendingHighSurrogate = 0;
    return true;
}

Win32WindowSystem::Slot* Win32WindowSystem::Resolve(WindowHandle window) {
    return const_cast<Slot*>(static_cast<const Win32WindowSystem*>(this)->Resolve(window));
}

const Win32WindowSystem::Slot* Win32WindowSystem::Resolve(WindowHandle window) const {
    if (!window.IsValid() || window.index > m_Slots.size()) {
        return nullptr;
    }

    const std::unique_ptr<Slot>& slot = m_Slots[window.index - 1];
    if (!slot || slot->hwnd == nullptr || slot->generation != window.generation) {
        return nullptr;
    }
    return slot.get();
}

} // namespace Engine
