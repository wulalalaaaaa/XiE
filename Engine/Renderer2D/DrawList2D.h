#pragma once

#include "DrawCommands2D.h"

#include <span>
#include <vector>

namespace Engine {

class DrawList2D {
public:
    void Clear();
    void Reserve(std::size_t commandCount);

    void AddSprite(const SpriteCommand& command);
    void AddSolidRect(const SolidRectCommand& command);
    void AddRoundedRect(const RoundedRectCommand& command);
    void AddLine(const LineCommand& command);
    void AddCircle(const CircleCommand& command);
    void AddRing(const RingCommand& command);
    void AddNineSlice(const NineSliceCommand& command);
    void AddText(const TextCommand& command);
    void AddCustomMesh(const CustomMeshCommand& command);

    void PushClipRect(const RectF& rect);
    void PopClip();

    [[nodiscard]] bool Empty() const;
    [[nodiscard]] std::span<const DrawCommand2D> Commands() const;

private:
    std::vector<DrawCommand2D> m_Commands;
};

} // namespace Engine
