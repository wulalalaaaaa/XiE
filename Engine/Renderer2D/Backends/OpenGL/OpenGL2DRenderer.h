#pragma once

#include "Renderer2D/DrawList2D.h"
#include "Renderer2D/TextLayoutService.h"
#include "Renderer2D/TextureRegistry2D.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Engine {

class IRenderBackend;
class IRenderSurface;

class OpenGL2DRenderer {
public:
    void SetTextLayoutService(const ITextLayoutService* text);
    void SetTextureRegistry(const TextureRegistry2D* textures);
    void Render(IRenderSurface& surface, IRenderBackend& backend, const DrawList2D& drawList, const Render2DView& view);

    [[nodiscard]] const Render2DStats& GetStats() const;
    [[nodiscard]] std::uint32_t TextTextureUploadCount() const;

private:
    void RenderSprite(IRenderBackend& backend, const SpriteCommand& command);
    void RenderSolidRect(IRenderBackend& backend, const SolidRectCommand& command);
    void RenderRoundedRect(IRenderBackend& backend, const RoundedRectCommand& command);
    void RenderLine(IRenderBackend& backend, const LineCommand& command);
    void RenderCircle(IRenderBackend& backend, const CircleCommand& command);
    void RenderRing(IRenderBackend& backend, const RingCommand& command);
    void RenderNineSlice(IRenderBackend& backend, const NineSliceCommand& command);
    void RenderText(IRenderBackend& backend, const TextCommand& command);
    void RenderCustomMesh(IRenderBackend& backend, const CustomMeshCommand& command);

private:
    Render2DStats m_Stats{};
    const ITextLayoutService* m_Text = nullptr;
    const TextureRegistry2D* m_Textures = nullptr;
    std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> m_TextTexturePixels;
    std::uint32_t m_TextTextureUploadCount = 0;
};

} // namespace Engine
