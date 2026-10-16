#include "UI2D/InkStroke.h"

#include <algorithm>

namespace Engine::UI2D {

void StrokeHistory::Add(InkStroke stroke) {
    m_Strokes.push_back(std::move(stroke));
    m_Undo.clear();
    m_Redo.clear();
}

bool StrokeHistory::Remove(const std::string& id) {
    const auto found = std::find_if(m_Strokes.begin(), m_Strokes.end(),
        [&](const InkStroke& stroke) { return stroke.id == id; });
    if (found == m_Strokes.end()) return false;
    m_Undo.push_back(std::move(*found));
    m_Strokes.erase(found);
    m_Redo.clear();
    return true;
}

bool StrokeHistory::Undo() {
    if (m_Strokes.empty()) return false;
    m_Undo.push_back(std::move(m_Strokes.back()));
    m_Strokes.pop_back();
    m_Redo.clear();
    return true;
}

bool StrokeHistory::Redo() {
    if (m_Undo.empty()) return false;
    m_Strokes.push_back(std::move(m_Undo.back()));
    m_Undo.pop_back();
    return true;
}

void StrokeHistory::Clear() noexcept {
    m_Strokes.clear();
    m_Undo.clear();
    m_Redo.clear();
}

} // namespace Engine::UI2D
