#include "Platform/Windows/TextInput/WindowsTextClipboard.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include <cstring>

namespace Engine {
#ifdef _WIN32
namespace {

std::wstring ToWide(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
        static_cast<int>(utf8.size()), nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
        static_cast<int>(utf8.size()), result.data(), count);
    return result;
}

std::string ToUtf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    const int count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
        static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (count <= 0) return {};
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
        static_cast<int>(wide.size()), result.data(), count, nullptr, nullptr);
    return result;
}

struct ClipboardGuard {
    bool opened = OpenClipboard(nullptr) != FALSE;
    ~ClipboardGuard() { if (opened) CloseClipboard(); }
};

} // namespace
#endif

bool WindowsTextClipboard::HasText() const {
#ifdef _WIN32
    return IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
#else
    return false;
#endif
}

TextClipboardReadResult WindowsTextClipboard::ReadText() {
#ifdef _WIN32
    ClipboardGuard guard;
    if (!guard.opened) return {false, {}, "OpenClipboard failed"};
    HANDLE handle = GetClipboardData(CF_UNICODETEXT);
    if (!handle) return {false, {}, "CF_UNICODETEXT is unavailable"};
    const auto* value = static_cast<const wchar_t*>(GlobalLock(handle));
    if (!value) return {false, {}, "GlobalLock failed"};
    const std::wstring wide(value);
    GlobalUnlock(handle);
    return {true, ToUtf8(wide), {}};
#else
    return {false, {}, "Windows clipboard is unavailable"};
#endif
}

TextClipboardWriteResult WindowsTextClipboard::WriteText(
    std::string_view utf8) {
#ifdef _WIN32
    const std::wstring wide = ToWide(utf8);
    if (!utf8.empty() && wide.empty())
        return {false, "clipboard text is not valid UTF-8"};
    ClipboardGuard guard;
    if (!guard.opened) return {false, "OpenClipboard failed"};
    if (!EmptyClipboard()) return {false, "EmptyClipboard failed"};
    const SIZE_T bytes = (wide.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!memory) return {false, "GlobalAlloc failed"};
    void* target = GlobalLock(memory);
    if (!target) {
        GlobalFree(memory);
        return {false, "GlobalLock failed"};
    }
    std::memcpy(target, wide.c_str(), bytes);
    GlobalUnlock(memory);
    if (!SetClipboardData(CF_UNICODETEXT, memory)) {
        GlobalFree(memory);
        return {false, "SetClipboardData failed"};
    }
    return {true, {}};
#else
    (void)utf8;
    return {false, "Windows clipboard is unavailable"};
#endif
}

} // namespace Engine
