#include "Application/FrameScheduler.h"
#include "Renderer2D/Backends/Software/Software2DRenderer.h"
#include "Renderer2D/DrawListValidator.h"
#include "Renderer2D/TextLayoutService.h"
#include "Renderer2D/TextureRegistry2D.h"

#include <cstdlib>
#include <iostream>

namespace {

int g_Failures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        ++g_Failures;
        std::cerr << "[FAIL] " << message << '\n';
    } else {
        std::cout << "[PASS] " << message << '\n';
    }
}

std::uint8_t PixelA(const Engine::PremultipliedBitmap& bitmap, int x, int y) {
    return bitmap.bgra[static_cast<std::size_t>((y * bitmap.width + x) * 4 + 3)];
}

std::uint8_t PixelR(const Engine::PremultipliedBitmap& bitmap, int x, int y) {
    return bitmap.bgra[static_cast<std::size_t>((y * bitmap.width + x) * 4 + 2)];
}

} // namespace

int main() {
    using namespace Engine;

    {
        DrawList2D list;
        list.AddSolidRect({{0, 0, 1, 1}, {1, 0, 0, 1}});
        list.AddCircle({{4, 4}, 2, {0, 1, 0, 1}});
        Expect(std::holds_alternative<SolidRectCommand>(list.Commands()[0]), "DrawList keeps first submitted command");
        Expect(std::holds_alternative<CircleCommand>(list.Commands()[1]), "DrawList keeps second submitted command");
    }

    {
        DrawList2D balanced;
        balanced.PushClipRect({0, 0, 4, 4});
        balanced.PopClip();
        DrawListValidator validator;
        Expect(validator.Validate(balanced).valid, "Balanced clip stack validates");

        DrawList2D unbalanced;
        unbalanced.PushClipRect({0, 0, 4, 4});
        Expect(!validator.Validate(unbalanced).valid, "Unbalanced clip stack is detected");
    }

    {
        const float scales[] = {1.0f, 1.25f, 1.5f, 2.0f};
        for (float scale : scales) {
            Software2DRenderer renderer;
            renderer.Resize(static_cast<int>(10 * scale), static_cast<int>(10 * scale));
            DrawList2D list;
            list.AddSolidRect({{1, 1, 2, 2}, {1, 0, 0, 0.5f}, AlphaMode::Straight});
            renderer.Render(list, scale);
            Expect(renderer.GetBitmap().width == static_cast<int>(10 * scale), "Software renderer keeps pixel width for DPI scale");
        }
    }

    {
        Software2DRenderer renderer;
        renderer.Resize(4, 4);
        DrawList2D list;
        list.AddSolidRect({{0, 0, 4, 4}, {1, 0, 0, 0.5f}, AlphaMode::Straight});
        renderer.Render(list, 1.0f);
        Expect(PixelA(renderer.GetBitmap(), 1, 1) == 128, "Straight alpha blends half alpha onto transparent");
        Expect(PixelR(renderer.GetBitmap(), 1, 1) == 128, "Straight alpha premultiplies RGB in BGRA output");
    }

    {
        Software2DRenderer renderer;
        renderer.Resize(4, 4);
        DrawList2D list;
        list.AddSolidRect({{0, 0, 4, 4}, {0.5f, 0, 0, 0.5f}, AlphaMode::Premultiplied});
        renderer.Render(list, 1.0f);
        Expect(PixelA(renderer.GetBitmap(), 1, 1) == 128, "Premultiplied alpha preserves source alpha");
        Expect(PixelR(renderer.GetBitmap(), 1, 1) == 128, "Premultiplied alpha preserves premul red");
    }

    {
        TextureRegistry2D textures;
        const std::uint8_t edgePixels[] = {
            255, 0, 0, 0, 255, 0, 0, 128,
            255, 0, 0, 128, 255, 0, 0, 255
        };
        TextureHandle texture = textures.CreateTextureRGBA8(2, 2, edgePixels, AlphaMode::Straight);
        Software2DRenderer renderer(&textures, nullptr);
        renderer.Resize(4, 4);
        DrawList2D list;
        list.AddSprite({{0, 0, 4, 4}, {0, 0, 1, 1}, texture, {1, 1, 1, 1}, AlphaMode::Straight});
        renderer.Render(list, 1.0f);
        Expect(renderer.GetBitmap().bgra[0] == 0 && renderer.GetBitmap().bgra[1] == 0 && renderer.GetBitmap().bgra[2] == 0, "Transparent PNG edge does not produce black fringe color");
    }

    {
        TextureRegistry2D textures;
        const std::uint8_t pixels[] = {255, 255, 255, 255};
        TextureHandle texture = textures.CreateTextureRGBA8(1, 1, pixels, AlphaMode::Straight);
        Software2DRenderer renderer(&textures, nullptr);
        renderer.Resize(6, 6);
        DrawList2D list;
        list.PushClipRect({2, 2, 2, 2});
        list.AddSprite({{0, 0, 6, 6}, {0, 0, 1, 1}, texture});
        list.PopClip();
        renderer.Render(list, 1.0f);
        Expect(PixelA(renderer.GetBitmap(), 1, 1) == 0, "Sprite outside clip does not write pixels");
        Expect(PixelA(renderer.GetBitmap(), 2, 2) == 255, "Sprite inside clip writes pixels");
    }

    {
        TextureRegistry2D textures;
        const std::uint8_t pixels[] = {1, 2, 3, 4};
        TextureHandle texture = textures.CreateTextureRGBA8(1, 1, pixels, AlphaMode::Straight);
        textures.Destroy(texture);
        Expect(textures.GetInfo(texture).state == TextureState::Missing, "Destroyed texture handle reports Missing state");
    }

    {
        BasicTextLayoutService text;
        TextLayoutHandle first = text.CreateLayout({}, U"中文缓存", 160.0f, 20.0f);
        TextLayoutHandle second = text.CreateLayout({}, U"中文缓存", 160.0f, 20.0f);
        Expect(first.index == second.index && text.RasterizeCount() == 1, "Text layout cache avoids repeated rasterization");
    }

    {
        WindowContext hidden{};
        hidden.window = {1, 1};
        hidden.visible = false;
        hidden.framePolicy = FramePolicy::Hidden;
        FrameScheduler scheduler;
        const FrameDecision decision = scheduler.Evaluate(hidden, false, false, 0.0);
        Expect(!decision.update && !decision.render, "Hidden window skips DrawList build and render decision");
    }

    if (g_Failures != 0) {
        std::cerr << "Renderer2D tests failed: " << g_Failures << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Renderer2D tests passed\n";
    return EXIT_SUCCESS;
}
