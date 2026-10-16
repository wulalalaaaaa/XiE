#pragma once

#include "Curve/Curve.h"
#include "Renderer2D/DrawList2D.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace XiE::VoiceMixer {

struct CurveEditorTrack {
    std::string id;
    std::string label;
    Engine::Curve::CurveModel curve;
    Engine::Color4f color{0.25f, 0.72f, 1.0f, 1.0f};
    bool visible = true;
};

struct CurveEditorSelection {
    std::size_t trackIndex = 0;
    Engine::Curve::CurvePointId pointId = 0;

    [[nodiscard]] bool IsValid() const noexcept { return pointId != 0; }
};

class CurveEditorModel {
public:
    float domainMinimum = 70.0f;
    float domainMaximum = 500.0f;
    float rangeMinimum = -12.0f;
    float rangeMaximum = 12.0f;
    std::vector<CurveEditorTrack> tracks;
};

class UICurveEditor {
public:
    explicit UICurveEditor(CurveEditorModel model = {});

    void SetModel(CurveEditorModel model);
    void SetBounds(Engine::RectF bounds) noexcept { m_Bounds = bounds; }
    [[nodiscard]] Engine::RectF Bounds() const noexcept { return m_Bounds; }
    [[nodiscard]] CurveEditorModel& Model() noexcept { return m_Model; }
    [[nodiscard]] const CurveEditorModel& Model() const noexcept { return m_Model; }
    [[nodiscard]] CurveEditorSelection Selection() const noexcept { return m_Selection; }
    void SetActiveTrack(std::size_t track) noexcept;
    [[nodiscard]] std::size_t ActiveTrack() const noexcept { return m_ActiveTrack; }
    void SetChangedCallback(std::function<void(std::size_t)> callback) {
        m_OnChanged = std::move(callback);
    }

    bool PointerDown(Engine::Vec2F point, bool secondary);
    bool PointerMove(Engine::Vec2F point);
    void PointerUp() noexcept { m_Dragging = false; }
    void Draw(Engine::DrawList2D& list) const;

    [[nodiscard]] std::shared_ptr<const Engine::Curve::CurveSnapshot> BuildSnapshot(
        std::size_t trackIndex,
        std::size_t lutSize = 1024) const;

private:
    [[nodiscard]] bool Contains(Engine::Vec2F point) const noexcept;
    [[nodiscard]] Engine::Vec2F ToScreen(float x, float y) const noexcept;
    [[nodiscard]] Engine::Vec2F ToValue(Engine::Vec2F point) const noexcept;
    [[nodiscard]] std::optional<CurveEditorSelection> HitPoint(Engine::Vec2F point) const;
    [[nodiscard]] Engine::Curve::CurvePointId NextPointId() noexcept;
    void Notify(std::size_t track);

    CurveEditorModel m_Model;
    Engine::RectF m_Bounds{};
    CurveEditorSelection m_Selection{};
    bool m_Dragging = false;
    std::size_t m_ActiveTrack = 0;
    Engine::Curve::CurvePointId m_NextId = 1;
    std::function<void(std::size_t)> m_OnChanged;
};

} // namespace XiE::VoiceMixer
