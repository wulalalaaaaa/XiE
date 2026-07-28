#include "TextLayoutService.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace Engine {

namespace {

std::string MakeCacheKey(FontHandle font, std::u32string_view text, float maxWidth, float fontSize) {
    std::ostringstream out;
    out << font.index << ':' << font.generation << ':' << maxWidth << ':' << fontSize << ':';
    for (char32_t ch : text) {
        out << static_cast<std::uint32_t>(ch) << ',';
    }
    return out.str();
}

} // namespace

TextLayoutHandle BasicTextLayoutService::CreateLayout(
    FontHandle font,
    std::u32string_view text,
    float maxWidth,
    float fontSize
) {
    const std::string key = MakeCacheKey(font, text, maxWidth, fontSize);
    const auto found = m_Cache.find(key);
    if (found != m_Cache.end()) {
        return found->second;
    }

    auto it = std::find_if(m_Slots.begin(), m_Slots.end(), [](const Slot& slot) {
        return !slot.occupied;
    });
    if (it == m_Slots.end()) {
        m_Slots.push_back({});
        it = m_Slots.end() - 1;
    }

    it->bitmap = Rasterize(text, maxWidth, fontSize);
    it->occupied = true;
    ++m_RasterizeCount;
    ++m_Revision;
    if (it->generation == 0) {
        it->generation = 1;
    }

    TextLayoutHandle handle{
        static_cast<std::uint32_t>(std::distance(m_Slots.begin(), it) + 1),
        it->generation
    };
    m_Cache[key] = handle;
    return handle;
}

const TextLayoutBitmap* BasicTextLayoutService::GetBitmap(TextLayoutHandle handle) const {
    if (!handle.IsValid() || handle.index > m_Slots.size()) {
        return nullptr;
    }

    const Slot& slot = m_Slots[handle.index - 1];
    if (!slot.occupied || slot.generation != handle.generation) {
        return nullptr;
    }
    return &slot.bitmap;
}

std::uint32_t BasicTextLayoutService::RasterizeCount() const {
    return m_RasterizeCount;
}

std::wstring BasicTextLayoutService::ToWide(std::u32string_view text) {
    std::wstring wide;
    wide.reserve(text.size());
    for (char32_t ch : text) {
        if (ch <= 0xFFFF) {
            wide.push_back(static_cast<wchar_t>(ch));
        } else {
            ch -= 0x10000;
            wide.push_back(static_cast<wchar_t>(0xD800 + ((ch >> 10) & 0x3FF)));
            wide.push_back(static_cast<wchar_t>(0xDC00 + (ch & 0x3FF)));
        }
    }
    return wide;
}

TextLayoutBitmap BasicTextLayoutService::Rasterize(std::u32string_view text, float maxWidth, float fontSize) {
    const int width = std::max(1, static_cast<int>(std::ceil(maxWidth > 0.0f ? maxWidth : fontSize * static_cast<float>(text.size() + 1))));
    const int height = std::max(1, static_cast<int>(std::ceil(fontSize * 1.6f)));
    TextLayoutBitmap bitmap{};
    bitmap.width = width;
    bitmap.height = height;
    bitmap.bgraPremultiplied.assign(static_cast<std::size_t>(width * height * 4), 0);

#ifdef _WIN32
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP dib = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBitmap = SelectObject(dc, dib);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    HFONT font = CreateFontW(
        -static_cast<int>(std::round(fontSize)),
        0, 0, 0, FW_SEMIBOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH,
        L"Microsoft YaHei UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    RECT rect{0, 0, width, height};
    const std::wstring wide = ToWide(text);
    DrawTextW(dc, wide.c_str(), static_cast<int>(wide.size()), &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    const auto* src = static_cast<const std::uint8_t*>(bits);
    for (int i = 0; i < width * height; ++i) {
        const std::uint8_t b = src[static_cast<std::size_t>(i) * 4 + 0];
        const std::uint8_t g = src[static_cast<std::size_t>(i) * 4 + 1];
        const std::uint8_t r = src[static_cast<std::size_t>(i) * 4 + 2];
        const std::uint8_t a = std::max({r, g, b});
        bitmap.bgraPremultiplied[static_cast<std::size_t>(i) * 4 + 0] = a;
        bitmap.bgraPremultiplied[static_cast<std::size_t>(i) * 4 + 1] = a;
        bitmap.bgraPremultiplied[static_cast<std::size_t>(i) * 4 + 2] = a;
        bitmap.bgraPremultiplied[static_cast<std::size_t>(i) * 4 + 3] = a;
    }

    SelectObject(dc, oldFont);
    SelectObject(dc, oldBitmap);
    DeleteObject(font);
    DeleteObject(dib);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
#else
    (void)text;
#endif

    return bitmap;
}

} // namespace Engine
