#include "DrawList2D.h"

namespace Engine {

void DrawList2D::Clear() {
    m_Commands.clear();
}

void DrawList2D::Reserve(std::size_t commandCount) {
    m_Commands.reserve(commandCount);
}

void DrawList2D::AddSprite(const SpriteCommand& command) {
    m_Commands.emplace_back(command);
}

void DrawList2D::AddSolidRect(const SolidRectCommand& command) {
    m_Commands.emplace_back(command);
}

void DrawList2D::AddRoundedRect(const RoundedRectCommand& command) {
    m_Commands.emplace_back(command);
}

void DrawList2D::AddLine(const LineCommand& command) {
    m_Commands.emplace_back(command);
}

void DrawList2D::AddCircle(const CircleCommand& command) {
    m_Commands.emplace_back(command);
}

void DrawList2D::AddRing(const RingCommand& command) {
    m_Commands.emplace_back(command);
}

void DrawList2D::AddNineSlice(const NineSliceCommand& command) {
    m_Commands.emplace_back(command);
}

void DrawList2D::AddText(const TextCommand& command) {
    m_Commands.emplace_back(command);
}

void DrawList2D::AddCustomMesh(const CustomMeshCommand& command) {
    m_Commands.emplace_back(command);
}

void DrawList2D::PushClipRect(const RectF& rect) {
    m_Commands.emplace_back(PushClipRectCommand{rect});
}

void DrawList2D::PopClip() {
    m_Commands.emplace_back(PopClipCommand{});
}

bool DrawList2D::Empty() const {
    return m_Commands.empty();
}

std::span<const DrawCommand2D> DrawList2D::Commands() const {
    return m_Commands;
}

} // namespace Engine
