#include "Ink/CrystalInkController.h"

#include <chrono>
#include <utility>

namespace Crystal {
namespace {

std::string NewStrokeId() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "stroke-client-" + std::to_string(now);
}

} // namespace

void CrystalInkController::Load(std::vector<Engine::UI2D::InkStroke> strokes) {
    m_History.Clear();
    for (auto& stroke : strokes) m_History.Add(std::move(stroke));
    m_Drawing = false;
    m_Active = {};
}

void CrystalInkController::BeginStroke(Engine::Vec2F worldPoint, float width, std::string layer) {
    m_Active = {};
    m_Active.id = NewStrokeId();
    m_Active.width = width > 0.0f ? width : m_Width;
    m_Active.layer = std::move(layer);
    m_Active.tool = m_Tool;
    m_Active.shape = m_Shape;
    m_Active.color = m_Color;
    m_Active.points.push_back(worldPoint);
    m_Drawing = true;
}

void CrystalInkController::AppendPoint(Engine::Vec2F worldPoint) {
    if (m_Drawing) m_Active.points.push_back(worldPoint);
}

bool CrystalInkController::EndStroke(Engine::UI2D::InkStroke* completed) {
    if (!m_Drawing) return false;
    m_Drawing = false;
    if (m_Active.points.size() < 2) {
        m_Active = {};
        return false;
    }
    if (completed != nullptr) *completed = m_Active;
    m_History.Add(std::move(m_Active));
    m_Active = {};
    return true;
}

bool CrystalInkController::EraseStroke(const std::string& id) { return m_History.Remove(id); }

bool CrystalInkController::AttachToEntity(
    const std::string& strokeId, const std::string& entityId, Engine::Vec2F entityPosition) {
    for (auto& stroke : m_History.Strokes()) {
        if (stroke.id != strokeId) continue;
        stroke.anchorEntityId = entityId;
        stroke.localOffset = entityPosition;
        return true;
    }
    return false;
}

} // namespace Crystal
