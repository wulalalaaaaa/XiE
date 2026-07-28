#pragma once

#include "Renderer2D/ITextLayoutService.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Engine {

class BasicTextLayoutService final : public ITextLayoutService {
public:
    TextLayoutHandle CreateLayout(
        FontHandle font,
        std::u32string_view text,
        float maxWidth,
        float fontSize) override;
    const TextLayoutBitmap* GetBitmap(TextLayoutHandle handle) const override;
    std::uint32_t RasterizeCount() const override;
    std::uint64_t Revision() const override { return m_Revision; }

private:
    struct Slot {
        TextLayoutBitmap bitmap;
        std::uint32_t generation = 1;
        bool occupied = false;
    };

    static std::wstring ToWide(std::u32string_view text);
    TextLayoutBitmap Rasterize(std::u32string_view text, float maxWidth, float fontSize);

private:
    std::vector<Slot> m_Slots;
    std::unordered_map<std::string, TextLayoutHandle> m_Cache;
    std::uint32_t m_RasterizeCount = 0;
    std::uint64_t m_Revision = 0;
};

} // namespace Engine
