#include "Application/FrameScheduler.h"
#include "Application/WindowContext.h"
#include "Platform/WindowPlacementUtils.h"
#include "Platform/Win32/Win32ApplicationHost.h"
#include "Platform/Win32/Win32WindowSystem.h"
#include "Renderer2D/Backends/Software/LayeredWindowPresenter.h"
#include "Renderer2D/Backends/Software/LayeredWindowRenderPipeline.h"
#include "Renderer2D/Backends/Software/Software2DRenderer.h"
#include "Renderer2D/DrawList2D.h"
#include "Renderer2D/TextLayoutService.h"
#include "Renderer2D/TextureRegistry2D.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <wincodec.h>
#ifdef CreateWindow
#undef CreateWindow
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

struct ItemRect {
    RECT rect{};
    int index = -1;
};

std::filesystem::path ExeDirectory() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

void ThrowIfFailed(HRESULT hr, const char* message) {
    if (FAILED(hr)) {
        throw std::runtime_error(message);
    }
}

void GeneratePng(IWICImagingFactory* factory, const std::filesystem::path& path, int index) {
    constexpr UINT width = 72;
    constexpr UINT height = 72;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4), 0);

    const std::array<std::array<std::uint8_t, 3>, 10> palette = {{
        {{235, 94, 85}}, {{247, 183, 49}}, {{38, 166, 154}}, {{66, 135, 245}}, {{156, 92, 222}},
        {{255, 112, 67}}, {{126, 190, 79}}, {{41, 182, 246}}, {{236, 64, 122}}, {{141, 110, 99}}
    }};
    const auto color = palette[static_cast<std::size_t>(index) % palette.size()];

    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            const float dx = static_cast<float>(x) - 35.5f;
            const float dy = static_cast<float>(y) - 35.5f;
            const float d2 = dx * dx + dy * dy;
            const bool inside = d2 <= 34.0f * 34.0f;
            const std::size_t offset = static_cast<std::size_t>((y * width + x) * 4);
            pixels[offset + 0] = color[0];
            pixels[offset + 1] = color[1];
            pixels[offset + 2] = color[2];
            pixels[offset + 3] = inside ? static_cast<std::uint8_t>(230 - (index % 3) * 35) : 0;
        }
    }

    IWICStream* stream = nullptr;
    ThrowIfFailed(factory->CreateStream(&stream), "CreateStream failed");
    ThrowIfFailed(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE), "InitializeFromFilename failed");

    IWICBitmapEncoder* encoder = nullptr;
    ThrowIfFailed(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder), "CreateEncoder failed");
    ThrowIfFailed(encoder->Initialize(stream, WICBitmapEncoderNoCache), "Encoder Initialize failed");

    IWICBitmapFrameEncode* frame = nullptr;
    ThrowIfFailed(encoder->CreateNewFrame(&frame, nullptr), "CreateNewFrame failed");
    ThrowIfFailed(frame->Initialize(nullptr), "Frame Initialize failed");
    ThrowIfFailed(frame->SetSize(width, height), "SetSize failed");
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppRGBA;
    ThrowIfFailed(frame->SetPixelFormat(&format), "SetPixelFormat failed");
    ThrowIfFailed(frame->WritePixels(height, width * 4, static_cast<UINT>(pixels.size()), pixels.data()), "WritePixels failed");
    ThrowIfFailed(frame->Commit(), "Frame Commit failed");
    ThrowIfFailed(encoder->Commit(), "Encoder Commit failed");

    frame->Release();
    encoder->Release();
    stream->Release();
}

Image LoadPng(IWICImagingFactory* factory, const std::filesystem::path& path) {
    IWICBitmapDecoder* decoder = nullptr;
    ThrowIfFailed(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder), "CreateDecoderFromFilename failed");

    IWICBitmapFrameDecode* frame = nullptr;
    ThrowIfFailed(decoder->GetFrame(0, &frame), "GetFrame failed");

    IWICFormatConverter* converter = nullptr;
    ThrowIfFailed(factory->CreateFormatConverter(&converter), "CreateFormatConverter failed");
    ThrowIfFailed(
        converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom),
        "FormatConverter Initialize failed"
    );

    UINT width = 0;
    UINT height = 0;
    ThrowIfFailed(converter->GetSize(&width, &height), "GetSize failed");

    Image image{};
    image.width = static_cast<int>(width);
    image.height = static_cast<int>(height);
    image.rgba.resize(static_cast<std::size_t>(width * height * 4));
    ThrowIfFailed(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(image.rgba.size()), image.rgba.data()), "CopyPixels failed");

    converter->Release();
    frame->Release();
    decoder->Release();
    return image;
}

std::vector<Image> BuildPngThumbnails() {
    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED), "CoInitializeEx failed");

    IWICImagingFactory* factory = nullptr;
    ThrowIfFailed(
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)),
        "CoCreateInstance WIC failed"
    );

    const std::filesystem::path dir = ExeDirectory() / "GeneratedAlphaPng";
    std::filesystem::create_directories(dir);

    std::vector<Image> images;
    for (int i = 0; i < 10; ++i) {
        const std::filesystem::path path = dir / ("thumb_" + std::to_string(i) + ".png");
        GeneratePng(factory, path, i);
        images.push_back(LoadPng(factory, path));
    }

    factory->Release();
    return images;
}

void BlendPixel(std::vector<std::uint8_t>& bgra, int surfaceWidth, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    if (x < 0 || y < 0 || x >= surfaceWidth) {
        return;
    }
    const std::size_t offset = static_cast<std::size_t>((y * surfaceWidth + x) * 4);
    const float srcA = static_cast<float>(a) / 255.0f;
    const float dstA = static_cast<float>(bgra[offset + 3]) / 255.0f;
    const float outA = srcA + dstA * (1.0f - srcA);
    if (outA <= 0.0f) {
        return;
    }
    auto blend = [&](std::uint8_t src, std::uint8_t dst) {
        return static_cast<std::uint8_t>((static_cast<float>(src) * srcA + static_cast<float>(dst) * dstA * (1.0f - srcA)) / outA);
    };
    bgra[offset + 0] = blend(b, bgra[offset + 0]);
    bgra[offset + 1] = blend(g, bgra[offset + 1]);
    bgra[offset + 2] = blend(r, bgra[offset + 2]);
    bgra[offset + 3] = static_cast<std::uint8_t>(outA * 255.0f);
}

void FillRectAlpha(std::vector<std::uint8_t>& bgra, int width, int height, RECT rect, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    const int left = std::clamp<LONG>(rect.left, 0, width);
    const int right = std::clamp<LONG>(rect.right, 0, width);
    const int top = std::clamp<LONG>(rect.top, 0, height);
    const int bottom = std::clamp<LONG>(rect.bottom, 0, height);
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            BlendPixel(bgra, width, x, y, r, g, b, a);
        }
    }
}

void DrawImage(std::vector<std::uint8_t>& bgra, int surfaceWidth, int surfaceHeight, const Image& image, int x0, int y0, int targetSize) {
    for (int y = 0; y < targetSize; ++y) {
        for (int x = 0; x < targetSize; ++x) {
            const int srcX = x * image.width / targetSize;
            const int srcY = y * image.height / targetSize;
            const std::size_t src = static_cast<std::size_t>((srcY * image.width + srcX) * 4);
            const int dstX = x0 + x;
            const int dstY = y0 + y;
            if (dstX >= 0 && dstY >= 0 && dstX < surfaceWidth && dstY < surfaceHeight) {
                BlendPixel(bgra, surfaceWidth, dstX, dstY, image.rgba[src], image.rgba[src + 1], image.rgba[src + 2], image.rgba[src + 3]);
            }
        }
    }
}

void DrawTextOnSurface(std::vector<std::uint8_t>& bgra, int width, int height) {
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
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    SelectObject(dc, bitmap);
    std::memcpy(bits, bgra.data(), bgra.size());

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    HFONT font = CreateFontW(22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    SelectObject(dc, font);
    RECT textRect{26, 24, width - 26, 58};
    DrawTextW(dc, L"透明桌面窗口验证：中文文本", -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    std::memcpy(bgra.data(), bits, bgra.size());
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

std::vector<ItemRect> Render(HWND hwnd, int width, int height, const std::vector<Image>& images, int hoverIndex) {
    std::vector<std::uint8_t> bgra(static_cast<std::size_t>(width * height * 4), 0);
    FillRectAlpha(bgra, width, height, {0, 0, width, height}, 16, 18, 22, 120);
    FillRectAlpha(bgra, width, height, {18, 18, width - 18, 68}, 30, 36, 44, 210);

    std::vector<ItemRect> items;
    for (int i = 0; i < static_cast<int>(images.size()); ++i) {
        const int col = i % 5;
        const int row = i / 5;
        const int x = 30 + col * 86;
        const int y = 92 + row * 88;
        RECT rect{x, y, x + 72, y + 72};
        FillRectAlpha(bgra, width, height, {x - 4, y - 4, x + 76, y + 76}, hoverIndex == i ? 255 : 240, hoverIndex == i ? 255 : 255, hoverIndex == i ? 255 : 255, hoverIndex == i ? 90 : 38);
        DrawImage(bgra, width, height, images[static_cast<std::size_t>(i)], x, y, 72);
        items.push_back({rect, i});
    }

    const int preview = hoverIndex >= 0 ? hoverIndex : 0;
    FillRectAlpha(bgra, width, height, {178, 286, 322, 430}, 255, 255, 255, 46);
    DrawImage(bgra, width, height, images[static_cast<std::size_t>(preview)], 186, 294, 128);
    DrawTextOnSurface(bgra, width, height);

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    std::memcpy(bits, bgra.data(), bgra.size());
    HBITMAP old = static_cast<HBITMAP>(SelectObject(mem, bitmap));

    POINT source{0, 0};
    SIZE size{width, height};
    POINT pos{};
    RECT windowRect{};
    GetWindowRect(hwnd, &windowRect);
    pos.x = windowRect.left;
    pos.y = windowRect.top;
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(hwnd, screen, &pos, &size, mem, &source, 0, &blend, ULW_ALPHA);

    SelectObject(mem, old);
    DeleteObject(bitmap);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);

    return items;
}

Engine::PointI PositionAboveMouse(int width, int height) {
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);

    Engine::RectI workArea{
        info.rcWork.left,
        info.rcWork.top,
        info.rcWork.right - info.rcWork.left,
        info.rcWork.bottom - info.rcWork.top
    };
    return Engine::ClampWindowPositionToWorkArea({cursor.x - width / 2, cursor.y - height - 24}, width, height, workArea);
}

int HitTest(const std::vector<ItemRect>& items, POINT client) {
    for (const ItemRect& item : items) {
        if (PtInRect(&item.rect, client)) {
            return item.index;
        }
    }
    return -1;
}

std::vector<ItemRect> BuildItemRects() {
    std::vector<ItemRect> items;
    for (int i = 0; i < 10; ++i) {
        const int col = i % 5;
        const int row = i / 5;
        const int x = 30 + col * 86;
        const int y = 92 + row * 88;
        items.push_back({RECT{x, y, x + 72, y + 72}, i});
    }
    return items;
}

void BuildDesktopDrawList(
    const std::vector<Engine::TextureHandle>& textures,
    Engine::TextLayoutHandle titleText,
    Engine::DrawList2D& drawList,
    int hoverIndex
) {
    using namespace Engine;
    drawList.Clear();
    drawList.Reserve(64);

    drawList.AddSolidRect(SolidRectCommand{{0, 0, 500, 500}, {0.063f, 0.071f, 0.086f, 0.47f}});
    drawList.AddRoundedRect(RoundedRectCommand{{18, 18, 464, 50}, 12.0f, {0.118f, 0.141f, 0.173f, 0.82f}});
    drawList.AddText(TextCommand{{26, 24}, titleText, {1.0f, 1.0f, 1.0f, 0.95f}});

    const Vec2F center{250.0f, 356.0f};
    drawList.AddRing(RingCommand{center, 148.0f, 2.0f, {0.78f, 0.80f, 0.84f, 0.35f}});
    drawList.AddRing(RingCommand{center, 110.0f, 2.0f, {0.90f, 0.92f, 0.96f, 0.28f}});

    for (int i = 0; i < 10; ++i) {
        const int col = i % 5;
        const int row = i / 5;
        const float x = 30.0f + static_cast<float>(col * 86);
        const float y = 92.0f + static_cast<float>(row * 88);
        drawList.AddLine(LineCommand{{x + 36.0f, y + 36.0f}, center, 1.0f, {0.74f, 0.76f, 0.80f, 0.16f}});
    }

    for (int i = 0; i < 10; ++i) {
        const int col = i % 5;
        const int row = i / 5;
        const float x = 30.0f + static_cast<float>(col * 86);
        const float y = 92.0f + static_cast<float>(row * 88);
        drawList.AddRoundedRect(RoundedRectCommand{{x - 4.0f, y - 4.0f, 80.0f, 80.0f}, 10.0f, {1.0f, 1.0f, 1.0f, hoverIndex == i ? 0.35f : 0.15f}});
        drawList.AddSprite(SpriteCommand{{x, y, 72.0f, 72.0f}, {0, 0, 1, 1}, textures[static_cast<std::size_t>(i)], {1, 1, 1, 1}, AlphaMode::Straight});
    }

    const int preview = hoverIndex >= 0 ? hoverIndex : 0;
    drawList.AddRoundedRect(RoundedRectCommand{{178, 286, 144, 144}, 14.0f, {1.0f, 1.0f, 1.0f, 0.18f}});
    drawList.AddSprite(SpriteCommand{{186, 294, 128, 128}, {0, 0, 1, 1}, textures[static_cast<std::size_t>(preview)], {1, 1, 1, 1}, AlphaMode::Straight});
    drawList.AddRoundedRect(RoundedRectCommand{{148, 438, 204, 34}, 9.0f, {0.82f, 0.84f, 0.88f, 0.28f}});
}

} // namespace

int main(int argc, char** argv) {
    using namespace Engine;

    constexpr int kWidth = 500;
    constexpr int kHeight = 500;
    const bool selfTest = argc > 1 && std::string(argv[1]) == "--self-test";

    try {
        std::vector<Image> images = BuildPngThumbnails();
        TextureRegistry2D textureRegistry;
        std::vector<TextureHandle> thumbnailTextures;
        thumbnailTextures.reserve(images.size());
        for (const Image& image : images) {
            thumbnailTextures.push_back(textureRegistry.CreateTextureRGBA8(image.width, image.height, image.rgba, AlphaMode::Straight));
        }
        BasicTextLayoutService textLayouts;
        TextLayoutHandle titleText = textLayouts.CreateLayout({}, U"透明桌面窗口验证：中文文本", 430.0f, 22.0f);

        Win32ApplicationHost host;
        Win32WindowSystem windows;

        WindowDesc desc{};
        desc.title = "XiE DesktopSurfaceTest";
        desc.width = kWidth;
        desc.height = kHeight;
        desc.visible = false;
        desc.resizable = false;
        desc.decorated = false;
        desc.transparent = true;
        desc.alwaysOnTop = true;
        desc.toolWindow = true;

        WindowHandle window = windows.CreateWindow(desc);
        HWND hwnd = reinterpret_cast<HWND>(windows.GetNativeHandle(window));
        const PointI position = PositionAboveMouse(kWidth, kHeight);
        windows.SetWindowPosition(window, position.x, position.y);
        if (!selfTest) {
            windows.ShowWindow(window);
        }

        WindowContext context{};
        context.window = window;
        context.visible = !selfTest;
        context.dirty = true;
        context.framePolicy = selfTest ? FramePolicy::Static : FramePolicy::VisibleIdle;

        std::cout << "[Window] Created hwnd=" << hwnd << " size=" << kWidth << "x" << kHeight << '\n';
        std::cout << "[DPI] scale=" << windows.GetDpiScale(window) << '\n';
        std::cout << "[Render] PNG alpha thumbnails loaded=" << images.size() << '\n';

        Software2DRenderer softwareRenderer(&textureRegistry, &textLayouts);
        LayeredWindowPresenter presenter(windows.GetNativeHandle(window));
        LayeredWindowRenderPipeline renderPipeline(softwareRenderer, presenter);
        renderPipeline.Resize(kWidth, kHeight, windows.GetDpiScale(window));
        DrawList2D drawList;
        std::vector<ItemRect> items = BuildItemRects();

        if (selfTest) {
            BuildDesktopDrawList(thumbnailTextures, titleText, drawList, -1);
            (void)renderPipeline.Render(drawList, Render2DView{static_cast<float>(kWidth), static_cast<float>(kHeight), 1.0f});
            renderPipeline.Present();
            std::cout << "[HitTest] item=" << (items.empty() ? -1 : items.front().index) << '\n';
            context.framePolicy = FramePolicy::Hidden;
            context.visible = false;
            std::cout << "[FrameScheduler] Static -> Hidden\n";
            FrameScheduler selfTestScheduler;
            const FrameDecision decision = selfTestScheduler.Evaluate(context, false, false, 0.0);
            if (!decision.update && !decision.render) {
                std::cout << "[Render] Stopped while hidden\n";
            }
            windows.DestroyWindow(window);
            CoUninitialize();
            return 0;
        }

        FrameScheduler scheduler;
        int hoverIndex = -1;
        int lastLoggedHit = -1;
        bool hiddenLogged = false;
        bool leftWasDown = false;

        const auto start = std::chrono::steady_clock::now();
        while (!host.ShouldExit()) {
            host.PollEvents();
            context.visible = windows.IsVisible(window);
            context.closeRequested = windows.IsCloseRequested(window);
            if (context.closeRequested) {
                host.RequestExit();
                break;
            }

            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0 && context.framePolicy != FramePolicy::Hidden) {
                windows.HideWindow(window);
                context.visible = false;
                context.framePolicy = FramePolicy::Hidden;
                context.dirty = false;
                std::cout << "[FrameScheduler] VisibleIdle -> Hidden\n";
            }

            if ((GetAsyncKeyState(VK_F2) & 0x8000) != 0 && context.framePolicy == FramePolicy::Hidden) {
                windows.ShowWindow(window);
                context.visible = true;
                context.framePolicy = FramePolicy::VisibleIdle;
                context.dirty = true;
                hiddenLogged = false;
                std::cout << "[FrameScheduler] Hidden -> VisibleIdle\n";
            }

            POINT cursor{};
            GetCursorPos(&cursor);
            POINT client = cursor;
            ScreenToClient(hwnd, &client);
            const int newHover = HitTest(items, client);
            if (newHover != hoverIndex) {
                hoverIndex = newHover;
                context.dirty = true;
            }

            const bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            if (leftDown && !leftWasDown && hoverIndex >= 0 && hoverIndex != lastLoggedHit) {
                std::cout << "[HitTest] item=" << hoverIndex << '\n';
                lastLoggedHit = hoverIndex;
            }
            leftWasDown = leftDown;

            const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const FrameDecision decision = scheduler.Evaluate(context, false, false, now);
            if (decision.update || decision.render) {
                if (decision.render) {
                    BuildDesktopDrawList(thumbnailTextures, titleText, drawList, hoverIndex);
                    (void)renderPipeline.Render(drawList, Render2DView{
                        static_cast<float>(kWidth), static_cast<float>(kHeight), windows.GetDpiScale(window)});
                    renderPipeline.Present();
                    context.dirty = false;
                }
            } else {
                if (context.framePolicy == FramePolicy::Hidden && !hiddenLogged) {
                    std::cout << "[Render] Stopped while hidden\n";
                    hiddenLogged = true;
                }
                host.WaitForEventsTimeout(decision.waitSeconds);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[Error] " << e.what() << '\n';
        return 1;
    }

    return 0;
}
