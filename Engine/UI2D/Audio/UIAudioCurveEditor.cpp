#include "UI2D/Audio/UIAudioCurveEditor.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Engine::UI {
namespace {

constexpr float kMinimumPositive = 1.0e-6f;
constexpr float kNodeHitRadius = 14.0f;
constexpr float kF0ConfidenceThreshold = 0.25f;
constexpr Color4f kEditorBackground{0.005f, 0.025f, 0.043f, 1.0f};
constexpr Color4f kGrid{0.08f, 0.26f, 0.35f, 0.50f};
constexpr Color4f kZeroLine{0.25f, 0.62f, 0.73f, 0.82f};

float Normalize(float value, float minimum, float maximum) noexcept {
    return std::clamp((value - minimum) / std::max(kMinimumPositive, maximum - minimum), 0.0f, 1.0f);
}

float Denormalize(float value, float minimum, float maximum) noexcept {
    return std::lerp(minimum, maximum, std::clamp(value, 0.0f, 1.0f));
}

float LogNormalize(float value, float minimum, float maximum) noexcept {
    const float safeMinimum = std::max(kMinimumPositive, minimum);
    const float safeMaximum = std::max(safeMinimum + kMinimumPositive, maximum);
    const float safeValue = std::clamp(value, safeMinimum, safeMaximum);
    return std::clamp(std::log(safeValue / safeMinimum) /
        std::log(safeMaximum / safeMinimum), 0.0f, 1.0f);
}

float LogDenormalize(float value, float minimum, float maximum) noexcept {
    const float safeMinimum = std::max(kMinimumPositive, minimum);
    const float safeMaximum = std::max(safeMinimum + kMinimumPositive, maximum);
    return safeMinimum * std::pow(safeMaximum / safeMinimum, std::clamp(value, 0.0f, 1.0f));
}

float DistanceSquared(Vec2F a, Vec2F b) noexcept {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace

float AudioEditorAxisBinding::ToScreenX(RectF bounds, float value) const noexcept {
    const float t = xScale == AudioEditorAxisScale::Logarithmic
        ? LogNormalize(value, xMinimum, xMaximum)
        : Normalize(value, xMinimum, xMaximum);
    return bounds.x + t * bounds.width;
}

float AudioEditorAxisBinding::ToScreenY(RectF bounds, float value) const noexcept {
    const float t = yScale == AudioEditorAxisScale::Logarithmic
        ? LogNormalize(value, yMinimum, yMaximum)
        : Normalize(value, yMinimum, yMaximum);
    return bounds.y + (1.0f - t) * bounds.height;
}

float AudioEditorAxisBinding::FromScreenX(RectF bounds, float screen) const noexcept {
    const float t = Normalize(screen, bounds.x, bounds.x + bounds.width);
    return xScale == AudioEditorAxisScale::Logarithmic
        ? LogDenormalize(t, xMinimum, xMaximum)
        : Denormalize(t, xMinimum, xMaximum);
}

float AudioEditorAxisBinding::FromScreenY(RectF bounds, float screen) const noexcept {
    const float t = Normalize(screen, bounds.y, bounds.y + bounds.height);
    const float normalized = 1.0f - t;
    return yScale == AudioEditorAxisScale::Logarithmic
        ? LogDenormalize(normalized, yMinimum, yMaximum)
        : Denormalize(normalized, yMinimum, yMaximum);
}

void UIAudioCurveEditor::SetBounds(RectF bounds) noexcept {
    m_Bounds = bounds;
    RecalculatePlotBounds();
}

void UIAudioCurveEditor::SetPlotFooterHeight(float height) noexcept {
    m_PlotFooterHeight = std::max(0.0f, height);
    RecalculatePlotBounds();
}

void UIAudioCurveEditor::SetTracks(std::vector<AudioEditorTrack> tracks) {
    m_Tracks = std::move(tracks);
    if (m_Tracks.empty()) {
        m_ActiveTrack = 0;
        ClearSelection();
        return;
    }
    m_ActiveTrack = std::min(m_ActiveTrack, m_Tracks.size() - 1);
    if (m_Selection.trackIndex >= m_Tracks.size()) ClearSelection();
}

void UIAudioCurveEditor::SetActiveTrack(std::size_t index) noexcept {
    if (m_Tracks.empty()) {
        m_ActiveTrack = 0;
        return;
    }
    if (index < m_Tracks.size() && m_Tracks[index].visible) {
        m_ActiveTrack = index;
        if (m_Selection.IsValid() && m_Selection.trackIndex != index) ClearSelection();
    }
}

void UIAudioCurveEditor::ClearSelection() noexcept {
    m_Selection = {};
    m_Hovered.reset();
}

void UIAudioCurveEditor::SetSpectrumSnapshot(
    std::shared_ptr<const Audio::SpectrumSnapshot> snapshot) {
    if (!snapshot || snapshot->revision == m_SpectrumRevision) return;
    m_SpectrumRevision = snapshot->revision;
    m_Spectrum = std::move(snapshot);
}

void UIAudioCurveEditor::SetCurrentF0(float frequencyHz, float confidence) noexcept {
    m_CurrentF0 = frequencyHz;
    m_F0Confidence = confidence;
}

bool UIAudioCurveEditor::PointerDown(Vec2F point, bool secondary, double timestampSeconds) {
    if (!Contains(m_Bounds, point) || m_Tracks.empty()) return false;

    const auto hit = HitNode(point);
    if (hit) {
        m_ActiveTrack = hit->first;
        m_Selection = {hit->first, hit->second};
        AudioEditorTrack& track = m_Tracks[hit->first];
        const bool handled = track.adapter && track.editable && track.adapter->PointerDown(
            point, secondary, timestampSeconds, track.axes, m_PlotBounds, hit->second);
        m_Dragging = !secondary && track.editable;
        m_LastClickPosition = point;
        m_LastClickTime = timestampSeconds;
        return handled || true;
    }

    const bool doubleClick = timestampSeconds - m_LastClickTime <= 0.35 &&
        DistanceSquared(point, m_LastClickPosition) <= 12.0f * 12.0f;
    m_LastClickPosition = point;
    m_LastClickTime = timestampSeconds;

    if (m_ActiveTrack >= m_Tracks.size()) return true;
    AudioEditorTrack& active = m_Tracks[m_ActiveTrack];
    if (active.adapter && active.adapter->PointerDown(
            point, secondary, timestampSeconds, active.axes, m_PlotBounds, std::nullopt)) {
        return true;
    }
    if (!secondary && doubleClick && active.visible && active.editable &&
        Contains(m_PlotBounds, point) && active.adapter &&
        active.adapter->CreatePointAt(point, active.axes, m_PlotBounds)) {
        std::uint64_t closestId = 0;
        float closestDistance = std::numeric_limits<float>::max();
        for (std::size_t index = 0; index < active.adapter->NodeCount(); ++index) {
            const std::uint64_t id = active.adapter->NodeIdAt(index);
            const Vec2F center = active.adapter->NodePosition(id, active.axes, m_PlotBounds);
            const float distance = DistanceSquared(point, center);
            if (distance < closestDistance) {
                closestDistance = distance;
                closestId = id;
            }
        }
        m_Selection = {m_ActiveTrack, closestId};
        m_Dragging = false;
        NotifyChanged(m_ActiveTrack);
        return true;
    }
    if (!secondary) ClearSelection();
    return true;
}

bool UIAudioCurveEditor::PointerMove(Vec2F point) {
    if (!m_Dragging || !m_Selection.IsValid() ||
        m_Selection.trackIndex >= m_Tracks.size()) {
        m_Hovered = HitNode(point);
        return false;
    }
    AudioEditorTrack& track = m_Tracks[m_Selection.trackIndex];
    if (!track.adapter || !track.editable) return false;
    const bool changed = track.adapter->PointerMove(
        point, track.axes, m_PlotBounds, m_Selection.nodeId);
    if (changed) NotifyChanged(m_Selection.trackIndex);
    return changed;
}

void UIAudioCurveEditor::PointerUp() noexcept {
    if (m_Selection.IsValid() && m_Selection.trackIndex < m_Tracks.size()) {
        if (const auto& adapter = m_Tracks[m_Selection.trackIndex].adapter) adapter->PointerUp();
    }
    m_Dragging = false;
}

bool UIAudioCurveEditor::PointerWheel(Vec2F point, float delta) {
    if (!Contains(m_Bounds, point) || m_Tracks.empty()) return false;
    if (m_ActiveTrack >= m_Tracks.size()) return false;
    AudioEditorTrack& track = m_Tracks[m_ActiveTrack];
    if (!track.adapter || !track.editable) return false;
    const bool changed = track.adapter->PointerWheel(
        point, delta, track.axes, m_PlotBounds);
    if (changed) NotifyChanged(m_ActiveTrack);
    return changed;
}

bool UIAudioCurveEditor::KeyDown(InputKeyCode key) {
    if (key == InputKeyCode::Escape) {
        ClearSelection();
        return true;
    }
    if (key != InputKeyCode::Delete || !m_Selection.IsValid() ||
        m_Selection.trackIndex >= m_Tracks.size()) return false;
    AudioEditorTrack& track = m_Tracks[m_Selection.trackIndex];
    if (!track.adapter || !track.editable || !track.adapter->RemoveNode(m_Selection.nodeId)) return false;
    const std::size_t changedTrack = m_Selection.trackIndex;
    ClearSelection();
    NotifyChanged(changedTrack);
    return true;
}

void UIAudioCurveEditor::Draw(DrawList2D& list) const {
    list.AddRoundedRect({m_Bounds, 9.0f, kEditorBackground});
    if (m_Tracks.empty()) return;

    const AudioEditorTrack* gridTrack = &m_Tracks[std::min(m_ActiveTrack, m_Tracks.size() - 1)];
    list.PushClipRect(m_PlotBounds);
    constexpr int kVerticalGridCount = 10;
    for (int index = 0; index <= kVerticalGridCount; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(kVerticalGridCount);
        const float x = m_PlotBounds.x + m_PlotBounds.width * t;
        list.AddLine({{x, m_PlotBounds.y}, {x, m_PlotBounds.y + m_PlotBounds.height},
            index == 0 || index == kVerticalGridCount ? 1.3f : 1.0f, kGrid});
    }
    constexpr int kHorizontalGridCount = 8;
    const bool hasZero = gridTrack->axes.yMinimum <= 0.0f &&
        gridTrack->axes.yMaximum >= 0.0f;
    const float zeroY = hasZero
        ? gridTrack->axes.ToScreenY(m_PlotBounds, 0.0f) : -1.0f;
    for (int index = 0; index <= kHorizontalGridCount; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(kHorizontalGridCount);
        const float y = m_PlotBounds.y + m_PlotBounds.height * t;
        const bool zeroLine = hasZero && std::abs(y - zeroY) <= 0.75f;
        list.AddLine({{m_PlotBounds.x, y}, {m_PlotBounds.x + m_PlotBounds.width, y},
            zeroLine ? 1.6f : 1.0f, zeroLine ? kZeroLine : kGrid});
    }

    const auto drawSpectrum = [&] {
        const AudioEditorTrack* eqTrack = nullptr;
        for (const auto& track : m_Tracks) {
            if (track.visible && track.type == AudioEditorTrackType::ParametricEQ &&
                track.adapter) { eqTrack = &track; break; }
        }
        if (!eqTrack || !m_Spectrum || !m_Spectrum->frequencyBins) return;
        const auto& frequencies = *m_Spectrum->frequencyBins;
        const auto draw = [&](const std::vector<float>& magnitudes, Color4f color, float width) {
            const std::size_t count = std::min(frequencies.size(), magnitudes.size());
            Vec2F previous{};
            bool hasPrevious = false;
            for (std::size_t index = 0; index < count; ++index) {
                if (frequencies[index] < eqTrack->axes.xMinimum || frequencies[index] > eqTrack->axes.xMaximum) continue;
                const float normalized = std::clamp((magnitudes[index] + 100.0f) / 100.0f, 0.0f, 1.0f);
                const Vec2F current{eqTrack->axes.ToScreenX(m_PlotBounds, frequencies[index]),
                    m_PlotBounds.y + (1.0f - normalized) * m_PlotBounds.height};
                if (hasPrevious) list.AddLine({previous, current, width, color});
                previous = current;
                hasPrevious = true;
            }
        };
        draw(m_Spectrum->inputMagnitudeDb, {0.18f, 0.56f, 0.73f, 0.34f}, 1.2f);
        draw(m_Spectrum->outputMagnitudeDb, {0.30f, 0.91f, 0.67f, 0.48f}, 1.5f);
    };
    drawSpectrum();

    const auto forEachTrackInZOrder = [&](auto&& callback) {
        int currentZ = std::numeric_limits<int>::min();
        for (;;) {
            int nextZ = std::numeric_limits<int>::max();
            bool found = false;
            for (const auto& track : m_Tracks) {
                if (track.visible && track.adapter && track.zOrder > currentZ) {
                    nextZ = std::min(nextZ, track.zOrder);
                    found = true;
                }
            }
            if (!found) break;
            currentZ = nextZ;
            for (std::size_t index = 0; index < m_Tracks.size(); ++index) {
                if (m_Tracks[index].visible && m_Tracks[index].adapter &&
                    m_Tracks[index].zOrder == currentZ) callback(index, m_Tracks[index]);
            }
        }
    };
    forEachTrackInZOrder([&](std::size_t index, const AudioEditorTrack& track) {
        track.adapter->DrawOverlay(list, track.axes, m_PlotBounds, index == m_ActiveTrack);
    });

    forEachTrackInZOrder([&](std::size_t index, const AudioEditorTrack& track) {
        const float alpha = index == m_ActiveTrack ? 1.0f : track.style.inactiveAlpha;
        Color4f color = ScaledColor(track.style.color, alpha);
        for (std::size_t node = 0; node < track.adapter->NodeCount(); ++node) {
            const std::uint64_t id = track.adapter->NodeIdAt(node);
            const Vec2F center = track.adapter->NodePosition(id, track.axes, m_PlotBounds);
            const bool selected = m_Selection.IsValid() && m_Selection.trackIndex == index &&
                m_Selection.nodeId == id;
            const bool hovered = m_Hovered && m_Hovered->first == index && m_Hovered->second == id;
            const float radius = (selected || hovered ? track.style.nodeRadius + 2.0f : track.style.nodeRadius);
            list.AddCircle({center, radius, color});
            list.AddRing({center, radius + (selected ? 3.0f : 1.5f), selected ? 2.0f : 1.0f,
                selected ? Color4f{1.0f, 0.88f, 0.43f, alpha} :
                Color4f{0.90f, 0.95f, 1.0f, alpha * 0.82f}});
        }
    });

    for (const auto& track : m_Tracks) {
        if (track.type != AudioEditorTrackType::Pitch || !track.visible) continue;
        if (m_F0Confidence < kF0ConfidenceThreshold || m_CurrentF0 <= 0.0f) break;
        const float x = track.axes.ToScreenX(m_PlotBounds, m_CurrentF0);
        const float alpha = std::clamp(m_F0Confidence, 0.25f, 1.0f);
        list.AddLine({{x, m_PlotBounds.y}, {x, m_PlotBounds.y + m_PlotBounds.height},
            1.5f, {1.0f, 0.82f, 0.32f, alpha}});
        list.AddCircle({{x, m_PlotBounds.y + 8.0f}, 3.5f, {1.0f, 0.82f, 0.32f, alpha}});
        break;
    }
    list.PopClip();
}

bool UIAudioCurveEditor::Contains(RectF rect, Vec2F point) const noexcept {
    return point.x >= rect.x && point.y >= rect.y &&
        point.x <= rect.x + rect.width && point.y <= rect.y + rect.height;
}

std::optional<std::pair<std::size_t, std::uint64_t>> UIAudioCurveEditor::HitNode(
    Vec2F point) const noexcept {
    auto hitTrack = [&](std::size_t index) -> std::optional<std::pair<std::size_t, std::uint64_t>> {
        if (index >= m_Tracks.size()) return std::nullopt;
        const auto& track = m_Tracks[index];
        if (!track.visible || !track.adapter) return std::nullopt;
        if (const auto id = track.adapter->HitTest(point, track.axes, m_PlotBounds, kNodeHitRadius)) {
            return std::make_pair(index, *id);
        }
        return std::nullopt;
    };
    if (const auto active = hitTrack(m_ActiveTrack)) return active;
    int currentZ = std::numeric_limits<int>::max();
    for (;;) {
        int nextZ = std::numeric_limits<int>::min();
        for (const auto& track : m_Tracks) {
            if (track.visible && track.adapter && track.zOrder < currentZ) {
                nextZ = std::max(nextZ, track.zOrder);
            }
        }
        if (nextZ == std::numeric_limits<int>::min()) break;
        currentZ = nextZ;
        for (std::size_t index = 0; index < m_Tracks.size(); ++index) {
            if (index != m_ActiveTrack && m_Tracks[index].zOrder == currentZ) {
                if (const auto result = hitTrack(index)) return result;
            }
        }
    }
    return std::nullopt;
}

Color4f UIAudioCurveEditor::ScaledColor(Color4f color, float alpha) const noexcept {
    color.a *= alpha;
    return color;
}

void UIAudioCurveEditor::RecalculatePlotBounds() noexcept {
    m_PlotBounds = {m_Bounds.x, m_Bounds.y, m_Bounds.width,
        std::max(1.0f, m_Bounds.height - m_PlotFooterHeight)};
}

void UIAudioCurveEditor::NotifyChanged(std::size_t trackIndex) {
    if (m_OnChanged) m_OnChanged(trackIndex);
}

} // namespace Engine::UI
