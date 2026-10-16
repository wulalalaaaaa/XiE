#include "Win32WindowSystem.h"

#include "Input/InputEventQueue.h"
#include "Input/InputDiagnostics.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <windowsx.h>
#include <imm.h>
#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef IsMinimized
#undef IsMinimized
#endif
#ifdef IsMaximized
#undef IsMaximized
#endif

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace Engine {

namespace {

constexpr wchar_t kWindowClassName[] = L"XiE.Win32Window";

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
    else if constexpr (std::is_same_v<T, PointerCancelEvent>)
        return InputDiagnosticKind::PointerCancel;
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
    bool noActivate = false;
    bool clickThrough = false;
    bool trackingMouseLeave = false;
    wchar_t pendingHighSurrogate = 0;
    std::size_t suppressCommittedScalars = 0;
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
    case VK_DELETE: return InputKeyCode::Delete;
    case VK_SPACE: return InputKeyCode::Space;
    case VK_INSERT: return InputKeyCode::Insert;
    case VK_HOME: return InputKeyCode::Home;
    case VK_END: return InputKeyCode::End;
    case VK_PRIOR: return InputKeyCode::PageUp;
    case VK_NEXT: return InputKeyCode::PageDown;
    case VK_LEFT: return InputKeyCode::Left;
    case VK_RIGHT: return InputKeyCode::Right;
    case VK_UP: return InputKeyCode::Up;
    case VK_DOWN: return InputKeyCode::Down;
    case '0': return InputKeyCode::Digit0;
    case '1': return InputKeyCode::Digit1;
    case '2': return InputKeyCode::Digit2;
    case '3': return InputKeyCode::Digit3;
    case '4': return InputKeyCode::Digit4;
    case '5': return InputKeyCode::Digit5;
    case '6': return InputKeyCode::Digit6;
    case '7': return InputKeyCode::Digit7;
    case '8': return InputKeyCode::Digit8;
    case '9': return InputKeyCode::Digit9;
    case VK_F1: return InputKeyCode::F1;
    case VK_F2: return InputKeyCode::F2;
    case VK_F3: return InputKeyCode::F3;
    case VK_F4: return InputKeyCode::F4;
    case VK_F5: return InputKeyCode::F5;
    case VK_F6: return InputKeyCode::F6;
    case VK_F7: return InputKeyCode::F7;
    case VK_F8: return InputKeyCode::F8;
    case VK_F9: return InputKeyCode::F9;
    case VK_F10: return InputKeyCode::F10;
    case VK_F11: return InputKeyCode::F11;
    case VK_F12: return InputKeyCode::F12;
    case 'A': return InputKeyCode::A;
    case 'B': return InputKeyCode::B;
    case 'C': return InputKeyCode::C;
    case 'D': return InputKeyCode::D;
    case 'E': return InputKeyCode::E;
    case 'F': return InputKeyCode::F;
    case 'G': return InputKeyCode::G;
    case 'H': return InputKeyCode::H;
    case 'I': return InputKeyCode::I;
    case 'J': return InputKeyCode::J;
    case 'K': return InputKeyCode::K;
    case 'L': return InputKeyCode::L;
    case 'M': return InputKeyCode::M;
    case 'N': return InputKeyCode::N;
    case 'O': return InputKeyCode::O;
    case 'P': return InputKeyCode::P;
    case 'Q': return InputKeyCode::Q;
    case 'R': return InputKeyCode::R;
    case 'S': return InputKeyCode::S;
    case 'T': return InputKeyCode::T;
    case 'U': return InputKeyCode::U;
    case 'V': return InputKeyCode::V;
    case 'W': return InputKeyCode::W;
    case 'X': return InputKeyCode::X;
    case 'Y': return InputKeyCode::Y;
    case 'Z': return InputKeyCode::Z;
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
    // Editing/navigation controls are already delivered as KeyEvent values.
    // WM_CHAR repeats Backspace, Tab, Enter and Escape as control characters;
    // forwarding those again inserts non-printing glyphs into text widgets.
    if (codeUnit < L' ' || codeUnit == 0x7f) return;
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
    if (slot.suppressCommittedScalars > 0) {
        --slot.suppressCommittedScalars;
        return;
    }
    PushNative(*slot.inputQueue, TextInputEvent{
        slot.handle, codepoint, static_cast<double>(GetTickCount64()) / 1000.0});
}

static Vec2F CurrentLogicalClientPosition(HWND hwnd) {
    POINT point{};
    (void)GetCursorPos(&point);
    (void)ScreenToClient(hwnd, &point);
    const float scale = WindowDpiScale(hwnd);
    return {static_cast<float>(point.x) / scale,
        static_cast<float>(point.y) / scale};
}

static std::string WideToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), result.data(), count, nullptr, nullptr);
    return result;
}

static std::wstring ReadCompositionString(HIMC context, DWORD kind) {
    const LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
    if (bytes <= 0) return {};
    std::wstring result(static_cast<std::size_t>(bytes) / sizeof(wchar_t),
        L'\0');
    const LONG read = ImmGetCompositionStringW(
        context, kind, result.data(), bytes);
    if (read < 0) return {};
    result.resize(static_cast<std::size_t>(read) / sizeof(wchar_t));
    return result;
}

static std::size_t WideScalarIndex(
    std::wstring_view text, std::size_t codeUnitIndex) {
    std::size_t scalar = 0;
    for (std::size_t i = 0;
         i < std::min(codeUnitIndex, text.size()); ++i) {
        if (text[i] >= 0xD800 && text[i] <= 0xDBFF &&
            i + 1 < text.size() &&
            text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF) {
            ++i;
        }
        ++scalar;
    }
    return scalar;
}

static void PushCompositionUpdate(
    HWND hwnd, Win32WindowSystem::Slot& slot, LPARAM flags) {
    if (!slot.inputQueue) return;
    HIMC context = ImmGetContext(hwnd);
    if (!context) return;
    if ((flags & GCS_RESULTSTR) != 0) {
        const std::wstring value =
            ReadCompositionString(context, GCS_RESULTSTR);
        slot.suppressCommittedScalars =
            WideScalarIndex(value, value.size());
        PushNative(*slot.inputQueue, TextCompositionCommitEvent{
            slot.handle, WideToUtf8(value), InputTimestampSeconds()});
    }
    if ((flags & GCS_COMPSTR) != 0) {
        const std::wstring value =
            ReadCompositionString(context, GCS_COMPSTR);
        const LONG cursor = ImmGetCompositionStringW(
            context, GCS_CURSORPOS, nullptr, 0);
        PushNative(*slot.inputQueue, TextCompositionUpdateEvent{
            slot.handle,
            WideToUtf8(value),
            WideScalarIndex(value,
                cursor >= 0 ? static_cast<std::size_t>(cursor) : 0),
            std::nullopt,
            InputTimestampSeconds()});
    }
    ImmReleaseContext(hwnd, context);
}

static LRESULT CALLBACK XiEWin32WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* slot = reinterpret_cast<Win32WindowSystem::Slot*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        slot = reinterpret_cast<Win32WindowSystem::Slot*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(slot));
    }

    switch (message) {
    case WM_NCHITTEST:
        if (slot != nullptr && slot->clickThrough) {
            return HTTRANSPARENT;
        }
        break;
    case WM_MOUSEMOVE:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            if (!slot->trackingMouseLeave) {
                TRACKMOUSEEVENT tracking{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
                slot->trackingMouseLeave = TrackMouseEvent(&tracking) != FALSE;
            }
            PushNative(*slot->inputQueue, PointerMoveEvent{
                slot->handle, LogicalClientPosition(hwnd, lParam, false), CurrentInputModifiers(),
                0, InputTimestampSeconds()});
        }
        break;
    case WM_MOUSELEAVE:
        if (slot != nullptr) {
            slot->trackingMouseLeave = false;
            if (slot->inputQueue != nullptr) {
                PushNative(*slot->inputQueue, PointerLeaveEvent{
                    slot->handle, 0, InputTimestampSeconds()});
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
            if (pressed) (void)SetCapture(hwnd);
            PushNative(*slot->inputQueue, PointerButtonEvent{
                slot->handle, LogicalClientPosition(hwnd, lParam, false), button, pressed,
                CurrentInputModifiers(), 0, InputTimestampSeconds()});
            if (!pressed && (wParam &
                (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) == 0 &&
                GetCapture() == hwnd) {
                (void)ReleaseCapture();
            }
        }
        break;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            PushNative(*slot->inputQueue, PointerCancelEvent{
                slot->handle, 0, CurrentLogicalClientPosition(hwnd),
                InputTimestampSeconds()});
        }
        break;
    case WM_MOUSEWHEEL:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            PushNative(*slot->inputQueue, PointerWheelEvent{
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
            // IME result text is followed by duplicate WM_CHAR messages after
            // WM_IME_ENDCOMPOSITION. Keep their suppression count across the
            // end message, but never let a missing duplicate consume text
            // produced by the next ordinary key press.
            if (pressed && wParam != VK_PROCESSKEY)
                slot->suppressCommittedScalars = 0;
            const bool repeat = pressed && (lParam & (1ll << 30ll)) != 0;
            PushNative(*slot->inputQueue, KeyEvent{
                slot->handle, ToInputKeyCode(wParam), pressed, repeat,
                CurrentInputModifiers(), InputTimestampSeconds()});
        }
        break;
    case WM_CHAR:
        if (slot != nullptr) PushTextInput(*slot, static_cast<wchar_t>(wParam));
        return 0;
    case WM_IME_STARTCOMPOSITION:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            slot->suppressCommittedScalars = 0;
            PushNative(*slot->inputQueue, TextCompositionStartEvent{
                slot->handle, InputTimestampSeconds()});
        }
        break;
    case WM_IME_COMPOSITION:
        if (slot != nullptr) PushCompositionUpdate(hwnd, *slot, lParam);
        break;
    case WM_IME_ENDCOMPOSITION:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            PushNative(*slot->inputQueue, TextCompositionEndEvent{
                slot->handle, InputTimestampSeconds()});
        }
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        if (slot != nullptr && slot->inputQueue != nullptr) {
            PushNative(*slot->inputQueue, WindowFocusEvent{
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
    if (desc.noActivate) {
        exStyle |= WS_EX_NOACTIVATE;
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
    slot->noActivate = desc.noActivate;
    slot->clickThrough = desc.clickThrough;
    if (slot->generation == 0) {
        slot->generation = 1;
    }
    slot->handle = WindowHandle{slotIndex, slot->generation};
    slot->inputQueue = nullptr;
    slot->trackingMouseLeave = false;
    slot->pendingHighSurrogate = 0;
    slot->suppressCommittedScalars = 0;

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
    // Per-pixel-alpha windows are initialized by UpdateLayeredWindow in the
    // presenter. Calling SetLayeredWindowAttributes here makes subsequent
    // UpdateLayeredWindow calls fail until WS_EX_LAYERED is toggled.
    if (desc.visible) {
        ::ShowWindow(hwnd, desc.noActivate ? SW_SHOWNOACTIVATE : SW_SHOW);
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

bool Win32WindowSystem::ActivateWindow(WindowHandle window) {
    Slot* slot = Resolve(window);
    if (slot == nullptr || slot->hwnd == nullptr) return false;
    ::ShowWindow(slot->hwnd, SW_SHOW);
    (void)BringWindowToTop(slot->hwnd);
    (void)SetForegroundWindow(slot->hwnd);
    (void)SetActiveWindow(slot->hwnd);
    (void)SetFocus(slot->hwnd);
    slot->visible = true;
    slot->closeRequested = false;
    return true;
}

bool Win32WindowSystem::RequestClose(WindowHandle window) {
    Slot* slot = Resolve(window);
    if (slot == nullptr || slot->hwnd == nullptr) return false;
    return PostMessageW(slot->hwnd, WM_CLOSE, 0, 0) != FALSE;
}

bool Win32WindowSystem::ClearCloseRequest(WindowHandle window) {
    Slot* slot = Resolve(window);
    if (slot == nullptr) return false;
    slot->closeRequested = false;
    return true;
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

bool Win32WindowSystem::IsMinimized(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    return slot == nullptr || slot->hwnd == nullptr || ::IsIconic(slot->hwnd) != FALSE;
}

bool Win32WindowSystem::IsMaximized(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    if (slot == nullptr || slot->hwnd == nullptr) return false;
    WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
    return GetWindowPlacement(slot->hwnd, &placement) != FALSE &&
        placement.showCmd == SW_SHOWMAXIMIZED;
}

bool Win32WindowSystem::IsFocused(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    return slot != nullptr && slot->hwnd != nullptr && GetFocus() == slot->hwnd;
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

RenderSurfaceSize Win32WindowSystem::GetFramebufferSize(WindowHandle window) const {
    RenderSurfaceSize size{};
    const Slot* slot = Resolve(window);
    if (slot == nullptr || slot->hwnd == nullptr) return size;
    RECT client{};
    if (GetClientRect(slot->hwnd, &client) != FALSE) {
        size.width = client.right - client.left;
        size.height = client.bottom - client.top;
    }

    return size;
}

NativeWindowHandle Win32WindowSystem::GetNativeHandle(WindowHandle window) const {
    const Slot* slot = Resolve(window);
    return slot != nullptr ? reinterpret_cast<NativeWindowHandle>(slot->hwnd) : 0;
}

Vec2F Win32WindowSystem::GetWindowScreenPositionPhysical(
    WindowHandle window) const {
    const Slot* slot = Resolve(window);
    RECT rect{};
    if (!slot || !GetWindowRect(slot->hwnd, &rect)) return {};
    return {static_cast<float>(rect.left), static_cast<float>(rect.top)};
}

bool Win32WindowSystem::SetInputEventQueue(WindowHandle window, InputEventQueue* queue) {
    Slot* slot = Resolve(window);
    if (slot == nullptr || (queue != nullptr && queue->Window() != window)) {
        return false;
    }
    if (slot->inputQueue == queue) return true;
    slot->inputQueue = queue;
    slot->pendingHighSurrogate = 0;
    slot->suppressCommittedScalars = 0;
    if (queue != nullptr) {
        PushNative(*queue, WindowFocusEvent{
            window, GetFocus() == slot->hwnd, InputTimestampSeconds()});
    }
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
