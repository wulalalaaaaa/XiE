#pragma once

#include "Audio/Analysis/SpectrumAnalyzer.h"
#include "Input/InputEvents.h"
#include "Renderer2D/DrawList2D.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Engine::UI {

enum class AudioEditorTrackType : std::uint8_t {
    Pitch,
    Formant,
    ParametricEQ,
    ElectronicNoise,
    Haze,
    Custom
};

enum class AudioEditorAxisScale : std::uint8_t {
    Linear,
    Logarithmic
};

// A track-local coordinate space. The editor never interprets the values;
// adapters own the meaning of the axes (F0, semitones, Hz, dB, ...).
struct AudioEditorAxisBinding {
    float xMinimum = 0.0f;
    float xMaximum = 1.0f;
    float yMinimum = 0.0f;
    float yMaximum = 1.0f;
    AudioEditorAxisScale xScale = AudioEditorAxisScale::Linear;
    AudioEditorAxisScale yScale = AudioEditorAxisScale::Linear;

    [[nodiscard]] float ToScreenX(RectF bounds, float value) const noexcept;
    [[nodiscard]] float ToScreenY(RectF bounds, float value) const noexcept;
    [[nodiscard]] float FromScreenX(RectF bounds, float screen) const noexcept;
    [[nodiscard]] float FromScreenY(RectF bounds, float screen) const noexcept;
    [[nodiscard]] Vec2F ToScreen(RectF bounds, float x, float y) const noexcept {
        return {ToScreenX(bounds, x), ToScreenY(bounds, y)};
    }
};

struct AudioEditorTrackStyle {
    Color4f color{0.25f, 0.72f, 1.0f, 1.0f};
    float lineWidth = 2.0f;
    float nodeRadius = 6.0f;
    float inactiveAlpha = 0.28f;
};

class IAudioEditorTrackAdapter {
public:
    virtual ~IAudioEditorTrackAdapter() = default;

    [[nodiscard]] virtual std::size_t NodeCount() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t NodeIdAt(std::size_t index) const noexcept = 0;
    [[nodiscard]] virtual Vec2F NodePosition(
        std::uint64_t nodeId,
        const AudioEditorAxisBinding& axes,
        RectF plotBounds) const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::uint64_t> HitTest(
        Vec2F point,
        const AudioEditorAxisBinding& axes,
        RectF plotBounds,
        float radius) const noexcept = 0;

    // Called for a node hit or for adapter-owned controls (for example the
    // EQ parameter strip below the plot). Blank-point creation is kept in the
    // generic editor and dispatched through CreatePointAt.
    virtual bool PointerDown(
        Vec2F point,
        bool secondary,
        double timestampSeconds,
        const AudioEditorAxisBinding& axes,
        RectF plotBounds,
        std::optional<std::uint64_t> nodeId) = 0;
    virtual bool PointerMove(
        Vec2F point,
        const AudioEditorAxisBinding& axes,
        RectF plotBounds,
        std::uint64_t nodeId) = 0;
    virtual bool PointerWheel(
        Vec2F point,
        float delta,
        const AudioEditorAxisBinding& axes,
        RectF plotBounds) = 0;
    virtual void PointerUp() noexcept = 0;
    virtual bool CreatePointAt(
        Vec2F point,
        const AudioEditorAxisBinding& axes,
        RectF plotBounds) = 0;
    virtual bool RemoveNode(std::uint64_t nodeId) = 0;
    virtual void DrawOverlay(
        DrawList2D& list,
        const AudioEditorAxisBinding& axes,
        RectF plotBounds,
        bool active) const = 0;
};

struct AudioEditorTrack {
    std::string id;
    std::string label;
    AudioEditorTrackType type = AudioEditorTrackType::Custom;
    AudioEditorAxisBinding axes{};
    AudioEditorTrackStyle style{};
    bool visible = true;
    bool editable = true;
    int zOrder = 0;
    std::shared_ptr<IAudioEditorTrackAdapter> adapter;
};

struct AudioEditorSelection {
    std::size_t trackIndex = 0;
    std::uint64_t nodeId = 0;

    [[nodiscard]] bool IsValid() const noexcept { return nodeId != 0; }
};

class UIAudioCurveEditor {
public:
    UIAudioCurveEditor() = default;

    void SetBounds(RectF bounds) noexcept;
    [[nodiscard]] RectF Bounds() const noexcept { return m_Bounds; }
    [[nodiscard]] RectF PlotBounds() const noexcept { return m_PlotBounds; }
    void SetPlotFooterHeight(float height) noexcept;

    void SetTracks(std::vector<AudioEditorTrack> tracks);
    [[nodiscard]] std::vector<AudioEditorTrack>& Tracks() noexcept { return m_Tracks; }
    [[nodiscard]] const std::vector<AudioEditorTrack>& Tracks() const noexcept { return m_Tracks; }
    void SetActiveTrack(std::size_t index) noexcept;
    [[nodiscard]] std::size_t ActiveTrack() const noexcept { return m_ActiveTrack; }
    [[nodiscard]] AudioEditorSelection Selection() const noexcept { return m_Selection; }
    void ClearSelection() noexcept;

    void SetSpectrumSnapshot(std::shared_ptr<const Audio::SpectrumSnapshot> snapshot);
    void SetCurrentF0(float frequencyHz, float confidence) noexcept;
    void SetChangedCallback(std::function<void(std::size_t)> callback) {
        m_OnChanged = std::move(callback);
    }

    bool PointerDown(Vec2F point, bool secondary, double timestampSeconds);
    bool PointerMove(Vec2F point);
    void PointerUp() noexcept;
    bool PointerWheel(Vec2F point, float delta);
    bool KeyDown(InputKeyCode key);
    void Draw(DrawList2D& list) const;

private:
    [[nodiscard]] bool Contains(RectF rect, Vec2F point) const noexcept;
    [[nodiscard]] std::optional<std::pair<std::size_t, std::uint64_t>> HitNode(
        Vec2F point) const noexcept;
    [[nodiscard]] std::optional<std::size_t> FindNextVisibleByZ(int zOrder) const noexcept;
    [[nodiscard]] Color4f ScaledColor(Color4f color, float alpha) const noexcept;
    void RecalculatePlotBounds() noexcept;
    void NotifyChanged(std::size_t trackIndex);

    RectF m_Bounds{};
    RectF m_PlotBounds{};
    float m_PlotFooterHeight = 0.0f;
    std::vector<AudioEditorTrack> m_Tracks;
    std::size_t m_ActiveTrack = 0;
    AudioEditorSelection m_Selection{};
    std::optional<std::pair<std::size_t, std::uint64_t>> m_Hovered;
    bool m_Dragging = false;
    Vec2F m_LastClickPosition{};
    double m_LastClickTime = -10.0;
    std::shared_ptr<const Audio::SpectrumSnapshot> m_Spectrum;
    std::uint64_t m_SpectrumRevision = 0;
    float m_CurrentF0 = 0.0f;
    float m_F0Confidence = 0.0f;
    std::function<void(std::size_t)> m_OnChanged;
};

} // namespace Engine::UI
