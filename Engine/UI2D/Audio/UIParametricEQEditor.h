#pragma once

#include "Audio/Analysis/SpectrumAnalyzer.h"
#include "Audio/DSP/EQ/ParametricEQ.h"
#include "Input/InputEvents.h"
#include "Renderer2D/DrawList2D.h"

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace Engine::UI {

struct UIEQBandControlLayout {
    RectF enabled{};
    RectF type{};
    RectF frequency{};
    RectF gain{};
    RectF q{};
    RectF slope{};
    RectF remove{};
};

class UIParametricEQEditor {
public:
    explicit UIParametricEQEditor(Audio::ParametricEQModel model = {});

    void SetBounds(RectF bounds);
    [[nodiscard]] RectF Bounds() const noexcept { return m_Bounds; }
    [[nodiscard]] RectF PlotBounds() const noexcept { return m_PlotBounds; }
    [[nodiscard]] const UIEQBandControlLayout& Controls() const noexcept { return m_Controls; }
    void SetSampleRate(double sampleRate);
    void SetModel(Audio::ParametricEQModel model);
    [[nodiscard]] Audio::ParametricEQModel& Model() noexcept { return m_Model; }
    [[nodiscard]] const Audio::ParametricEQModel& Model() const noexcept { return m_Model; }
    [[nodiscard]] Audio::EQBandId SelectedBandId() const noexcept { return m_SelectedBand; }
    [[nodiscard]] const Audio::ParametricEQBand* SelectedBand() const noexcept;
    [[nodiscard]] std::optional<Audio::EQBandId> HitBandAt(Vec2F point) const noexcept;
    void SetChangedCallback(std::function<void()> callback) { m_OnChanged = std::move(callback); }
    void SetSpectrumSnapshot(std::shared_ptr<const Audio::SpectrumSnapshot> snapshot);

    bool PointerDown(Vec2F point, bool secondary, double timestampSeconds);
    bool PointerMove(Vec2F point);
    void PointerUp() noexcept { m_Dragging = false; }
    bool PointerWheel(Vec2F point, float delta);
    bool KeyDown(InputKeyCode key);
    bool CreateBellBandAt(Vec2F point);
    bool RemoveBand(Audio::EQBandId id);
    void ClearSelection() noexcept { m_SelectedBand = 0; }
    void Draw(DrawList2D& list) const;
    // Draw only the response/spectrum overlay. The generic multi-track editor
    // owns the background, grid and node interaction layer.
    void DrawUnifiedOverlay(DrawList2D& list, bool active) const;
    void DrawControls(DrawList2D& list) const;

    [[nodiscard]] float FrequencyToX(float frequencyHz) const noexcept;
    [[nodiscard]] float XToFrequency(float x) const noexcept;
    [[nodiscard]] float GainToY(float gainDb) const noexcept;
    [[nodiscard]] float YToGain(float y) const noexcept;
    void Reset();

private:
    [[nodiscard]] bool Contains(RectF rect, Vec2F point) const noexcept;
    [[nodiscard]] std::optional<Audio::EQBandId> HitBand(Vec2F point) const noexcept;
    [[nodiscard]] Color4f BandColor(std::size_t index, bool enabled) const noexcept;
    void RecalculateLayout();
    void RebuildResponse();
    void RebuildResponseGeometry();
    void RebuildSpectrumGeometry();
    void NotifyChanged();
    void CycleSelectedType();
    void RemoveSelected();

    Audio::ParametricEQModel m_Model;
    RectF m_Bounds{};
    RectF m_PlotBounds{};
    UIEQBandControlLayout m_Controls{};
    double m_SampleRate = 48'000.0;
    Audio::EQBandId m_SelectedBand = 0;
    bool m_Dragging = false;
    Vec2F m_LastClickPosition{};
    double m_LastClickTime = -10.0;
    std::uint64_t m_SpectrumRevision = 0;
    std::shared_ptr<const Audio::SpectrumSnapshot> m_Spectrum;
    Audio::EQResponseData m_Response;
    std::vector<Vec2F> m_TotalResponsePoints;
    std::vector<std::vector<Vec2F>> m_BandResponsePoints;
    std::vector<Vec2F> m_InputSpectrumPoints;
    std::vector<Vec2F> m_OutputSpectrumPoints;
    std::function<void()> m_OnChanged;
};

} // namespace Engine::UI
