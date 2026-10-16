#include "UI/UICurveEditor.h"

#include <algorithm>
#include <cmath>

namespace XiE::VoiceMixer {
namespace {

float DistanceSquared(Engine::Vec2F a, Engine::Vec2F b) noexcept {
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    return x * x + y * y;
}

} // namespace

UICurveEditor::UICurveEditor(CurveEditorModel model) {
    SetModel(std::move(model));
}

void UICurveEditor::SetModel(CurveEditorModel model) {
    m_Model = std::move(model);
    m_Selection = {};
    m_Dragging = false;
    m_ActiveTrack = std::min(m_ActiveTrack,
        m_Model.tracks.empty() ? std::size_t{0} : m_Model.tracks.size() - 1);
    m_NextId = 1;
    for (const CurveEditorTrack& track : m_Model.tracks) {
        for (const Engine::Curve::CurvePoint& point : track.curve.Points()) {
            m_NextId = std::max(m_NextId, point.id + 1);
        }
    }
}

void UICurveEditor::SetActiveTrack(std::size_t track) noexcept {
    if (track < m_Model.tracks.size()) {
        m_ActiveTrack = track;
    }
}

bool UICurveEditor::PointerDown(Engine::Vec2F point, bool secondary) {
    if (!Contains(point) || m_Model.tracks.empty()) {
        return false;
    }
    const auto hit = HitPoint(point);
    if (hit) {
        m_Selection = *hit;
        m_ActiveTrack = hit->trackIndex;
        if (secondary) {
            auto& curve = m_Model.tracks[m_Selection.trackIndex].curve;
            if (curve.Points().size() > 1 &&
                curve.RemovePoint(m_Selection.pointId) == Engine::Curve::CurveStatus::Ok) {
                const std::size_t changedTrack = m_Selection.trackIndex;
                m_Selection = {};
                Notify(changedTrack);
            }
        } else {
            m_Dragging = true;
        }
        return true;
    }
    if (secondary) {
        return true;
    }
    const std::size_t trackIndex = std::min(m_ActiveTrack, m_Model.tracks.size() - 1);
    const Engine::Vec2F value = ToValue(point);
    Engine::Curve::CurvePoint curvePoint{
        NextPointId(), value.x, value.y, Engine::Curve::CurveInterpolation::Smooth};
    auto& curve = m_Model.tracks[trackIndex].curve;
    Engine::Curve::CurveStatus addStatus = curve.AddPoint(curvePoint);
    if (addStatus == Engine::Curve::CurveStatus::DuplicateX) {
        curvePoint.x = std::nextafter(curvePoint.x, m_Model.domainMaximum);
        addStatus = curve.AddPoint(curvePoint);
    }
    if (addStatus == Engine::Curve::CurveStatus::Ok) {
        m_Selection = {trackIndex, curvePoint.id};
        m_Dragging = true;
        Notify(trackIndex);
    }
    return true;
}

bool UICurveEditor::PointerMove(Engine::Vec2F point) {
    if (!m_Dragging || !m_Selection.IsValid() ||
        m_Selection.trackIndex >= m_Model.tracks.size()) {
        return false;
    }
    const Engine::Vec2F value = ToValue(point);
    auto& curve = m_Model.tracks[m_Selection.trackIndex].curve;
    if (curve.MovePoint(m_Selection.pointId, value.x, value.y) ==
        Engine::Curve::CurveStatus::Ok) {
        Notify(m_Selection.trackIndex);
    }
    return true;
}

void UICurveEditor::Draw(Engine::DrawList2D& list) const {
    list.AddRoundedRect({m_Bounds, 8.0f, {0.006f, 0.032f, 0.050f, 1.0f}});
    list.PushClipRect(m_Bounds);
    for (int line = 1; line < 4; ++line) {
        const float x = m_Bounds.x + m_Bounds.width * line / 4.0f;
        const float y = m_Bounds.y + m_Bounds.height * line / 4.0f;
        list.AddLine({{x, m_Bounds.y}, {x, m_Bounds.y + m_Bounds.height}, 1.0f,
            {0.10f, 0.30f, 0.39f, 0.48f}});
        list.AddLine({{m_Bounds.x, y}, {m_Bounds.x + m_Bounds.width, y}, 1.0f,
            {0.10f, 0.30f, 0.39f, 0.48f}});
    }
    if (m_Model.rangeMinimum <= 0.0f && m_Model.rangeMaximum >= 0.0f) {
        const float zeroY = ToScreen(m_Model.domainMinimum, 0.0f).y;
        list.AddLine({{m_Bounds.x, zeroY}, {m_Bounds.x + m_Bounds.width, zeroY},
            1.5f, {0.22f, 0.57f, 0.68f, 0.74f}});
    }
    constexpr int segments = 512;
    const auto drawTrack = [&](std::size_t trackIndex, float thickness) {
        const CurveEditorTrack& track = m_Model.tracks[trackIndex];
        if (!track.visible || track.curve.Points().empty()) {
            return;
        }
        Engine::Vec2F previous = ToScreen(
            m_Model.domainMinimum,
            Engine::Curve::CurveSampler::Sample(track.curve.Points(), m_Model.domainMinimum));
        for (int segment = 1; segment <= segments; ++segment) {
            const float t = static_cast<float>(segment) / segments;
            const float x = std::lerp(m_Model.domainMinimum, m_Model.domainMaximum, t);
            const float y = Engine::Curve::CurveSampler::Sample(track.curve.Points(), x);
            const Engine::Vec2F current = ToScreen(x, y);
            list.AddLine({previous, current, thickness, track.color});
            previous = current;
        }
    };
    for (std::size_t trackIndex = 0; trackIndex < m_Model.tracks.size(); ++trackIndex) {
        if (trackIndex != m_ActiveTrack) drawTrack(trackIndex, 5.0f);
    }
    if (m_ActiveTrack < m_Model.tracks.size()) drawTrack(m_ActiveTrack, 2.0f);

    for (std::size_t trackIndex = 0; trackIndex < m_Model.tracks.size(); ++trackIndex) {
        const CurveEditorTrack& track = m_Model.tracks[trackIndex];
        if (!track.visible) continue;
        for (const Engine::Curve::CurvePoint& point : track.curve.Points()) {
            const Engine::Vec2F center = ToScreen(point.x, point.y);
            const bool selected = m_Selection.trackIndex == trackIndex &&
                m_Selection.pointId == point.id;
            const bool active = trackIndex == m_ActiveTrack;
            list.AddCircle({center, selected ? 6.0f : active ? 4.5f : 3.5f,
                selected ? Engine::Color4f{1.0f, 0.83f, 0.32f, 1.0f} : track.color});
            list.AddRing({center, selected ? 7.5f : active ? 6.0f : 5.0f, 1.0f,
                {0.95f, 0.97f, 1.0f, 0.9f}});
        }
    }
    list.PopClip();
}

std::shared_ptr<const Engine::Curve::CurveSnapshot> UICurveEditor::BuildSnapshot(
    std::size_t trackIndex,
    std::size_t lutSize) const {
    if (trackIndex >= m_Model.tracks.size()) {
        return {};
    }
    return Engine::Curve::BuildCurveSnapshot(
        m_Model.tracks[trackIndex].curve,
        lutSize,
        m_Model.domainMinimum,
        m_Model.domainMaximum);
}

bool UICurveEditor::Contains(Engine::Vec2F point) const noexcept {
    return point.x >= m_Bounds.x && point.y >= m_Bounds.y &&
        point.x <= m_Bounds.x + m_Bounds.width &&
        point.y <= m_Bounds.y + m_Bounds.height;
}

Engine::Vec2F UICurveEditor::ToScreen(float x, float y) const noexcept {
    const float tx = (x - m_Model.domainMinimum) /
        (m_Model.domainMaximum - m_Model.domainMinimum);
    const float ty = (y - m_Model.rangeMinimum) /
        (m_Model.rangeMaximum - m_Model.rangeMinimum);
    return {
        m_Bounds.x + std::clamp(tx, 0.0f, 1.0f) * m_Bounds.width,
        m_Bounds.y + (1.0f - std::clamp(ty, 0.0f, 1.0f)) * m_Bounds.height};
}

Engine::Vec2F UICurveEditor::ToValue(Engine::Vec2F point) const noexcept {
    const float tx = std::clamp((point.x - m_Bounds.x) / m_Bounds.width, 0.0f, 1.0f);
    const float ty = std::clamp((point.y - m_Bounds.y) / m_Bounds.height, 0.0f, 1.0f);
    return {
        std::lerp(m_Model.domainMinimum, m_Model.domainMaximum, tx),
        std::lerp(m_Model.rangeMaximum, m_Model.rangeMinimum, ty)};
}

std::optional<CurveEditorSelection> UICurveEditor::HitPoint(Engine::Vec2F point) const {
    std::optional<CurveEditorSelection> result;
    float closest = 11.0f * 11.0f;
    for (std::size_t trackIndex = 0; trackIndex < m_Model.tracks.size(); ++trackIndex) {
        const CurveEditorTrack& track = m_Model.tracks[trackIndex];
        if (!track.visible) continue;
        for (const Engine::Curve::CurvePoint& curvePoint : track.curve.Points()) {
            const float distance = DistanceSquared(point, ToScreen(curvePoint.x, curvePoint.y));
            if (distance <= closest) {
                closest = distance;
                result = CurveEditorSelection{trackIndex, curvePoint.id};
            }
        }
    }
    return result;
}

Engine::Curve::CurvePointId UICurveEditor::NextPointId() noexcept {
    return m_NextId++;
}

void UICurveEditor::Notify(std::size_t track) {
    if (m_OnChanged) {
        m_OnChanged(track);
    }
}

} // namespace XiE::VoiceMixer
