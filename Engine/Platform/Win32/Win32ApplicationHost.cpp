#include "Win32ApplicationHost.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace Engine {

Win32ApplicationHost::Win32ApplicationHost() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    using SetDpiAwarenessContextProc = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    auto setDpiAwarenessContext = reinterpret_cast<SetDpiAwarenessContextProc>(
        GetProcAddress(user32, "SetProcessDpiAwarenessContext")
    );
    if (setDpiAwarenessContext != nullptr) {
        (void)setDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
}

void Win32ApplicationHost::PollEvents() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_ShouldExit = true;
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void Win32ApplicationHost::WaitForEvents() {
    MSG msg{};
    const BOOL result = GetMessageW(&msg, nullptr, 0, 0);
    if (result <= 0) {
        m_ShouldExit = true;
        return;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
}

void Win32ApplicationHost::WaitForEventsTimeout(double seconds) {
    const DWORD milliseconds = seconds <= 0.0 ? 0u : static_cast<DWORD>(seconds * 1000.0);
    MsgWaitForMultipleObjects(0, nullptr, FALSE, milliseconds, QS_ALLINPUT);
    PollEvents();
}

bool Win32ApplicationHost::ShouldExit() const {
    return m_ShouldExit;
}

void Win32ApplicationHost::RequestExit() {
    m_ShouldExit = true;
    PostQuitMessage(0);
}

} // namespace Engine
