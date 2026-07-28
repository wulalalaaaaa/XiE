#include "OpenGL2DRenderer.h"

#include "Platform/IRenderSurface.h"
#include "Renderer/IRenderBackend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Engine {

namespace {

constexpr TextureHandle kDefaultTexture{1, 1};

void UploadQuad(IRenderBackend& backend, RectF rect, RectF uv, const Mat3F& transform) {
    const Vec2F points[] = {
        TransformPoint(transform, {rect.x, rect.y}),
        TransformPoint(transform, {rect.x + rect.width, rect.y}),
        TransformPoint(transform, {rect.x + rect.width, rect.y + rect.height}),
        TransformPoint(transform, {rect.x, rect.y + rect.height})};
    const float vertices[] = {
        points[0].x, points[0].y, points[1].x, points[1].y,
        points[2].x, points[2].y, points[3].x, points[3].y
    };
    const float uvs[] = {
        uv.x, uv.y,
        uv.x + uv.width, uv.y,
        uv.x + uv.width, uv.y + uv.height,
        uv.x, uv.y + uv.height
    };
    const unsigned int indices[] = {0, 1, 2, 0, 2, 3};
    backend.UploadMesh(vertices, 4, 2, uvs, 4, indices, 6);
}

RectF Intersect(RectF a, RectF b) {
    const float x0 = std::max(a.x, b.x);
    const float y0 = std::max(a.y, b.y);
    const float x1 = std::min(a.x + a.width, b.x + b.width);
    const float y1 = std::min(a.y + a.height, b.y + b.height);
    return {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
}

void ApplyScissor(IRenderBackend& backend, RectF clip, float dpi) {
    const int x0 = static_cast<int>(std::floor(clip.x * dpi));
    const int y0 = static_cast<int>(std::floor(clip.y * dpi));
    const int x1 = static_cast<int>(std::ceil((clip.x + clip.width) * dpi));
    const int y1 = static_cast<int>(std::ceil((clip.y + clip.height) * dpi));
    backend.SetScissorRect(x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0));
}

} // namespace

void OpenGL2DRenderer::SetTextLayoutService(const ITextLayoutService* text) {
    if (m_Text != text) {
        m_Text = text;
        m_TextTexturePixels.clear();
        m_TextTextureUploadCount = 0;
    }
}

void OpenGL2DRenderer::SetTextureRegistry(const TextureRegistry2D* textures) {
    m_Textures = textures;
}

void OpenGL2DRenderer::Render(
    IRenderSurface& surface,
    IRenderBackend& backend,
    const DrawList2D& drawList,
    const Render2DView& view
) {
    (void)surface;
    m_Stats = {};
    m_Stats.commandCount = static_cast<std::uint32_t>(drawList.Commands().size());

    backend.SetViewport(static_cast<int>(view.framebufferWidth), static_cast<int>(view.framebufferHeight));
    backend.BeginFrame();

    std::vector<RectF> clips;
    const float dpi = std::max(0.01f, view.dpiScale);
    for (const DrawCommand2D& command : drawList.Commands()) {
        std::visit([&](const auto& typed) {
            using T = std::decay_t<decltype(typed)>;
            if constexpr (std::is_same_v<T, SpriteCommand>) {
                RenderSprite(backend, typed);
            } else if constexpr (std::is_same_v<T, SolidRectCommand>) {
                RenderSolidRect(backend, typed);
            } else if constexpr (std::is_same_v<T, LineCommand>) {
                RenderLine(backend, typed);
            } else if constexpr (std::is_same_v<T, CircleCommand>) {
                RenderCircle(backend, typed);
            } else if constexpr (std::is_same_v<T, RingCommand>) {
                RenderRing(backend, typed);
            } else if constexpr (std::is_same_v<T, CustomMeshCommand>) {
                RenderCustomMesh(backend, typed);
            } else if constexpr (std::is_same_v<T, RoundedRectCommand>) {
                RenderRoundedRect(backend, typed);
            } else if constexpr (std::is_same_v<T, NineSliceCommand>) {
                RenderNineSlice(backend, typed);
            } else if constexpr (std::is_same_v<T, TextCommand>) {
                RenderText(backend, typed);
            } else if constexpr (std::is_same_v<T, PushClipRectCommand>) {
                clips.push_back(clips.empty() ? typed.rect : Intersect(clips.back(), typed.rect));
                const RectF clip = clips.back();
                backend.SetScissorEnabled(true);
                ApplyScissor(backend, clip, dpi);
                ++m_Stats.clipChangeCount;
            } else if constexpr (std::is_same_v<T, PopClipCommand>) {
                if (!clips.empty()) clips.pop_back();
                if (clips.empty()) backend.SetScissorEnabled(false);
                else {
                    const RectF clip = clips.back();
                    ApplyScissor(backend, clip, dpi);
                }
                ++m_Stats.clipChangeCount;
            }
        }, command);
    }

    backend.SetScissorEnabled(false);
    backend.EndFrame();
}

const Render2DStats& OpenGL2DRenderer::GetStats() const {
    return m_Stats;
}

std::uint32_t OpenGL2DRenderer::TextTextureUploadCount() const {
    return m_TextTextureUploadCount;
}

void OpenGL2DRenderer::RenderSprite(IRenderBackend& backend, const SpriteCommand& command) {
    backend.SetBlendMode(command.blendMode, command.alphaMode);
    backend.SetMaterialTint(&command.color.r);
    if (m_Textures != nullptr) {
        const TextureInfo info = m_Textures->GetInfo(command.texture);
        const TexturePixels2D* texture = info.state == TextureState::Ready
            ? m_Textures->Get(command.texture)
            : nullptr;
        if (texture != nullptr && !texture->rgba.empty()) {
            const std::uint64_t handleKey =
                (static_cast<std::uint64_t>(command.texture.generation) << 32u) | command.texture.index;
            const std::uint64_t resourceKey = handleKey ^
                (info.revision + 0x9e3779b97f4a7c15ull + (handleKey << 6u) + (handleKey >> 2u));
            backend.BindTextureRGBA8(
                0x494D470000000000ull ^ resourceKey,
                texture->rgba.data(),
                texture->info.width,
                texture->info.height);
        } else {
            backend.SetTextureEnabled(false);
        }
    } else {
        (void)kDefaultTexture;
        backend.SetTextureEnabled(true);
    }
    UploadQuad(backend, command.dst, command.uv, command.transform);
    backend.DrawMesh();
    ++m_Stats.drawCallCount;
    ++m_Stats.quadCount;
}

void OpenGL2DRenderer::RenderSolidRect(IRenderBackend& backend, const SolidRectCommand& command) {
    backend.SetBlendMode(command.blendMode, command.alphaMode);
    backend.SetMaterialTint(&command.color.r);
    backend.SetTextureEnabled(false);
    UploadQuad(backend, command.rect, {0.0f, 0.0f, 1.0f, 1.0f}, command.transform);
    backend.DrawMesh();
    ++m_Stats.drawCallCount;
    ++m_Stats.quadCount;
}

void OpenGL2DRenderer::RenderRoundedRect(IRenderBackend& backend, const RoundedRectCommand& command) {
    constexpr int kCornerSegments = 8;
    const float radius = std::clamp(command.radius, 0.0f,
        std::min(command.rect.width, command.rect.height) * 0.5f);
    if (radius <= 0.0f) {
        SolidRectCommand solid;
        solid.rect = command.rect; solid.color = command.color;
        solid.alphaMode = command.alphaMode; solid.blendMode = command.blendMode;
        solid.transform = command.transform; RenderSolidRect(backend, solid); return;
    }
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    const Vec2F localCenter{command.rect.x + command.rect.width * 0.5f,
        command.rect.y + command.rect.height * 0.5f};
    const Vec2F center = TransformPoint(command.transform, localCenter);
    vertices.insert(vertices.end(), {center.x, center.y});
    const Vec2F corners[] = {
        {command.rect.x + command.rect.width - radius, command.rect.y + radius},
        {command.rect.x + command.rect.width - radius, command.rect.y + command.rect.height - radius},
        {command.rect.x + radius, command.rect.y + command.rect.height - radius},
        {command.rect.x + radius, command.rect.y + radius}};
    for (int corner = 0; corner < 4; ++corner) {
        const float start = -1.57079632679f + static_cast<float>(corner) * 1.57079632679f;
        for (int segment = 0; segment <= kCornerSegments; ++segment) {
            const float angle = start + 1.57079632679f * static_cast<float>(segment) / kCornerSegments;
            const Vec2F point = TransformPoint(command.transform,
                {corners[corner].x + std::cos(angle) * radius,
                 corners[corner].y + std::sin(angle) * radius});
            vertices.insert(vertices.end(), {point.x, point.y});
        }
    }
    const unsigned int perimeter = static_cast<unsigned int>(vertices.size() / 2 - 1);
    for (unsigned int i = 0; i < perimeter; ++i)
        indices.insert(indices.end(), {0, i + 1, ((i + 1) % perimeter) + 1});
    backend.SetBlendMode(command.blendMode, command.alphaMode);
    backend.SetMaterialTint(&command.color.r); backend.SetTextureEnabled(false);
    backend.UploadMesh(vertices.data(), static_cast<int>(vertices.size() / 2), 2,
        nullptr, 0, indices.data(), static_cast<int>(indices.size()));
    backend.DrawMesh(); ++m_Stats.drawCallCount; ++m_Stats.quadCount;
}

void OpenGL2DRenderer::RenderLine(IRenderBackend& backend, const LineCommand& command) {
    const float dx = command.to.x - command.from.x;
    const float dy = command.to.y - command.from.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0001f) {
        return;
    }
    const float nx = -dy / len * command.thickness * 0.5f;
    const float ny = dx / len * command.thickness * 0.5f;
    const Vec2F points[] = {
        TransformPoint(command.transform, {command.from.x + nx, command.from.y + ny}),
        TransformPoint(command.transform, {command.to.x + nx, command.to.y + ny}),
        TransformPoint(command.transform, {command.to.x - nx, command.to.y - ny}),
        TransformPoint(command.transform, {command.from.x - nx, command.from.y - ny})};
    const float vertices[] = {points[0].x,points[0].y,points[1].x,points[1].y,
        points[2].x,points[2].y,points[3].x,points[3].y};
    const float uvs[] = {0, 0, 1, 0, 1, 1, 0, 1};
    const unsigned int indices[] = {0, 1, 2, 0, 2, 3};
    backend.SetBlendMode(command.blendMode, command.alphaMode);
    backend.SetMaterialTint(&command.color.r);
    backend.SetTextureEnabled(false);
    backend.UploadMesh(vertices, 4, 2, uvs, 4, indices, 6);
    backend.DrawMesh();
    ++m_Stats.drawCallCount;
    ++m_Stats.quadCount;
}

void OpenGL2DRenderer::RenderCircle(IRenderBackend& backend, const CircleCommand& command) {
    constexpr int kSegments = 32;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve((kSegments + 1) * 2);
    indices.reserve(kSegments * 3);
    const Vec2F transformedCenter = TransformPoint(command.transform, command.center);
    vertices.push_back(transformedCenter.x);
    vertices.push_back(transformedCenter.y);
    for (int i = 0; i < kSegments; ++i) {
        const float a = static_cast<float>(i) * 6.28318530718f / static_cast<float>(kSegments);
        const Vec2F point = TransformPoint(command.transform,
            {command.center.x + std::cos(a) * command.radius,
             command.center.y + std::sin(a) * command.radius});
        vertices.push_back(point.x); vertices.push_back(point.y);
    }
    for (int i = 0; i < kSegments; ++i) {
        indices.push_back(0);
        indices.push_back(static_cast<unsigned int>(i + 1));
        indices.push_back(static_cast<unsigned int>(((i + 1) % kSegments) + 1));
    }
    backend.SetBlendMode(command.blendMode, command.alphaMode);
    backend.SetMaterialTint(&command.color.r);
    backend.SetTextureEnabled(false);
    backend.UploadMesh(vertices.data(), static_cast<int>(vertices.size() / 2), 2, nullptr, 0, indices.data(), static_cast<int>(indices.size()));
    backend.DrawMesh();
    ++m_Stats.drawCallCount;
}

void OpenGL2DRenderer::RenderRing(IRenderBackend& backend, const RingCommand& command) {
    constexpr int kSegments = 32;
    const float inner = std::max(0.0f, command.radius - command.thickness);
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(kSegments * 4);
    indices.reserve(kSegments * 6);
    for (int i = 0; i < kSegments; ++i) {
        const float a = static_cast<float>(i) * 6.28318530718f / static_cast<float>(kSegments);
        const float ca = std::cos(a);
        const float sa = std::sin(a);
        const Vec2F outerPoint = TransformPoint(command.transform,
            {command.center.x + ca * command.radius, command.center.y + sa * command.radius});
        const Vec2F innerPoint = TransformPoint(command.transform,
            {command.center.x + ca * inner, command.center.y + sa * inner});
        vertices.push_back(outerPoint.x); vertices.push_back(outerPoint.y);
        vertices.push_back(innerPoint.x); vertices.push_back(innerPoint.y);
    }
    for (int i = 0; i < kSegments; ++i) {
        const unsigned int outer0 = static_cast<unsigned int>(i * 2);
        const unsigned int inner0 = outer0 + 1;
        const unsigned int outer1 = static_cast<unsigned int>(((i + 1) % kSegments) * 2);
        const unsigned int inner1 = outer1 + 1;
        indices.insert(indices.end(), {outer0, outer1, inner1, outer0, inner1, inner0});
    }
    backend.SetBlendMode(command.blendMode, command.alphaMode);
    backend.SetMaterialTint(&command.color.r);
    backend.SetTextureEnabled(false);
    backend.UploadMesh(vertices.data(), static_cast<int>(vertices.size() / 2), 2, nullptr, 0, indices.data(), static_cast<int>(indices.size()));
    backend.DrawMesh();
    ++m_Stats.drawCallCount;
}

void OpenGL2DRenderer::RenderNineSlice(IRenderBackend& backend, const NineSliceCommand& command) {
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
            RenderSprite(backend, sprite);
        }
    }
}

void OpenGL2DRenderer::RenderText(IRenderBackend& backend, const TextCommand& command) {
    const TextLayoutBitmap* bitmap = m_Text != nullptr ? m_Text->GetBitmap(command.layout) : nullptr;
    if (bitmap == nullptr || bitmap->width <= 0 || bitmap->height <= 0 || bitmap->bgraPremultiplied.empty()) {
        return;
    }

    const std::uint64_t handleKey =
        (static_cast<std::uint64_t>(command.layout.generation) << 32u) | command.layout.index;
    const std::uint64_t key = 0x5445585400000000ull ^ handleKey;
    auto [it, inserted] = m_TextTexturePixels.try_emplace(key);
    if (inserted) {
        std::vector<std::uint8_t>& rgba = it->second;
        rgba.resize(bitmap->bgraPremultiplied.size(), 0);
        for (int i = 0; i < bitmap->width * bitmap->height; ++i) {
            const std::size_t base = static_cast<std::size_t>(i) * 4;
            rgba[base + 0] = bitmap->bgraPremultiplied[base + 2];
            rgba[base + 1] = bitmap->bgraPremultiplied[base + 1];
            rgba[base + 2] = bitmap->bgraPremultiplied[base + 0];
            rgba[base + 3] = bitmap->bgraPremultiplied[base + 3];
        }
        ++m_TextTextureUploadCount;
    }
    backend.BindTextureRGBA8(key, it->second.data(), bitmap->width, bitmap->height);
    backend.SetBlendMode(command.blendMode, AlphaMode::Premultiplied);
    backend.SetMaterialTint(&command.color.r);
    UploadQuad(
        backend,
        {command.position.x, command.position.y,
         static_cast<float>(bitmap->width), static_cast<float>(bitmap->height)},
        {0.0f, 0.0f, 1.0f, 1.0f},
        command.transform);
    backend.DrawMesh();
    ++m_Stats.drawCallCount;
    ++m_Stats.quadCount;
}

void OpenGL2DRenderer::RenderCustomMesh(IRenderBackend& backend, const CustomMeshCommand& command) {
    if (command.vertices.empty() || command.indices.empty()) {
        return;
    }
    backend.SetBlendMode(command.blendMode, command.alphaMode);
    backend.SetTextureEnabled(true);
    backend.UploadMesh(
        command.vertices.data(),
        static_cast<int>(command.vertices.size() / static_cast<std::size_t>(command.vertexDimension)),
        command.vertexDimension,
        command.uvs.empty() ? nullptr : command.uvs.data(),
        command.uvs.empty() ? 0 : static_cast<int>(command.uvs.size() / 2),
        command.indices.data(),
        static_cast<int>(command.indices.size()));
    backend.DrawMesh();
    ++m_Stats.drawCallCount;
}

} // namespace Engine
