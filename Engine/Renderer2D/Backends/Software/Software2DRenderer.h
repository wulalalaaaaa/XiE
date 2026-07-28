#pragma once

#include "Renderer2D/DrawList2D.h"
#include "Renderer2D/TextLayoutService.h"
#include "Renderer2D/TextureRegistry2D.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Engine {

struct PremultipliedBitmap {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgra;
};

class Software2DRenderer {
public:
    explicit Software2DRenderer(const TextureRegistry2D* textures = nullptr, const ITextLayoutService* text = nullptr);

    void SetTextureRegistry(const TextureRegistry2D* textures);
    void SetTextLayoutService(const ITextLayoutService* text);
    void Resize(int pixelWidth, int pixelHeight);

    void Render(const DrawList2D& drawList, float dpiScale);

    [[nodiscard]] const PremultipliedBitmap& GetBitmap() const;
    [[nodiscard]] const Render2DStats& GetStats() const;

private:
    struct PixelPremul {
        float b = 0.0f;
        float g = 0.0f;
        float r = 0.0f;
        float a = 0.0f;
    };

    struct ClipI {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
    };

    void Clear();
    ClipI CurrentClip() const;
    ClipI ToClip(RectF rect, float dpiScale) const;

    void DrawSprite(const SpriteCommand& command, float dpiScale);
    void DrawSolidRect(const SolidRectCommand& command, float dpiScale);
    void DrawRoundedRect(const RoundedRectCommand& command, float dpiScale);
    void DrawLine(const LineCommand& command, float dpiScale);
    void DrawCircle(const CircleCommand& command, float dpiScale);
    void DrawRing(const RingCommand& command, float dpiScale);
    void DrawNineSlice(const NineSliceCommand& command, float dpiScale);
    void DrawText(const TextCommand& command, float dpiScale);

    void FillRect(RectF rect, const Mat3F& transform, Color4f color,
        AlphaMode alphaMode, BlendMode blendMode, float dpiScale);
    void RasterTransformed(
        RectF localBounds,
        const Mat3F& transform,
        float dpiScale,
        const std::function<void(int, int, Vec2F)>& visitor);
    void BlendPixel(int x, int y, PixelPremul src, BlendMode blendMode);
    PixelPremul ToPremul(Color4f color, AlphaMode alphaMode) const;

private:
    PremultipliedBitmap m_Bitmap;
    const TextureRegistry2D* m_Textures = nullptr;
    const ITextLayoutService* m_Text = nullptr;
    std::vector<ClipI> m_ClipStack;
    Render2DStats m_Stats{};
};

} // namespace Engine
