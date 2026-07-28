#include "LayeredWindowPresenter.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <cstring>

namespace Engine {

LayeredWindowPresenter::LayeredWindowPresenter(NativeWindowHandle hwnd)
    : m_Window(hwnd) {}

void LayeredWindowPresenter::SetWindow(NativeWindowHandle hwnd) {
    m_Window = hwnd;
}

bool LayeredWindowPresenter::Present(const PremultipliedBitmap& bitmap) {
#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(m_Window);
    if (hwnd == nullptr || bitmap.width <= 0 || bitmap.height <= 0 || bitmap.bgra.empty()) {
        return false;
    }

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = bitmap.width;
    info.bmiHeader.biHeight = -bitmap.height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib == nullptr || bits == nullptr) {
        if (dib != nullptr) {
            DeleteObject(dib);
        }
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
        return false;
    }

    std::memcpy(bits, bitmap.bgra.data(), bitmap.bgra.size());
    HGDIOBJ old = SelectObject(mem, dib);

    POINT source{0, 0};
    SIZE size{bitmap.width, bitmap.height};
    POINT pos{};
    RECT windowRect{};
    GetWindowRect(hwnd, &windowRect);
    pos.x = windowRect.left;
    pos.y = windowRect.top;
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    const BOOL ok = UpdateLayeredWindow(hwnd, screen, &pos, &size, mem, &source, 0, &blend, ULW_ALPHA);

    SelectObject(mem, old);
    DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return ok != FALSE;
#else
    (void)bitmap;
    return false;
#endif
}

} // namespace Engine
