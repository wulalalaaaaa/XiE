#include "Software2DRenderer.h"

#include <algorithm>
#include <cmath>

namespace Engine {

namespace {

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

int FloorToInt(float value) {
    return static_cast<int>(std::floor(value));
}

int CeilToInt(float value) {
    return static_cast<int>(std::ceil(value));
}

float DistanceToSegment(float px, float py, Vec2F a, Vec2F b) {
    const float vx = b.x - a.x;
    const float vy = b.y - a.y;
    const float wx = px - a.x;
    const float wy = py - a.y;
    const float len2 = vx * vx + vy * vy;
    const float t = len2 > 0.0f ? std::clamp((wx * vx + wy * vy) / len2, 0.0f, 1.0f) : 0.0f;
    const float dx = px - (a.x + t * vx);
    const float dy = py - (a.y + t * vy);
    return std::sqrt(dx * dx + dy * dy);
}

RectF TransformBounds(const Mat3F& transform, RectF rect) {
    const Vec2F points[] = {
        TransformPoint(transform, {rect.x, rect.y}),
        TransformPoint(transform, {rect.x + rect.width, rect.y}),
        TransformPoint(transform, {rect.x, rect.y + rect.height}),
        TransformPoint(transform, {rect.x + rect.width, rect.y + rect.height})};
    float minX = points[0].x, maxX = points[0].x;
    float minY = points[0].y, maxY = points[0].y;
    for (Vec2F point : points) {
        minX = std::min(minX, point.x); maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y); maxY = std::max(maxY, point.y);
    }
    return {minX, minY, maxX - minX, maxY - minY};
}

} // namespace

Software2DRenderer::Software2DRenderer(const TextureRegistry2D* textures, const ITextLayoutService* text)
    : m_Textures(textures)
    , m_Text(text) {}

void Software2DRenderer::SetTextureRegistry(const TextureRegistry2D* textures) {
    m_Textures = textures;
}

void Software2DRenderer::SetTextLayoutService(const ITextLayoutService* text) {
    m_Text = text;
}

void Software2DRenderer::Resize(int pixelWidth, int pixelHeight) {
    pixelWidth = std::max(1, pixelWidth);
    pixelHeight = std::max(1, pixelHeight);
    if (m_Bitmap.width == pixelWidth && m_Bitmap.height == pixelHeight && m_Bitmap.bgra.size() == static_cast<std::size_t>(pixelWidth * pixelHeight * 4)) {
        return;
    }
    m_Bitmap.width = pixelWidth;
    m_Bitmap.height = pixelHeight;
    m_Bitmap.bgra.assign(static_cast<std::size_t>(pixelWidth * pixelHeight * 4), 0);
}

void Software2DRenderer::Render(const DrawList2D& drawList, float dpiScale) {
    Clear();
    m_Stats = {};
    m_Stats.commandCount = static_cast<std::uint32_t>(drawList.Commands().size());

    for (const DrawCommand2D& command : drawList.Commands()) {
        std::visit([&](const auto& typed) {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, SpriteCommand>) {
                DrawSprite(typed, dpiScale);
            } else if constexpr (std::is_same_v<T, SolidRectCommand>) {
                DrawSolidRect(typed, dpiScale);
            } else if constexpr (std::is_same_v<T, RoundedRectCommand>) {
                DrawRoundedRect(typed, dpiScale);
            } else if constexpr (std::is_same_v<T, LineCommand>) {
                DrawLine(typed, dpiScale);
            } else if constexpr (std::is_same_v<T, CircleCommand>) {
                DrawCircle(typed, dpiScale);
            } else if constexpr (std::is_same_v<T, RingCommand>) {
                DrawRing(typed, dpiScale);
            } else if constexpr (std::is_same_v<T, NineSliceCommand>) {
                DrawNineSlice(typed, dpiScale);
            } else if constexpr (std::is_same_v<T, TextCommand>) {
                DrawText(typed, dpiScale);
            } else if constexpr (std::is_same_v<T, PushClipRectCommand>) {
                m_ClipStack.push_back(ToClip(typed.rect, dpiScale));
                ++m_Stats.clipChangeCount;
            } else if constexpr (std::is_same_v<T, PopClipCommand>) {
                if (!m_ClipStack.empty()) {
                    m_ClipStack.pop_back();
                    ++m_Stats.clipChangeCount;
                }
            } else if constexpr (std::is_same_v<T, CustomMeshCommand>) {
                (void)typed;
            }
        }, command);
    }
}

const PremultipliedBitmap& Software2DRenderer::GetBitmap() const {
    return m_Bitmap;
}

const Render2DStats& Software2DRenderer::GetStats() const {
    return m_Stats;
}

void Software2DRenderer::Clear() {
    std::fill(m_Bitmap.bgra.begin(), m_Bitmap.bgra.end(), 0);
    m_ClipStack.clear();
}

Software2DRenderer::ClipI Software2DRenderer::CurrentClip() const {
    ClipI clip{0, 0, m_Bitmap.width, m_Bitmap.height};
    for (const ClipI& pushed : m_ClipStack) {
        clip.x0 = std::max(clip.x0, pushed.x0);
        clip.y0 = std::max(clip.y0, pushed.y0);
        clip.x1 = std::min(clip.x1, pushed.x1);
        clip.y1 = std::min(clip.y1, pushed.y1);
    }
    return clip;
}

Software2DRenderer::ClipI Software2DRenderer::ToClip(RectF rect, float dpiScale) const {
    const float s = dpiScale > 0.0f ? dpiScale : 1.0f;
    return {
        std::clamp(FloorToInt(rect.x * s), 0, m_Bitmap.width),
        std::clamp(FloorToInt(rect.y * s), 0, m_Bitmap.height),
        std::clamp(CeilToInt((rect.x + rect.width) * s), 0, m_Bitmap.width),
        std::clamp(CeilToInt((rect.y + rect.height) * s), 0, m_Bitmap.height)
    };
}

void Software2DRenderer::DrawSolidRect(const SolidRectCommand& command, float dpiScale) {
    FillRect(command.rect, command.transform, command.color, command.alphaMode, command.blendMode, dpiScale);
}

void Software2DRenderer::DrawRoundedRect(const RoundedRectCommand& command, float dpiScale) {
    const float radius = std::max(0.0f, command.radius);
    const float r = radius;
    const PixelPremul color = ToPremul(command.color, command.alphaMode);
    RasterTransformed(command.rect, command.transform, dpiScale, [&](int x, int y, Vec2F local) {
        const float cx = std::clamp(local.x, command.rect.x + r, command.rect.x + command.rect.width - r);
        const float cy = std::clamp(local.y, command.rect.y + r, command.rect.y + command.rect.height - r);
        const float dx = local.x - cx; const float dy = local.y - cy;
        if (dx * dx + dy * dy <= r * r || r <= 0.0f) BlendPixel(x, y, color, command.blendMode);
    });
    ++m_Stats.drawCallCount;
    ++m_Stats.quadCount;
}

void Software2DRenderer::DrawLine(const LineCommand& command, float dpiScale) {
    const float half = std::max(0.0f, command.thickness * 0.5f);
    const RectF bounds{std::min(command.from.x, command.to.x) - half,
        std::min(command.from.y, command.to.y) - half,
        std::abs(command.to.x - command.from.x) + half * 2.0f,
        std::abs(command.to.y - command.from.y) + half * 2.0f};
    const PixelPremul color = ToPremul(command.color, command.alphaMode);
    RasterTransformed(bounds, command.transform, dpiScale, [&](int x, int y, Vec2F local) {
        if (DistanceToSegment(local.x, local.y, command.from, command.to) <= half)
            BlendPixel(x, y, color, command.blendMode);
    });
    ++m_Stats.drawCallCount;
}

void Software2DRenderer::DrawCircle(const CircleCommand& command, float dpiScale) {
    const float r = command.radius;
    const PixelPremul color = ToPremul(command.color, command.alphaMode);
    RasterTransformed({command.center.x-r, command.center.y-r, r*2, r*2}, command.transform, dpiScale,
        [&](int x, int y, Vec2F local) {
            const float dx = local.x-command.center.x, dy = local.y-command.center.y;
            if (dx*dx+dy*dy <= r*r) BlendPixel(x,y,color,command.blendMode);
        });
    ++m_Stats.drawCallCount;
}

void Software2DRenderer::DrawRing(const RingCommand& command, float dpiScale) {
    const float outer = command.radius;
    const float inner = std::max(0.0f, outer - command.thickness);
    const PixelPremul color = ToPremul(command.color, command.alphaMode);
    RasterTransformed({command.center.x-outer, command.center.y-outer, outer*2, outer*2}, command.transform, dpiScale,
        [&](int x, int y, Vec2F local) {
            const float dx=local.x-command.center.x, dy=local.y-command.center.y, d2=dx*dx+dy*dy;
            if(d2<=outer*outer && d2>=inner*inner) BlendPixel(x,y,color,command.blendMode);
        });
    ++m_Stats.drawCallCount;
}

void Software2DRenderer::DrawSprite(const SpriteCommand& command, float dpiScale) {
    const TexturePixels2D* texture = m_Textures != nullptr ? m_Textures->Get(command.texture) : nullptr;
    if (texture == nullptr || texture->rgba.empty()) {
        return;
    }

    const PixelPremul tint = ToPremul(command.color, command.alphaMode);
    RasterTransformed(command.dst, command.transform, dpiScale, [&](int x, int y, Vec2F local) {
        const float v = (local.y - command.dst.y) / command.dst.height;
        const float tv = command.uv.y + v * command.uv.height;
        const int sy = std::clamp(static_cast<int>(tv * static_cast<float>(texture->info.height)), 0, texture->info.height - 1);
        const float u = (local.x - command.dst.x) / command.dst.width;
        const float tu = command.uv.x + u * command.uv.width;
        const int sx = std::clamp(static_cast<int>(tu * static_cast<float>(texture->info.width)), 0, texture->info.width - 1);
        const std::size_t src = static_cast<std::size_t>((sy * texture->info.width + sx) * 4);
        Color4f sample{static_cast<float>(texture->rgba[src+0])/255.0f,
            static_cast<float>(texture->rgba[src+1])/255.0f,
            static_cast<float>(texture->rgba[src+2])/255.0f,
            static_cast<float>(texture->rgba[src+3])/255.0f};
        PixelPremul premul=ToPremul(sample,texture->info.alphaMode);
        premul.r*=tint.r; premul.g*=tint.g; premul.b*=tint.b; premul.a*=tint.a;
        BlendPixel(x,y,premul,command.blendMode);
    });
    ++m_Stats.drawCallCount;
    ++m_Stats.quadCount;
    ++m_Stats.textureBindCount;
}

void Software2DRenderer::DrawNineSlice(const NineSliceCommand& command, float dpiScale) {
    const float x0 = command.dst.x;
    const float x1 = command.dst.x + command.left;
    const float x2 = command.dst.x + command.dst.width - command.right;
    const float x3 = command.dst.x + command.dst.width;
    const float y0 = command.dst.y;
    const float y1 = command.dst.y + command.top;
    const float y2 = command.dst.y + command.dst.height - command.bottom;
    const float y3 = command.dst.y + command.dst.height;

    const float u0 = command.uv.x;
    const float u1 = command.uv.x + command.sourceLeftUv;
    const float u2 = command.uv.x + command.uv.width - command.sourceRightUv;
    const float u3 = command.uv.x + command.uv.width;
    const float v0 = command.uv.y;
    const float v1 = command.uv.y + command.sourceTopUv;
    const float v2 = command.uv.y + command.uv.height - command.sourceBottomUv;
    const float v3 = command.uv.y + command.uv.height;

    const float xs[] = {x0, x1, x2, x3};
    const float ys[] = {y0, y1, y2, y3};
    const float us[] = {u0, u1, u2, u3};
    const float vs[] = {v0, v1, v2, v3};

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const float w = xs[col + 1] - xs[col];
            const float h = ys[row + 1] - ys[row];
            if (w <= 0.0f || h <= 0.0f) {
                continue;
            }
            SpriteCommand sprite{};
            sprite.dst = {xs[col], ys[row], w, h};
            sprite.uv = {us[col], vs[row], us[col + 1] - us[col], vs[row + 1] - vs[row]};
            sprite.texture = command.texture;
            sprite.color = command.color;
            sprite.alphaMode = command.alphaMode;
            sprite.blendMode = command.blendMode;
            sprite.transform = command.transform;
            DrawSprite(sprite, dpiScale);
        }
    }
}

void Software2DRenderer::DrawText(const TextCommand& command, float dpiScale) {
    const TextLayoutBitmap* bitmap = m_Text != nullptr ? m_Text->GetBitmap(command.layout) : nullptr;
    if (bitmap == nullptr || bitmap->bgraPremultiplied.empty()) {
        return;
    }

    const PixelPremul tint = ToPremul(command.color, command.alphaMode);
    const RectF bounds{command.position.x,command.position.y,static_cast<float>(bitmap->width),static_cast<float>(bitmap->height)};
    RasterTransformed(bounds, command.transform, dpiScale, [&](int x,int y,Vec2F local){
        const int sx=std::clamp(static_cast<int>(local.x-command.position.x),0,bitmap->width-1);
        const int sy=std::clamp(static_cast<int>(local.y-command.position.y),0,bitmap->height-1);
        const std::size_t src=static_cast<std::size_t>((sy*bitmap->width+sx)*4);
        const float a=static_cast<float>(bitmap->bgraPremultiplied[src+3])/255.0f;
        BlendPixel(x,y,{tint.b*a,tint.g*a,tint.r*a,tint.a*a},command.blendMode);
    });
    ++m_Stats.drawCallCount;
}

void Software2DRenderer::FillRect(RectF rect, const Mat3F& transform, Color4f color, AlphaMode alphaMode, BlendMode blendMode, float dpiScale) {
    const PixelPremul premul = ToPremul(color, alphaMode);
    RasterTransformed(rect, transform, dpiScale, [&](int x,int y,Vec2F){ BlendPixel(x,y,premul,blendMode); });
    ++m_Stats.drawCallCount;
    ++m_Stats.quadCount;
}

void Software2DRenderer::RasterTransformed(
    RectF localBounds, const Mat3F& transform, float dpiScale,
    const std::function<void(int, int, Vec2F)>& visitor) {
    Mat3F inverse{};
    if (localBounds.width <= 0.0f || localBounds.height <= 0.0f || !TryInverseAffine(transform, inverse)) return;
    const RectF bounds = TransformBounds(transform, localBounds);
    const float s = dpiScale > 0.0f ? dpiScale : 1.0f;
    const ClipI clip = CurrentClip();
    const int x0=std::max(clip.x0,FloorToInt(bounds.x*s));
    const int y0=std::max(clip.y0,FloorToInt(bounds.y*s));
    const int x1=std::min(clip.x1,CeilToInt((bounds.x+bounds.width)*s));
    const int y1=std::min(clip.y1,CeilToInt((bounds.y+bounds.height)*s));
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const Vec2F scene{(static_cast<float>(x)+0.5f)/s,(static_cast<float>(y)+0.5f)/s};
            const Vec2F local=TransformPoint(inverse,scene);
            if(local.x>=localBounds.x && local.y>=localBounds.y &&
                local.x<=localBounds.x+localBounds.width && local.y<=localBounds.y+localBounds.height)
                visitor(x,y,local);
        }
    }
}

void Software2DRenderer::BlendPixel(int x, int y, PixelPremul src, BlendMode blendMode) {
    if (x < 0 || y < 0 || x >= m_Bitmap.width || y >= m_Bitmap.height) {
        return;
    }

    const std::size_t offset = static_cast<std::size_t>((y * m_Bitmap.width + x) * 4);
    PixelPremul dst{
        static_cast<float>(m_Bitmap.bgra[offset + 0]) / 255.0f,
        static_cast<float>(m_Bitmap.bgra[offset + 1]) / 255.0f,
        static_cast<float>(m_Bitmap.bgra[offset + 2]) / 255.0f,
        static_cast<float>(m_Bitmap.bgra[offset + 3]) / 255.0f
    };

    PixelPremul out{};
    if (blendMode == BlendMode::Opaque) {
        out = src;
        out.a = 1.0f;
    } else if (blendMode == BlendMode::Additive) {
        out.b = Clamp01(dst.b + src.b);
        out.g = Clamp01(dst.g + src.g);
        out.r = Clamp01(dst.r + src.r);
        out.a = Clamp01(dst.a + src.a);
    } else {
        out.b = src.b + dst.b * (1.0f - src.a);
        out.g = src.g + dst.g * (1.0f - src.a);
        out.r = src.r + dst.r * (1.0f - src.a);
        out.a = src.a + dst.a * (1.0f - src.a);
    }

    m_Bitmap.bgra[offset + 0] = static_cast<std::uint8_t>(Clamp01(out.b) * 255.0f + 0.5f);
    m_Bitmap.bgra[offset + 1] = static_cast<std::uint8_t>(Clamp01(out.g) * 255.0f + 0.5f);
    m_Bitmap.bgra[offset + 2] = static_cast<std::uint8_t>(Clamp01(out.r) * 255.0f + 0.5f);
    m_Bitmap.bgra[offset + 3] = static_cast<std::uint8_t>(Clamp01(out.a) * 255.0f + 0.5f);
}

Software2DRenderer::PixelPremul Software2DRenderer::ToPremul(Color4f color, AlphaMode alphaMode) const {
    color.r = Clamp01(color.r);
    color.g = Clamp01(color.g);
    color.b = Clamp01(color.b);
    color.a = Clamp01(color.a);
    if (alphaMode == AlphaMode::Straight) {
        return {color.b * color.a, color.g * color.a, color.r * color.a, color.a};
    }
    return {color.b, color.g, color.r, color.a};
}

} // namespace Engine
