#include "UI2D/Audio/UIParametricEQEditor.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Engine::UI {
namespace {

constexpr float kFooterHeight = 58.0f;
constexpr std::array<float, 10> kFrequencyGrid{
    20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
    1'000.0f, 2'000.0f, 5'000.0f, 10'000.0f, 20'000.0f};
constexpr std::array<Color4f, 8> kBandColors{{
    {0.20f, 0.78f, 1.00f, 1.0f}, {0.96f, 0.48f, 0.72f, 1.0f},
    {0.36f, 0.91f, 0.61f, 1.0f}, {1.00f, 0.72f, 0.27f, 1.0f},
    {0.62f, 0.52f, 1.00f, 1.0f}, {0.25f, 0.87f, 0.88f, 1.0f},
    {1.00f, 0.48f, 0.36f, 1.0f}, {0.66f, 0.86f, 0.34f, 1.0f}}};

float DistanceSquared(Vec2F a, Vec2F b) noexcept {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace

UIParametricEQEditor::UIParametricEQEditor(Audio::ParametricEQModel model)
    : m_Model(std::move(model)) {
    RebuildResponse();
}

void UIParametricEQEditor::SetBounds(RectF bounds) {
    if (bounds.x == m_Bounds.x && bounds.y == m_Bounds.y &&
        bounds.width == m_Bounds.width && bounds.height == m_Bounds.height) return;
    m_Bounds = bounds;
    RecalculateLayout();
    RebuildResponseGeometry();
    RebuildSpectrumGeometry();
}

void UIParametricEQEditor::SetSampleRate(double sampleRate) {
    if (sampleRate <= 0.0 || std::abs(sampleRate - m_SampleRate) < 0.5) return;
    m_SampleRate = sampleRate;
    RebuildResponse();
}

void UIParametricEQEditor::SetModel(Audio::ParametricEQModel model) {
    m_Model = std::move(model);
    if (m_Model.FindBand(m_SelectedBand) == nullptr) m_SelectedBand = 0;
    RebuildResponse();
}

const Audio::ParametricEQBand* UIParametricEQEditor::SelectedBand() const noexcept {
    return m_Model.FindBand(m_SelectedBand);
}

std::optional<Audio::EQBandId> UIParametricEQEditor::HitBandAt(Vec2F point) const noexcept {
    return HitBand(point);
}

void UIParametricEQEditor::SetSpectrumSnapshot(
    std::shared_ptr<const Audio::SpectrumSnapshot> snapshot) {
    if (!snapshot || snapshot->revision == m_SpectrumRevision) return;
    m_SpectrumRevision = snapshot->revision;
    m_Spectrum = std::move(snapshot);
    RebuildSpectrumGeometry();
}

bool UIParametricEQEditor::PointerDown(
    Vec2F point, bool secondary, double timestampSeconds) {
    if (!Contains(m_Bounds, point)) return false;
    if (m_SelectedBand != 0) {
        if (Contains(m_Controls.enabled, point)) {
            if (auto* band = m_Model.FindBand(m_SelectedBand)) {
                band->enabled = !band->enabled;
                NotifyChanged();
            }
            return true;
        }
        if (Contains(m_Controls.type, point)) {
            CycleSelectedType();
            return true;
        }
        if (Contains(m_Controls.remove, point)) {
            RemoveSelected();
            return true;
        }
    }
    if (!Contains(m_PlotBounds, point)) return true;
    const auto hit = HitBand(point);
    const bool doubleClick = timestampSeconds - m_LastClickTime <= 0.35 &&
        DistanceSquared(point, m_LastClickPosition) <= 12.0f * 12.0f;
    m_LastClickTime = timestampSeconds;
    m_LastClickPosition = point;
    if (hit) {
        m_SelectedBand = *hit;
        if (secondary) {
            CycleSelectedType();
        } else if (doubleClick) {
            if (auto* band = m_Model.FindBand(m_SelectedBand)) {
                band->enabled = !band->enabled;
                NotifyChanged();
            }
        } else {
            m_Dragging = true;
        }
        return true;
    }
    if (!secondary && doubleClick) {
        Audio::ParametricEQBand band;
        band.frequencyHz = XToFrequency(point.x);
        band.gainDb = YToGain(point.y);
        if (const auto id = m_Model.AddBand(band)) {
            m_SelectedBand = *id;
            NotifyChanged();
        }
    } else if (!secondary) {
        m_SelectedBand = 0;
    }
    return true;
}

bool UIParametricEQEditor::PointerMove(Vec2F point) {
    if (!m_Dragging || m_SelectedBand == 0) return false;
    Audio::ParametricEQBand* band = m_Model.FindBand(m_SelectedBand);
    if (band == nullptr) return false;
    band->frequencyHz = XToFrequency(point.x);
    if (band->type == Audio::EQBandType::Bell ||
        band->type == Audio::EQBandType::LowShelf ||
        band->type == Audio::EQBandType::HighShelf) {
        band->gainDb = YToGain(point.y);
    } else {
        band->gainDb = 0.0f;
    }
    *band = Audio::ClampParametricEQBand(*band, static_cast<float>(m_SampleRate));
    NotifyChanged();
    return true;
}

bool UIParametricEQEditor::PointerWheel(Vec2F point, float delta) {
    if (!Contains(m_Bounds, point) || m_SelectedBand == 0) return false;
    Audio::ParametricEQBand* band = m_Model.FindBand(m_SelectedBand);
    if (band == nullptr) return false;
    const bool hasGain = band->type == Audio::EQBandType::Bell ||
        band->type == Audio::EQBandType::LowShelf ||
        band->type == Audio::EQBandType::HighShelf;
    if (Contains(m_Controls.frequency, point)) {
        band->frequencyHz *= std::pow(1.02f, delta);
    } else if (Contains(m_Controls.gain, point) && hasGain) {
        band->gainDb += delta * 0.25f;
    } else if (Contains(m_Controls.slope, point) &&
        (band->type == Audio::EQBandType::LowCut ||
         band->type == Audio::EQBandType::HighCut)) {
        band->slopeDbPerOctave += delta * 6.0f;
    } else {
        band->q *= std::pow(1.08f, delta);
    }
    *band = Audio::ClampParametricEQBand(*band, static_cast<float>(m_SampleRate));
    NotifyChanged();
    return true;
}

bool UIParametricEQEditor::KeyDown(InputKeyCode key) {
    if (key == InputKeyCode::Escape) {
        ClearSelection();
        return true;
    }
    if (key != InputKeyCode::Delete || m_SelectedBand == 0) return false;
    RemoveSelected();
    return true;
}

bool UIParametricEQEditor::CreateBellBandAt(Vec2F point) {
    if (!Contains(m_PlotBounds, point)) return false;
    Audio::ParametricEQBand band;
    band.frequencyHz = XToFrequency(point.x);
    band.gainDb = YToGain(point.y);
    if (const auto id = m_Model.AddBand(band)) {
        m_SelectedBand = *id;
        NotifyChanged();
        return true;
    }
    return false;
}

bool UIParametricEQEditor::RemoveBand(Audio::EQBandId id) {
    if (!m_Model.RemoveBand(id)) return false;
    if (m_SelectedBand == id) m_SelectedBand = 0;
    NotifyChanged();
    return true;
}

float UIParametricEQEditor::FrequencyToX(float frequencyHz) const noexcept {
    const float frequency = std::clamp(frequencyHz,
        Audio::kParametricEQMinimumFrequencyHz, Audio::kParametricEQMaximumFrequencyHz);
    const float t = std::log(frequency / Audio::kParametricEQMinimumFrequencyHz) /
        std::log(Audio::kParametricEQMaximumFrequencyHz /
            Audio::kParametricEQMinimumFrequencyHz);
    return m_PlotBounds.x + t * m_PlotBounds.width;
}

float UIParametricEQEditor::XToFrequency(float x) const noexcept {
    const float t = std::clamp((x - m_PlotBounds.x) /
        std::max(1.0f, m_PlotBounds.width), 0.0f, 1.0f);
    return Audio::kParametricEQMinimumFrequencyHz * std::pow(
        Audio::kParametricEQMaximumFrequencyHz /
            Audio::kParametricEQMinimumFrequencyHz, t);
}

float UIParametricEQEditor::GainToY(float gainDb) const noexcept {
    const float t = (std::clamp(gainDb, Audio::kParametricEQMinimumGainDb,
        Audio::kParametricEQMaximumGainDb) - Audio::kParametricEQMinimumGainDb) /
        (Audio::kParametricEQMaximumGainDb - Audio::kParametricEQMinimumGainDb);
    return m_PlotBounds.y + (1.0f - t) * m_PlotBounds.height;
}

float UIParametricEQEditor::YToGain(float y) const noexcept {
    const float t = std::clamp((y - m_PlotBounds.y) /
        std::max(1.0f, m_PlotBounds.height), 0.0f, 1.0f);
    return std::lerp(Audio::kParametricEQMaximumGainDb,
        Audio::kParametricEQMinimumGainDb, t);
}

void UIParametricEQEditor::Reset() {
    m_Model.Reset();
    m_SelectedBand = 0;
    NotifyChanged();
}

void UIParametricEQEditor::Draw(DrawList2D& list) const {
    list.AddRoundedRect({m_Bounds, 8.0f, {0.005f, 0.029f, 0.046f, 1.0f}});
    list.PushClipRect(m_PlotBounds);
    for (float frequency : kFrequencyGrid) {
        const float x = FrequencyToX(frequency);
        list.AddLine({{x, m_PlotBounds.y}, {x, m_PlotBounds.y + m_PlotBounds.height},
            frequency == 1'000.0f ? 1.2f : 1.0f, {0.08f, 0.29f, 0.38f, 0.52f}});
    }
    for (int db = -24; db <= 24; db += 6) {
        const float y = GainToY(static_cast<float>(db));
        list.AddLine({{m_PlotBounds.x, y}, {m_PlotBounds.x + m_PlotBounds.width, y},
            db == 0 ? 1.6f : 1.0f,
            db == 0 ? Color4f{0.24f, 0.59f, 0.70f, 0.78f}
                    : Color4f{0.08f, 0.29f, 0.38f, 0.45f}});
    }
    const auto drawPolyline = [&](const std::vector<Vec2F>& points, Color4f color, float width) {
        for (std::size_t index = 1; index < points.size(); ++index) {
            list.AddLine({points[index - 1], points[index], width, color});
        }
    };
    drawPolyline(m_InputSpectrumPoints, {0.18f, 0.56f, 0.73f, 0.42f}, 1.2f);
    drawPolyline(m_OutputSpectrumPoints, {0.30f, 0.91f, 0.67f, 0.54f}, 1.5f);
    for (std::size_t band = 0; band < m_BandResponsePoints.size(); ++band) {
        const bool enabled = band < m_Model.Bands().size() && m_Model.Bands()[band].enabled;
        drawPolyline(m_BandResponsePoints[band], BandColor(band, enabled), 1.2f);
    }
    drawPolyline(m_TotalResponsePoints, {0.92f, 0.96f, 1.0f, 1.0f}, 2.4f);
    for (std::size_t index = 0; index < m_Model.Bands().size(); ++index) {
        const Audio::ParametricEQBand& band = m_Model.Bands()[index];
        const bool hasGain = band.type == Audio::EQBandType::Bell ||
            band.type == Audio::EQBandType::LowShelf ||
            band.type == Audio::EQBandType::HighShelf;
        const Vec2F center{FrequencyToX(band.frequencyHz), GainToY(hasGain ? band.gainDb : 0.0f)};
        const bool selected = band.id == m_SelectedBand;
        list.AddCircle({center, selected ? 8.0f : 6.0f, BandColor(index, band.enabled)});
        list.AddRing({center, selected ? 10.0f : 8.0f, selected ? 2.0f : 1.0f,
            selected ? Color4f{1.0f, 0.88f, 0.43f, 1.0f}
                     : Color4f{0.90f, 0.95f, 1.0f, 0.82f}});
    }
    list.PopClip();

    DrawControls(list);
}

void UIParametricEQEditor::DrawUnifiedOverlay(DrawList2D& list, bool active) const {
    list.PushClipRect(m_PlotBounds);
    const auto drawPolyline = [&](const std::vector<Vec2F>& points, Color4f color, float width) {
        if (!active) color.a *= 0.42f;
        for (std::size_t index = 1; index < points.size(); ++index) {
            list.AddLine({points[index - 1], points[index], width, color});
        }
    };
    drawPolyline(m_InputSpectrumPoints, {0.18f, 0.56f, 0.73f, 0.42f}, 1.2f);
    drawPolyline(m_OutputSpectrumPoints, {0.30f, 0.91f, 0.67f, 0.54f}, 1.5f);
    for (std::size_t band = 0; band < m_BandResponsePoints.size(); ++band) {
        const bool enabled = band < m_Model.Bands().size() && m_Model.Bands()[band].enabled;
        drawPolyline(m_BandResponsePoints[band], BandColor(band, enabled), 1.2f);
    }
    drawPolyline(m_TotalResponsePoints, {0.92f, 0.96f, 1.0f, 1.0f}, 2.4f);
    list.PopClip();
}

void UIParametricEQEditor::DrawControls(DrawList2D& list) const {
    if (m_SelectedBand == 0) return;
    for (const RectF rect : {m_Controls.enabled, m_Controls.type, m_Controls.frequency,
            m_Controls.gain, m_Controls.q, m_Controls.slope, m_Controls.remove}) {
        list.AddRoundedRect({rect, 5.0f, {0.035f, 0.095f, 0.125f, 1.0f}});
    }
}

bool UIParametricEQEditor::Contains(RectF rect, Vec2F point) const noexcept {
    return point.x >= rect.x && point.y >= rect.y &&
        point.x <= rect.x + rect.width && point.y <= rect.y + rect.height;
}

std::optional<Audio::EQBandId> UIParametricEQEditor::HitBand(Vec2F point) const noexcept {
    std::optional<Audio::EQBandId> result;
    float closest = 13.0f * 13.0f;
    for (const Audio::ParametricEQBand& band : m_Model.Bands()) {
        const bool hasGain = band.type == Audio::EQBandType::Bell ||
            band.type == Audio::EQBandType::LowShelf ||
            band.type == Audio::EQBandType::HighShelf;
        const Vec2F center{FrequencyToX(band.frequencyHz), GainToY(hasGain ? band.gainDb : 0.0f)};
        const float distance = DistanceSquared(point, center);
        if (distance <= closest) {
            closest = distance;
            result = band.id;
        }
    }
    return result;
}

Color4f UIParametricEQEditor::BandColor(std::size_t index, bool enabled) const noexcept {
    Color4f color = kBandColors[index % kBandColors.size()];
    if (!enabled) color.a = 0.28f;
    else color.a = 0.72f;
    return color;
}

void UIParametricEQEditor::RecalculateLayout() {
    m_PlotBounds = {m_Bounds.x, m_Bounds.y, m_Bounds.width,
        std::max(1.0f, m_Bounds.height - kFooterHeight)};
    const float y = m_PlotBounds.y + m_PlotBounds.height + 8.0f;
    const float gap = 7.0f;
    const float widths[] = {72.0f, 100.0f, 112.0f, 96.0f, 82.0f, 86.0f, 72.0f};
    RectF* controls[] = {&m_Controls.enabled, &m_Controls.type, &m_Controls.frequency,
        &m_Controls.gain, &m_Controls.q, &m_Controls.slope, &m_Controls.remove};
    float x = m_Bounds.x + 8.0f;
    for (std::size_t index = 0; index < std::size(widths); ++index) {
        *controls[index] = {x, y, widths[index], 38.0f};
        x += widths[index] + gap;
    }
}

void UIParametricEQEditor::RebuildResponse() {
    m_Response = Audio::EQResponseCalculator::Calculate(m_Model, m_SampleRate, 1024);
    RebuildResponseGeometry();
}

void UIParametricEQEditor::RebuildResponseGeometry() {
    m_TotalResponsePoints.clear();
    m_BandResponsePoints.assign(m_Response.bandResponseDb.size(), {});
    m_TotalResponsePoints.reserve(m_Response.frequenciesHz.size());
    for (auto& points : m_BandResponsePoints) points.reserve(m_Response.frequenciesHz.size());
    for (std::size_t point = 0; point < m_Response.frequenciesHz.size(); ++point) {
        m_TotalResponsePoints.push_back({FrequencyToX(m_Response.frequenciesHz[point]),
            GainToY(m_Response.combinedResponseDb[point])});
        for (std::size_t band = 0; band < m_BandResponsePoints.size(); ++band) {
            m_BandResponsePoints[band].push_back({FrequencyToX(m_Response.frequenciesHz[point]),
                GainToY(m_Response.bandResponseDb[band][point])});
        }
    }
}

void UIParametricEQEditor::RebuildSpectrumGeometry() {
    m_InputSpectrumPoints.clear();
    m_OutputSpectrumPoints.clear();
    if (!m_Spectrum || !m_Spectrum->frequencyBins) return;
    const auto& frequencies = *m_Spectrum->frequencyBins;
    const auto build = [&](const std::vector<float>& magnitude, std::vector<Vec2F>& points) {
        const std::size_t count = std::min(frequencies.size(), magnitude.size());
        points.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            if (frequencies[index] < Audio::kParametricEQMinimumFrequencyHz ||
                frequencies[index] > Audio::kParametricEQMaximumFrequencyHz) continue;
            const float normalized = std::clamp((magnitude[index] + 100.0f) / 100.0f, 0.0f, 1.0f);
            points.push_back({FrequencyToX(frequencies[index]),
                m_PlotBounds.y + (1.0f - normalized) * m_PlotBounds.height});
        }
    };
    build(m_Spectrum->inputMagnitudeDb, m_InputSpectrumPoints);
    build(m_Spectrum->outputMagnitudeDb, m_OutputSpectrumPoints);
}

void UIParametricEQEditor::NotifyChanged() {
    RebuildResponse();
    if (m_OnChanged) m_OnChanged();
}

void UIParametricEQEditor::CycleSelectedType() {
    Audio::ParametricEQBand* band = m_Model.FindBand(m_SelectedBand);
    if (band == nullptr) return;
    const auto next = (static_cast<std::uint8_t>(band->type) + 1u) % 6u;
    band->type = static_cast<Audio::EQBandType>(next);
    if (band->type == Audio::EQBandType::LowCut ||
        band->type == Audio::EQBandType::HighCut ||
        band->type == Audio::EQBandType::Notch) band->gainDb = 0.0f;
    NotifyChanged();
}

void UIParametricEQEditor::RemoveSelected() {
    if (m_Model.RemoveBand(m_SelectedBand)) {
        m_SelectedBand = 0;
        NotifyChanged();
    }
}

} // namespace Engine::UI
