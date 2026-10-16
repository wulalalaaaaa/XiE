#include "Platform/Windows/TextInput/WindowsTextInputMethod.h"

#include "Input/InputDiagnostics.h"
#include "Platform/IWindowSystem.h"

#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <imm.h>
#endif

namespace Engine {

IWindowSystem* WindowsTextInputMethod::Resolve(
    WindowHandle window) const {
    return m_Resolver ? m_Resolver(window) : m_Windows;
}

void WindowsTextInputMethod::Activate(WindowHandle window) {
#ifdef _WIN32
    IWindowSystem* windows = Resolve(window);
    const auto hwnd = windows
        ? reinterpret_cast<HWND>(windows->GetNativeHandle(window)) : nullptr;
    if (hwnd && windows->IsFocused(window)) {
        ImmAssociateContextEx(hwnd, nullptr, IACE_DEFAULT);
        EmitInputDiagnostic({InputDiagnosticStage::Ime,
            InputDiagnosticKind::ImeActivated, window});
    }
#else
    (void)window;
#endif
}

void WindowsTextInputMethod::Deactivate(WindowHandle window) {
#ifdef _WIN32
    IWindowSystem* windows = Resolve(window);
    const auto hwnd = windows
        ? reinterpret_cast<HWND>(windows->GetNativeHandle(window)) : nullptr;
    if (hwnd) {
        EmitInputDiagnostic({InputDiagnosticStage::Ime,
            InputDiagnosticKind::ImeDeactivated, window});
        if (HIMC context = ImmGetContext(hwnd)) {
            ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
            ImmReleaseContext(hwnd, context);
        }
    }
#else
    (void)window;
#endif
}

void WindowsTextInputMethod::SetCaretRect(
    WindowHandle window, const RectF& rect) {
#ifdef _WIN32
    IWindowSystem* windows = Resolve(window);
    const auto hwnd = windows
        ? reinterpret_cast<HWND>(windows->GetNativeHandle(window)) : nullptr;
    if (!hwnd) return;
    EmitInputDiagnostic({InputDiagnosticStage::Ime,
        InputDiagnosticKind::ImeCandidatePosition, window,
        static_cast<std::uint64_t>(std::max(0.0f, rect.x)),
        static_cast<std::uint64_t>(std::max(0.0f, rect.y))});
    if (HIMC context = ImmGetContext(hwnd)) {
        POINT point{
            static_cast<LONG>(rect.x),
            static_cast<LONG>(rect.y + rect.height)};
        ScreenToClient(hwnd, &point);
        CANDIDATEFORM candidate{};
        candidate.dwIndex = 0;
        candidate.dwStyle = CFS_CANDIDATEPOS;
        candidate.ptCurrentPos = point;
        ImmSetCandidateWindow(context, &candidate);
        COMPOSITIONFORM composition{};
        composition.dwStyle = CFS_POINT;
        composition.ptCurrentPos = point;
        ImmSetCompositionWindow(context, &composition);
        ImmReleaseContext(hwnd, context);
    }
#else
    (void)window; (void)rect;
#endif
}

void WindowsTextInputMethod::CancelComposition(WindowHandle window) {
#ifdef _WIN32
    IWindowSystem* windows = Resolve(window);
    const auto hwnd = windows
        ? reinterpret_cast<HWND>(windows->GetNativeHandle(window)) : nullptr;
    if (!hwnd) return;
    if (HIMC context = ImmGetContext(hwnd)) {
        ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
        ImmReleaseContext(hwnd, context);
    }
#else
    (void)window;
#endif
}

} // namespace Engine
