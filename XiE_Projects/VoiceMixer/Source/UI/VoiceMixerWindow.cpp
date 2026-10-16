#include "UI/VoiceMixerWindow.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <type_traits>

namespace XiE::VoiceMixer {
namespace {

using Engine::Color4f;
using Engine::RectF;
using Engine::Vec2F;

constexpr Color4f kBackground{0.008f, 0.022f, 0.034f, 1.0f};
constexpr Color4f kPanel{0.015f, 0.043f, 0.061f, 1.0f};
constexpr Color4f kPanelRaised{0.040f, 0.075f, 0.100f, 1.0f};
constexpr Color4f kPitch{0.20f, 0.74f, 1.0f, 1.0f};
constexpr Color4f kFormant{0.93f, 0.38f, 0.82f, 1.0f};
constexpr Color4f kElectronicMist{0.92f, 0.54f, 0.22f, 1.0f};
constexpr Color4f kMuted{0.48f, 0.53f, 0.62f, 1.0f};
constexpr float kTitleHeight = 44.0f;
constexpr float kPadding = 20.0f;

std::string Fixed(float value, int precision = 2) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << value;
    return output.str();
}

CurveEditorModel CombinedCurveModel(const VoicePreset& preset) {
    CurveEditorModel model;
    model.domainMinimum = 70.0f;
    model.domainMaximum = 500.0f;
    model.rangeMinimum = -12.0f;
    model.rangeMaximum = 12.0f;
    model.tracks.push_back({"pitch", "Pitch", preset.pitchCurve, kPitch, true});
    model.tracks.push_back({"formant", "Formant", preset.formantCurve, kFormant, true});
    return model;
}

class CurveTrackAdapter final : public Engine::UI::IAudioEditorTrackAdapter {
public:
    CurveTrackAdapter(UICurveEditor* editor, std::size_t trackIndex)
        : m_Editor(editor), m_TrackIndex(trackIndex) {}

    std::size_t NodeCount() const noexcept override {
        return ValidTrack() ? m_Editor->Model().tracks[m_TrackIndex].curve.Points().size() : 0;
    }

    std::uint64_t NodeIdAt(std::size_t index) const noexcept override {
        if (!ValidTrack()) return 0;
        const auto points = m_Editor->Model().tracks[m_TrackIndex].curve.Points();
        return index < points.size() ? points[index].id : 0;
    }

    Vec2F NodePosition(std::uint64_t nodeId,
        const Engine::UI::AudioEditorAxisBinding& axes,
        RectF plotBounds) const noexcept override {
        if (!ValidTrack()) return {};
        for (const auto& point : m_Editor->Model().tracks[m_TrackIndex].curve.Points()) {
            if (point.id == nodeId) return axes.ToScreen(plotBounds, point.x, point.y);
        }
        return {};
    }

    std::optional<std::uint64_t> HitTest(Vec2F point,
        const Engine::UI::AudioEditorAxisBinding& axes,
        RectF plotBounds, float radius) const noexcept override {
        if (!ValidTrack()) return std::nullopt;
        std::optional<std::uint64_t> result;
        float closest = radius * radius;
        for (const auto& curvePoint : m_Editor->Model().tracks[m_TrackIndex].curve.Points()) {
            const Vec2F center = axes.ToScreen(plotBounds, curvePoint.x, curvePoint.y);
            const float dx = point.x - center.x;
            const float dy = point.y - center.y;
            const float distance = dx * dx + dy * dy;
            if (distance <= closest) {
                closest = distance;
                result = curvePoint.id;
            }
        }
        return result;
    }

    bool PointerDown(Vec2F, bool secondary, double,
        const Engine::UI::AudioEditorAxisBinding&, RectF,
        std::optional<std::uint64_t> nodeId) override {
        if (!ValidTrack() || !nodeId) return false;
        m_Dragging = !secondary;
        m_Editor->SetActiveTrack(m_TrackIndex);
        return true;
    }

    bool PointerMove(Vec2F point,
        const Engine::UI::AudioEditorAxisBinding& axes,
        RectF plotBounds, std::uint64_t nodeId) override {
        if (!m_Dragging || !ValidTrack()) return false;
        const float x = axes.FromScreenX(plotBounds, point.x);
        const float y = axes.FromScreenY(plotBounds, point.y);
        auto& model = m_Editor->Model();
        const float clampedX = std::clamp(x, model.domainMinimum, model.domainMaximum);
        const float clampedY = std::clamp(y, model.rangeMinimum, model.rangeMaximum);
        return model.tracks[m_TrackIndex].curve.MovePoint(nodeId, clampedX, clampedY) ==
            Engine::Curve::CurveStatus::Ok;
    }

    bool PointerWheel(Vec2F, float, const Engine::UI::AudioEditorAxisBinding&, RectF) override {
        return false;
    }

    void PointerUp() noexcept override { m_Dragging = false; }

    bool CreatePointAt(Vec2F point,
        const Engine::UI::AudioEditorAxisBinding& axes, RectF plotBounds) override {
        if (!ValidTrack()) return false;
        auto& model = m_Editor->Model();
        Engine::Curve::CurvePoint created;
        created.x = std::clamp(axes.FromScreenX(plotBounds, point.x),
            model.domainMinimum, model.domainMaximum);
        created.y = std::clamp(axes.FromScreenY(plotBounds, point.y),
            model.rangeMinimum, model.rangeMaximum);
        return model.tracks[m_TrackIndex].curve.AddPoint(created) ==
            Engine::Curve::CurveStatus::Ok;
    }

    bool RemoveNode(std::uint64_t nodeId) override {
        return ValidTrack() && model().curve.RemovePoint(nodeId) == Engine::Curve::CurveStatus::Ok;
    }

    void DrawOverlay(Engine::DrawList2D& list,
        const Engine::UI::AudioEditorAxisBinding& axes,
        RectF plotBounds, bool active) const override {
        if (!ValidTrack()) return;
        const auto points = model().curve.Points();
        if (points.empty()) return;
        const auto color = [&] {
            Color4f value = model().color;
            value.a *= active ? 0.95f : 0.30f;
            return value;
        }();
        Vec2F previous{};
        bool hasPrevious = false;
        constexpr std::size_t samples = 256;
        for (std::size_t index = 0; index < samples; ++index) {
            const float t = static_cast<float>(index) / static_cast<float>(samples - 1);
            const auto& editorModel = m_Editor->Model();
            const float x = std::lerp(editorModel.domainMinimum, editorModel.domainMaximum, t);
            const float y = Engine::Curve::CurveSampler::Sample(points, x);
            const Vec2F current = axes.ToScreen(plotBounds, x, y);
            if (hasPrevious) list.AddLine({previous, current,
                active ? 2.4f : 1.5f, color});
            previous = current;
            hasPrevious = true;
        }
    }

private:
    [[nodiscard]] bool ValidTrack() const noexcept {
        return m_Editor != nullptr && m_TrackIndex < m_Editor->Model().tracks.size();
    }
    [[nodiscard]] const CurveEditorTrack& model() const noexcept {
        return m_Editor->Model().tracks[m_TrackIndex];
    }
    [[nodiscard]] CurveEditorTrack& model() noexcept {
        return m_Editor->Model().tracks[m_TrackIndex];
    }

    UICurveEditor* m_Editor = nullptr;
    std::size_t m_TrackIndex = 0;
    bool m_Dragging = false;
};

class EQTrackAdapter final : public Engine::UI::IAudioEditorTrackAdapter {
public:
    explicit EQTrackAdapter(Engine::UI::UIParametricEQEditor* editor) : m_Editor(editor) {}

    std::size_t NodeCount() const noexcept override {
        return m_Editor ? m_Editor->Model().Bands().size() : 0;
    }
    std::uint64_t NodeIdAt(std::size_t index) const noexcept override {
        if (!m_Editor || index >= m_Editor->Model().Bands().size()) return 0;
        return m_Editor->Model().Bands()[index].id;
    }
    Vec2F NodePosition(std::uint64_t nodeId,
        const Engine::UI::AudioEditorAxisBinding& axes,
        RectF plotBounds) const noexcept override {
        if (!m_Editor) return {};
        for (const auto& band : m_Editor->Model().Bands()) {
            if (band.id != nodeId) continue;
            const bool hasGain = band.type == Engine::Audio::EQBandType::Bell ||
                band.type == Engine::Audio::EQBandType::LowShelf ||
                band.type == Engine::Audio::EQBandType::HighShelf;
            return axes.ToScreen(plotBounds, band.frequencyHz, hasGain ? band.gainDb : 0.0f);
        }
        return {};
    }
    std::optional<std::uint64_t> HitTest(Vec2F point,
        const Engine::UI::AudioEditorAxisBinding&, RectF plotBounds, float) const noexcept override {
        if (!m_Editor || !Contains(plotBounds, point)) return std::nullopt;
        return m_Editor->HitBandAt(point);
    }
    bool PointerDown(Vec2F point, bool secondary, double timestamp,
        const Engine::UI::AudioEditorAxisBinding&, RectF plotBounds,
        std::optional<std::uint64_t> nodeId) override {
        if (!m_Editor) return false;
        // The EQ control strip remains owned by its adapter, while blank plot
        // space is handled by the generic editor's CreatePointAt path.
        if (!nodeId && Contains(plotBounds, point)) return false;
        return m_Editor->PointerDown(point, secondary, timestamp);
    }
    bool PointerMove(Vec2F point,
        const Engine::UI::AudioEditorAxisBinding&, RectF, std::uint64_t) override {
        return m_Editor && m_Editor->PointerMove(point);
    }
    bool PointerWheel(Vec2F point, float delta,
        const Engine::UI::AudioEditorAxisBinding&, RectF) override {
        return m_Editor && m_Editor->PointerWheel(point, delta);
    }
    void PointerUp() noexcept override { if (m_Editor) m_Editor->PointerUp(); }
    bool CreatePointAt(Vec2F point,
        const Engine::UI::AudioEditorAxisBinding&, RectF) override {
        return m_Editor && m_Editor->CreateBellBandAt(point);
    }
    bool RemoveNode(std::uint64_t nodeId) override {
        if (!m_Editor) return false;
        const auto* band = m_Editor->Model().FindBand(static_cast<Engine::Audio::EQBandId>(nodeId));
        if (!band) return false;
        return m_Editor->RemoveBand(band->id);
    }
    void DrawOverlay(Engine::DrawList2D& list,
        const Engine::UI::AudioEditorAxisBinding&, RectF, bool active) const override {
        if (m_Editor) m_Editor->DrawUnifiedOverlay(list, active);
    }

private:
    static bool Contains(RectF rect, Vec2F point) noexcept {
        return point.x >= rect.x && point.y >= rect.y &&
            point.x <= rect.x + rect.width && point.y <= rect.y + rect.height;
    }
    Engine::UI::UIParametricEQEditor* m_Editor = nullptr;
};

class ElectronicNoiseTrackAdapter final : public Engine::UI::IAudioEditorTrackAdapter {
public:
    explicit ElectronicNoiseTrackAdapter(Engine::Curve::CurveModel* curve) : m_Curve(curve) {}

    std::size_t NodeCount() const noexcept override {
        return m_Curve ? m_Curve->Points().size() : 0;
    }
    std::uint64_t NodeIdAt(std::size_t index) const noexcept override {
        if (!m_Curve || index >= m_Curve->Points().size()) return 0;
        return m_Curve->Points()[index].id;
    }
    Vec2F NodePosition(std::uint64_t nodeId,
        const Engine::UI::AudioEditorAxisBinding& axes, RectF plotBounds) const noexcept override {
        if (!m_Curve) return {};
        for (const auto& point : m_Curve->Points()) {
            if (point.id == nodeId) return axes.ToScreen(plotBounds, point.x, point.y);
        }
        return {};
    }
    std::optional<std::uint64_t> HitTest(Vec2F point,
        const Engine::UI::AudioEditorAxisBinding& axes, RectF plotBounds, float radius) const noexcept override {
        if (!m_Curve) return std::nullopt;
        std::optional<std::uint64_t> result;
        float closest = radius * radius;
        for (const auto& curvePoint : m_Curve->Points()) {
            const Vec2F center = axes.ToScreen(plotBounds, curvePoint.x, curvePoint.y);
            const float dx = point.x - center.x;
            const float dy = point.y - center.y;
            const float distance = dx * dx + dy * dy;
            if (distance <= closest) {
                closest = distance;
                result = curvePoint.id;
            }
        }
        return result;
    }
    bool PointerDown(Vec2F, bool secondary, double,
        const Engine::UI::AudioEditorAxisBinding&, RectF,
        std::optional<std::uint64_t> nodeId) override {
        if (!m_Curve || !nodeId) return false;
        m_Dragging = !secondary;
        return true;
    }
    bool PointerMove(Vec2F point,
        const Engine::UI::AudioEditorAxisBinding& axes, RectF plotBounds,
        std::uint64_t nodeId) override {
        if (!m_Dragging || !m_Curve) return false;
        const float x = std::clamp(axes.FromScreenX(plotBounds, point.x),
            Engine::Audio::kElectronicMistMinimumFrequencyHz,
            Engine::Audio::kElectronicMistMaximumFrequencyHz);
        const float y = std::clamp(axes.FromScreenY(plotBounds, point.y), 0.0f, 1.0f);
        return m_Curve->MovePoint(nodeId, x, y) == Engine::Curve::CurveStatus::Ok;
    }
    bool PointerWheel(Vec2F, float, const Engine::UI::AudioEditorAxisBinding&, RectF) override {
        return false;
    }
    void PointerUp() noexcept override { m_Dragging = false; }
    bool CreatePointAt(Vec2F point,
        const Engine::UI::AudioEditorAxisBinding& axes, RectF plotBounds) override {
        if (!m_Curve) return false;
        Engine::Curve::CurvePoint created;
        created.x = std::clamp(axes.FromScreenX(plotBounds, point.x),
            Engine::Audio::kElectronicMistMinimumFrequencyHz,
            Engine::Audio::kElectronicMistMaximumFrequencyHz);
        created.y = std::clamp(axes.FromScreenY(plotBounds, point.y), 0.0f, 1.0f);
        return m_Curve->AddPoint(created) == Engine::Curve::CurveStatus::Ok;
    }
    bool RemoveNode(std::uint64_t nodeId) override {
        return m_Curve && m_Curve->RemovePoint(nodeId) == Engine::Curve::CurveStatus::Ok;
    }
    void DrawOverlay(Engine::DrawList2D& list,
        const Engine::UI::AudioEditorAxisBinding& axes, RectF plotBounds, bool active) const override {
        if (!m_Curve || m_Curve->Points().empty()) return;
        Color4f color = kElectronicMist;
        color.a = active ? 0.95f : 0.28f;
        Vec2F previous{};
        bool hasPrevious = false;
        constexpr std::size_t samples = 256;
        for (std::size_t index = 0; index < samples; ++index) {
            const float t = static_cast<float>(index) / static_cast<float>(samples - 1);
            const float frequency = Engine::Audio::kElectronicMistMinimumFrequencyHz *
                std::pow(Engine::Audio::kElectronicMistMaximumFrequencyHz /
                    Engine::Audio::kElectronicMistMinimumFrequencyHz, t);
            const float value = Engine::Curve::CurveSampler::Sample(m_Curve->Points(), frequency);
            const Vec2F current = axes.ToScreen(plotBounds, frequency, value);
            if (hasPrevious) list.AddLine({previous, current, active ? 2.4f : 1.5f, color});
            previous = current;
            hasPrevious = true;
        }
    }

private:
    Engine::Curve::CurveModel* m_Curve = nullptr;
    bool m_Dragging = false;
};

} // namespace

VoiceMixerWindow::VoiceMixerWindow(
    VoiceMixerAudioController& audio,
    std::filesystem::path presetPath)
    : m_Audio(audio),
      m_PresetPath(std::move(presetPath)),
      m_Preset(MakeNeutralPreset()),
      m_CurveEditor(CombinedCurveModel(m_Preset)),
      m_EQEditor(m_Preset.eq) {
    using Engine::UI::AudioEditorAxisBinding;
    using Engine::UI::AudioEditorAxisScale;
    using Engine::UI::AudioEditorTrack;
    using Engine::UI::AudioEditorTrackType;
    AudioEditorAxisBinding voiceAxes{
        70.0f, 500.0f, -12.0f, 12.0f,
        AudioEditorAxisScale::Logarithmic, AudioEditorAxisScale::Linear};
    AudioEditorAxisBinding eqAxes{
        20.0f, 20'000.0f, -24.0f, 24.0f,
        AudioEditorAxisScale::Logarithmic, AudioEditorAxisScale::Linear};
    AudioEditorAxisBinding electronicAxes{
        20.0f, 20'000.0f, 0.0f, 1.0f,
        AudioEditorAxisScale::Logarithmic, AudioEditorAxisScale::Linear};
    std::vector<AudioEditorTrack> tracks;
    tracks.push_back({"pitch", "Pitch", AudioEditorTrackType::Pitch, voiceAxes,
        {kPitch, 2.5f, 6.0f, 0.30f}, true, true, 20,
        std::make_shared<CurveTrackAdapter>(&m_CurveEditor, 0)});
    tracks.push_back({"formant", "Formant", AudioEditorTrackType::Formant, voiceAxes,
        {kFormant, 2.5f, 6.0f, 0.30f}, true, true, 30,
        std::make_shared<CurveTrackAdapter>(&m_CurveEditor, 1)});
    tracks.push_back({"eq", "EQ", AudioEditorTrackType::ParametricEQ, eqAxes,
        {{0.34f, 0.91f, 0.66f, 1.0f}, 2.4f, 7.0f, 0.30f}, true, true, 10,
        std::make_shared<EQTrackAdapter>(&m_EQEditor)});
    tracks.push_back({"electronicNoise", "Electronic Noise", AudioEditorTrackType::ElectronicNoise,
        electronicAxes, {kElectronicMist, 2.2f, 6.0f, 0.30f}, true, true, 25,
        std::make_shared<ElectronicNoiseTrackAdapter>(&m_Preset.electronicNoiseCurve)});
    m_UnifiedEditor.SetPlotFooterHeight(58.0f);
    m_UnifiedEditor.SetTracks(std::move(tracks));
    m_UnifiedEditor.SetChangedCallback([this](std::size_t track) {
        if (track < 2 || track == 3) m_CurvesDirty = true;
        else m_EQDirty = true;
    });
    LoadLanguagePreference();
    LoadEditorPagePreference();
    m_CurveEditor.SetChangedCallback([this](std::size_t) { m_CurvesDirty = true; });
    m_EQEditor.SetChangedCallback([this] { m_EQDirty = true; });
    SetEditorPage(m_EditorPage);
    RecalculateLayout();
    ApplyParameters();
    PublishCurves();
    PublishParametricEQ();
}

void VoiceMixerWindow::SetViewport(float width, float height, float dpiScale) {
    width = std::max(1.0f, width);
    height = std::max(1.0f, height);
    dpiScale = std::max(1.0f, dpiScale);
    if (std::abs(width - m_Width) < 0.5f &&
        std::abs(height - m_Height) < 0.5f &&
        std::abs(dpiScale - m_DpiScale) < 0.01f) {
        return;
    }
    m_Width = width;
    m_Height = height;
    m_DpiScale = dpiScale;
    RecalculateLayout();
}

bool VoiceMixerWindow::ConsumeHideRequested() noexcept {
    const bool requested = m_HideRequested;
    m_HideRequested = false;
    return requested;
}

bool VoiceMixerWindow::ConsumeMinimizeRequested() noexcept {
    const bool requested = m_MinimizeRequested;
    m_MinimizeRequested = false;
    return requested;
}

bool VoiceMixerWindow::IsTitleBarDragRegion(Vec2F point) const noexcept {
    return point.y >= 0.0f && point.y < kTitleHeight &&
        point.x >= 0.0f && point.x < m_Layout.language.x - 8.0f;
}

void VoiceMixerWindow::LoadLanguagePreference() {
    const std::filesystem::path path =
        m_PresetPath.parent_path().parent_path() / "Language.txt";
    std::ifstream input(path, std::ios::binary);
    std::string value;
    input >> value;
    m_Language = value == "en" ? Language::English : Language::Chinese;
}

void VoiceMixerWindow::SaveLanguagePreference() const {
    const std::filesystem::path path =
        m_PresetPath.parent_path().parent_path() / "Language.txt";
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (output) output << (m_Language == Language::English ? "en" : "zh");
}

void VoiceMixerWindow::LoadEditorPagePreference() {
    const std::filesystem::path path =
        m_PresetPath.parent_path().parent_path() / "LastEditorPage.txt";
    std::ifstream input(path, std::ios::binary);
    std::string value;
    input >> value;
    m_EditorPage = value == "noise" ? VoiceEditorPage::Noise
        : value == "formant" ? VoiceEditorPage::Formant
        : value == "eq" ? VoiceEditorPage::EQ
        : value == "fx" ? VoiceEditorPage::FX
        : VoiceEditorPage::Pitch;
}

void VoiceMixerWindow::SaveEditorPagePreference() const {
    const std::filesystem::path path =
        m_PresetPath.parent_path().parent_path() / "LastEditorPage.txt";
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return;
    output << (m_EditorPage == VoiceEditorPage::Noise ? "noise"
        : m_EditorPage == VoiceEditorPage::Pitch ? "pitch"
        : m_EditorPage == VoiceEditorPage::Formant ? "formant"
        : m_EditorPage == VoiceEditorPage::EQ ? "eq" : "fx");
}

void VoiceMixerWindow::SetEditorPage(VoiceEditorPage page) {
    m_EditorPage = page;
    if (m_CurveEditor.Model().tracks.size() >= 2) {
        m_CurveEditor.Model().tracks[0].visible = true;
        m_CurveEditor.Model().tracks[1].visible = true;
        if (page == VoiceEditorPage::Pitch) m_CurveEditor.SetActiveTrack(0);
        if (page == VoiceEditorPage::Formant) m_CurveEditor.SetActiveTrack(1);
    }
    if (page == VoiceEditorPage::Pitch) m_UnifiedEditor.SetActiveTrack(0);
    if (page == VoiceEditorPage::Formant) m_UnifiedEditor.SetActiveTrack(1);
    if (page == VoiceEditorPage::EQ) m_UnifiedEditor.SetActiveTrack(2);
    SaveEditorPagePreference();
}

void VoiceMixerWindow::RecalculateLayout() {
    const float contentWidth = m_Width - kPadding * 2.0f;
    m_Layout.language = {m_Width - 156.0f, 8.0f, 58.0f, 28.0f};
    m_Layout.minimize = {m_Width - 88.0f, 8.0f, 34.0f, 28.0f};
    m_Layout.close = {m_Width - 48.0f, 8.0f, 34.0f, 28.0f};

    const float deviceGap = 14.0f;
    const float deviceWidth = (contentWidth - deviceGap) * 0.5f;
    m_Layout.inputDevice = {kPadding, 56.0f, deviceWidth, 42.0f};
    m_Layout.outputDevice = {
        kPadding + deviceWidth + deviceGap, 56.0f, deviceWidth, 42.0f};

    const float toolbarY = 110.0f;
    const float buttonGap = 8.0f;
    float x = kPadding;
    auto nextButton = [&](float width) {
        const RectF rect{x, toolbarY, width, 34.0f};
        x += width + buttonGap;
        return rect;
    };
    m_Layout.refresh = nextButton(78.0f);
    m_Layout.start = nextButton(70.0f);
    m_Layout.stop = nextButton(70.0f);
    m_Layout.restart = nextButton(78.0f);
    m_Layout.monitor = nextButton(88.0f);
    m_Layout.bypass = nextButton(80.0f);

    float right = m_Width - kPadding;
    auto previousButton = [&](float width) {
        right -= width;
        const RectF rect{right, toolbarY, width, 34.0f};
        right -= buttonGap;
        return rect;
    };
    m_Layout.neutral = previousButton(112.0f);
    m_Layout.load = previousButton(70.0f);
    m_Layout.save = previousButton(70.0f);

    const float meterY = 158.0f;
    const float meterGap = 28.0f;
    const float meterWidth = (contentWidth - meterGap) * 0.5f;
    m_Layout.inputMeter = {kPadding, meterY, meterWidth, 9.0f};
    m_Layout.outputMeter = {
        kPadding + meterWidth + meterGap, meterY, meterWidth, 9.0f};

    const float diagnosticsHeight = 144.0f;
    m_Layout.diagnostics = {
        kPadding, m_Height - diagnosticsHeight - 16.0f,
        contentWidth, diagnosticsHeight};

    const float editorTop = 204.0f;
    const float editorBottom = m_Layout.diagnostics.y - 16.0f;
    const float editorHeight = std::max(300.0f, editorBottom - editorTop);
    const float faderPanelWidth = std::clamp(contentWidth * 0.20f, 240.0f, 300.0f);
    const float editorGap = 18.0f;
    m_Layout.curve = {
        kPadding, editorTop,
        contentWidth - faderPanelWidth - editorGap, editorHeight};

    const float legendY = editorTop - 34.0f;
    m_Layout.editorTabBar = {kPadding - 6.0f, legendY - 5.0f,
        contentWidth + 12.0f, 36.0f};
    constexpr float tabWidth = 100.0f;
    constexpr float tabGap = 8.0f;
    m_Layout.noiseTrack = {kPadding, legendY, tabWidth, 26.0f};
    m_Layout.pitchTrack = {kPadding + (tabWidth + tabGap), legendY, tabWidth, 26.0f};
    m_Layout.formantTrack = {kPadding + 2.0f * (tabWidth + tabGap), legendY, tabWidth, 26.0f};
    m_Layout.eqTrack = {kPadding + 3.0f * (tabWidth + tabGap), legendY, tabWidth, 26.0f};
    m_Layout.fxTrack = {kPadding + 4.0f * (tabWidth + tabGap), legendY, tabWidth, 26.0f};
    m_Layout.resetEq = {m_Layout.curve.x + m_Layout.curve.width - 94.0f,
        legendY, 94.0f, 26.0f};
    m_Layout.eqBypass = {m_Layout.resetEq.x - 102.0f,
        legendY, 94.0f, 26.0f};
    m_Layout.noiseEnable = {m_Layout.curve.x + 28.0f, editorTop + 30.0f, 126.0f, 34.0f};
    m_Layout.noiseMode = {m_Layout.curve.x + 166.0f, editorTop + 30.0f, 150.0f, 34.0f};
    m_Layout.noiseBypass = {m_Layout.curve.x + 328.0f, editorTop + 30.0f, 126.0f, 34.0f};
    m_Layout.resetNoise = {m_Layout.curve.x + 466.0f, editorTop + 30.0f, 126.0f, 34.0f};
    m_Layout.noiseStrength = {m_Layout.curve.x + 28.0f, editorTop + 94.0f,
        m_Layout.curve.width - 56.0f, 62.0f};
    m_Layout.noiseWet = {m_Layout.curve.x + 28.0f, editorTop + 168.0f,
        m_Layout.curve.width - 56.0f, 62.0f};

    const float fxGap = 14.0f;
    const float fxCardWidth = std::max(150.0f, (m_Layout.curve.width - 3.0f * fxGap) * 0.5f);
    const float fxCardHeight = std::max(112.0f, (m_Layout.curve.height - 3.0f * fxGap) * 0.5f);
    const float fxLeft = m_Layout.curve.x + fxGap;
    const float fxTop = m_Layout.curve.y + fxGap;
    m_Layout.electronicCard = {fxLeft, fxTop, fxCardWidth, fxCardHeight};
    m_Layout.syntheticCard = {fxLeft + fxCardWidth + fxGap, fxTop, fxCardWidth, fxCardHeight};
    m_Layout.softCard = {fxLeft, fxTop + fxCardHeight + fxGap, fxCardWidth, fxCardHeight};
    m_Layout.spatialCard = {fxLeft + fxCardWidth + fxGap,
        fxTop + fxCardHeight + fxGap, fxCardWidth, fxCardHeight};
    const auto enableRect = [](RectF card) {
        return RectF{card.x + 12.0f, card.y + 40.0f, 82.0f, 24.0f};
    };
    m_Layout.electronicEnable = enableRect(m_Layout.electronicCard);
    m_Layout.syntheticEnable = enableRect(m_Layout.syntheticCard);
    m_Layout.softEnable = enableRect(m_Layout.softCard);
    m_Layout.spatialEnable = enableRect(m_Layout.spatialCard);
    m_Layout.editNoiseCurve = {m_Layout.electronicCard.x + 100.0f,
        m_Layout.electronicCard.y + 40.0f, 112.0f, 24.0f};
    m_Layout.resetNoiseCurve = {m_Layout.electronicCard.x + 218.0f,
        m_Layout.electronicCard.y + 40.0f, 94.0f, 24.0f};
    const auto layoutSliders = [&](RectF card, auto& sliders) {
        const float startY = card.y + 70.0f;
        const float available = std::max(24.0f, card.height - 78.0f);
        const float spacing = available / static_cast<float>(sliders.size());
        const float height = std::max(11.0f, spacing - 1.5f);
        for (std::size_t index = 0; index < sliders.size(); ++index) {
            sliders[index] = {card.x + 12.0f, startY + static_cast<float>(index) * spacing,
                card.width - 24.0f, height};
        }
    };
    layoutSliders(m_Layout.electronicCard, m_Layout.electronicSliders);
    layoutSliders(m_Layout.syntheticCard, m_Layout.syntheticSliders);
    layoutSliders(m_Layout.softCard, m_Layout.softSliders);
    layoutSliders(m_Layout.spatialCard, m_Layout.spatialSliders);

    const float faderX = m_Layout.curve.x + m_Layout.curve.width + editorGap;
    const float faderSlot = faderPanelWidth / static_cast<float>(Slider::Count);
    for (std::size_t index = 0; index < m_Layout.faders.size(); ++index) {
        m_Layout.faders[index] = {
            faderX + faderSlot * static_cast<float>(index) + 3.0f,
            editorTop,
            faderSlot - 6.0f,
            editorHeight};
    }
    m_CurveEditor.SetBounds(m_Layout.curve);
    m_EQEditor.SetBounds(m_Layout.curve);
    m_UnifiedEditor.SetBounds(m_Layout.curve);
}

void VoiceMixerWindow::HandleInput(const Engine::InputEvent& event) {
    std::visit([this](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, Engine::PointerMoveEvent>) {
            m_HoverPoint = value.logicalPosition;
            m_HoverValid = true;
            if (m_ActiveNoiseSlider) {
                UpdateNoiseSlider(*m_ActiveNoiseSlider, value.logicalPosition.x);
            } else if (m_ActiveFXSlider) {
                UpdateFXSlider(*m_ActiveFXSlider, value.logicalPosition.x);
            } else if (m_ActiveSlider) {
                UpdateSlider(*m_ActiveSlider, value.logicalPosition.y);
            } else if (m_EditorPage != VoiceEditorPage::Noise &&
                m_EditorPage != VoiceEditorPage::FX) {
                (void)m_UnifiedEditor.PointerMove(value.logicalPosition);
            }
        } else if constexpr (std::is_same_v<T, Engine::PointerWheelEvent>) {
            if (m_EditorPage == VoiceEditorPage::Noise) {
                if (Contains(m_Layout.noiseStrength, value.logicalPosition)) {
                    m_Preset.parameters.noiseStrength = std::clamp(
                        m_Preset.parameters.noiseStrength + value.logicalDelta.y * 0.02f,
                        0.0f, 1.0f);
                    ApplyParameters();
                    return;
                }
                if (Contains(m_Layout.noiseWet, value.logicalPosition)) {
                    m_Preset.parameters.noiseWet = std::clamp(
                        m_Preset.parameters.noiseWet + value.logicalDelta.y * 0.02f,
                        0.0f, 1.0f);
                    ApplyParameters();
                    return;
                }
            }
            if (m_EditorPage == VoiceEditorPage::FX) {
                for (std::size_t index = 0; index < m_Layout.electronicSliders.size(); ++index) {
                    if (Contains(m_Layout.electronicSliders[index], value.logicalPosition)) {
                        AdjustFXSlider(static_cast<FXSlider>(
                            static_cast<std::size_t>(FXSlider::MistAmount) + index),
                            value.logicalDelta.y);
                        return;
                    }
                }
                for (std::size_t index = 0; index < m_Layout.syntheticSliders.size(); ++index) {
                    if (Contains(m_Layout.syntheticSliders[index], value.logicalPosition)) {
                        AdjustFXSlider(static_cast<FXSlider>(
                            static_cast<std::size_t>(FXSlider::SyntheticAmount) + index),
                            value.logicalDelta.y);
                        return;
                    }
                }
                for (std::size_t index = 0; index < m_Layout.softSliders.size(); ++index) {
                    if (Contains(m_Layout.softSliders[index], value.logicalPosition)) {
                        AdjustFXSlider(static_cast<FXSlider>(
                            static_cast<std::size_t>(FXSlider::SoftAmount) + index),
                            value.logicalDelta.y);
                        return;
                    }
                }
                for (std::size_t index = 0; index < m_Layout.spatialSliders.size(); ++index) {
                    if (Contains(m_Layout.spatialSliders[index], value.logicalPosition)) {
                        AdjustFXSlider(static_cast<FXSlider>(
                            static_cast<std::size_t>(FXSlider::SpatialAmount) + index),
                            value.logicalDelta.y);
                        return;
                    }
                }
            }
            if (m_EditorPage != VoiceEditorPage::Noise && m_EditorPage != VoiceEditorPage::FX &&
                m_UnifiedEditor.PointerWheel(value.logicalPosition, value.logicalDelta.y)) return;
            for (std::size_t index = 0; index < m_Layout.faders.size(); ++index) {
                if (Contains(m_Layout.faders[index], value.logicalPosition)) {
                    AdjustSlider(static_cast<Slider>(index), value.logicalDelta.y);
                    return;
                }
            }
        } else if constexpr (std::is_same_v<T, Engine::PointerButtonEvent>) {
            if (value.pressed) {
                const bool secondary = value.button == Engine::PointerButton::Secondary;
                if (m_OpenDropdown != Dropdown::None) {
                    if (!secondary) Activate(value.logicalPosition);
                    return;
                }
                if (m_EditorPage != VoiceEditorPage::Noise && m_EditorPage != VoiceEditorPage::FX &&
                    m_UnifiedEditor.PointerDown(value.logicalPosition, secondary,
                        value.timestampSeconds)) {
                    return;
                }
                if (!secondary) Activate(value.logicalPosition);
            } else {
                m_ActiveSlider.reset();
                m_ActiveNoiseSlider.reset();
                m_ActiveFXSlider.reset();
                m_UnifiedEditor.PointerUp();
                m_CurveEditor.PointerUp();
                m_EQEditor.PointerUp();
            }
        } else if constexpr (std::is_same_v<T, Engine::KeyEvent>) {
            if (value.pressed && m_EditorPage != VoiceEditorPage::Noise) {
                (void)m_UnifiedEditor.KeyDown(value.key);
            }
        }
    }, event);
}

void VoiceMixerWindow::Tick() {
    m_Audio.Poll();
    m_Snapshot = m_Audio.CaptureSnapshot();
    m_EQEditor.SetSampleRate(m_Snapshot.format.sampleRate > 0
        ? m_Snapshot.format.sampleRate : 48'000);
    m_EQEditor.SetSpectrumSnapshot(m_Snapshot.spectrum);
    m_UnifiedEditor.SetSpectrumSnapshot(m_Snapshot.spectrum);
    m_UnifiedEditor.SetCurrentF0(
        m_Snapshot.voice.state.pitch.frequencyHz,
        m_Snapshot.voice.state.pitch.confidence);
    const auto now = std::chrono::steady_clock::now();
    if (m_CurvesDirty &&
        (m_LastCurvePublish.time_since_epoch().count() == 0 ||
         now - m_LastCurvePublish >= std::chrono::milliseconds(33))) {
        PublishCurves();
    }
    if (m_EQDirty &&
        (m_LastEQPublish.time_since_epoch().count() == 0 ||
         now - m_LastEQPublish >= std::chrono::milliseconds(33))) {
        PublishParametricEQ();
    }
}

void VoiceMixerWindow::BuildDrawList(Engine::DrawList2D& list) {
    list.Clear();
    list.Reserve(24'000);
    list.AddSolidRect({{0, 0, m_Width, m_Height}, kBackground});
    list.AddSolidRect({{0, 0, m_Width, kTitleHeight}, {0.018f, 0.045f, 0.064f, 1.0f}});
    DrawText(list, U"XiE Voice Mixer", 20.0f, 11.0f, 18.0f);
    DrawButton(list, m_Layout.language,
        m_Language == Language::Chinese ? U"中文" : U"EN", true, false, true);
    DrawButton(list, m_Layout.minimize, U"_", true, false, true);
    DrawButton(list, m_Layout.close, U"x", true, false, true);

    const bool stopped = m_Snapshot.state == VoiceMixerAudioState::Stopped;
    const bool running = m_Snapshot.state == VoiceMixerAudioState::Running;
    const bool canRefresh = stopped ||
        m_Snapshot.state == VoiceMixerAudioState::DeviceLost ||
        m_Snapshot.state == VoiceMixerAudioState::Error;

    DrawDeviceSelect(list, m_Layout.inputDevice, Localized(U"输入设备", U"INPUT"),
        m_Audio.InputDevices().empty()
            ? std::u32string(Localized(U"未找到输入设备", U"No input endpoints"))
            : ToText(m_Audio.InputDevices()[m_Audio.SelectedInput()].name),
        stopped, m_OpenDropdown == Dropdown::Input);
    DrawDeviceSelect(list, m_Layout.outputDevice, Localized(U"输出设备", U"OUTPUT"),
        m_Audio.OutputDevices().empty()
            ? std::u32string(Localized(U"未找到输出设备", U"No output endpoints"))
            : ToText(m_Audio.OutputDevices()[m_Audio.SelectedOutput()].name),
        stopped, m_OpenDropdown == Dropdown::Output);

    DrawButton(list, m_Layout.refresh, Localized(U"刷新", U"Refresh"), canRefresh, false, true);
    DrawButton(list, m_Layout.start, Localized(U"启动", U"Start"), stopped, running, true);
    DrawButton(list, m_Layout.stop, Localized(U"停止", U"Stop"), !stopped, false, true);
    DrawButton(list, m_Layout.restart, Localized(U"重启", U"Restart"),
        !m_Audio.InputDevices().empty() && !m_Audio.OutputDevices().empty(), false, true);
    const bool monitorAllowed = !m_Audio.MonitorSafetyActive();
    DrawButton(list, m_Layout.monitor, Localized(U"监听", U"Monitor"), monitorAllowed,
        m_Preset.parameters.monitor && monitorAllowed, true);
    DrawButton(list, m_Layout.bypass, Localized(U"旁路直通", U"Bypass"), true, m_Preset.parameters.bypass, true);
    DrawButton(list, m_Layout.save, Localized(U"保存", U"Save"), true, false, true);
    DrawButton(list, m_Layout.load, Localized(U"加载", U"Load"), true, false, true);
    DrawButton(list, m_Layout.neutral, Localized(U"重置", U"Reset Neutral"), true, false, true);

    const Color4f stateColor = running
        ? Color4f{0.30f, 0.95f, 0.58f, 1.0f}
        : Color4f{0.94f, 0.69f, 0.28f, 1.0f};
    DrawText(list, std::u32string(Localized(U"状态  ", U"STATE  ")) + StateText(),
        574.0f, 118.0f, 11.0f, stateColor, 210.0f);
    if (!m_Message.empty()) {
        DrawText(list, m_Message, 790.0f, 118.0f, 10.0f, kMuted,
            std::max(120.0f, m_Layout.save.x - 800.0f));
    }

    DrawMeter(list, m_Layout.inputMeter, m_Snapshot.inputPeak,
        {0.24f, 0.86f, 0.56f, 1.0f});
    DrawMeter(list, m_Layout.outputMeter, m_Snapshot.outputPeak,
        {0.27f, 0.61f, 1.0f, 1.0f});

    list.AddRoundedRect({m_Layout.editorTabBar, 7.0f, kPanel});
    DrawButton(list, m_Layout.noiseTrack, Localized(U"降噪", U"Noise"), true,
        m_EditorPage == VoiceEditorPage::Noise, true);
    DrawButton(list, m_Layout.pitchTrack, Localized(U"音高", U"Pitch curve"), true,
        m_EditorPage == VoiceEditorPage::Pitch, true);
    DrawButton(list, m_Layout.formantTrack, Localized(U"共振峰", U"Formant curve"), true,
        m_EditorPage == VoiceEditorPage::Formant, true);
    DrawButton(list, m_Layout.eqTrack, U"EQ", true,
        m_EditorPage == VoiceEditorPage::EQ, true);
    DrawButton(list, m_Layout.fxTrack, Localized(U"效果", U"FX"), true,
        m_EditorPage == VoiceEditorPage::FX, true);
    list.AddCircle({{m_Layout.noiseTrack.x + 9.0f, m_Layout.noiseTrack.y + 13.0f}, 3.0f,
        {0.31f, 0.92f, 0.75f, 1.0f}});
    list.AddCircle({{m_Layout.pitchTrack.x + 9.0f, m_Layout.pitchTrack.y + 13.0f}, 3.0f, kPitch});
    list.AddCircle({{m_Layout.formantTrack.x + 9.0f, m_Layout.formantTrack.y + 13.0f}, 3.0f, kFormant});
    list.AddCircle({{m_Layout.eqTrack.x + 9.0f, m_Layout.eqTrack.y + 13.0f}, 3.0f,
        {0.34f, 0.91f, 0.66f, 1.0f}});
    list.AddCircle({{m_Layout.fxTrack.x + 9.0f, m_Layout.fxTrack.y + 13.0f}, 3.0f,
        {0.92f, 0.62f, 0.30f, 1.0f}});
    if (m_EditorPage == VoiceEditorPage::Noise) {
        DrawNoisePage(list);
    } else if (m_EditorPage == VoiceEditorPage::FX) {
        DrawFXPage(list);
    } else {
        DrawButton(list, m_Layout.eqBypass,
            Localized(U"EQ 启用", U"EQ Enabled"), true, m_EQEditor.Model().Enabled(), true);
        DrawButton(list, m_Layout.resetEq,
            Localized(U"重置 EQ", U"Reset EQ"), true, false, true);
        m_UnifiedEditor.Draw(list);
        const RectF plot = m_UnifiedEditor.PlotBounds();
        const auto& tracks = m_UnifiedEditor.Tracks();
        const auto& pitchAxes = tracks[0].axes;
        const auto& eqAxes = tracks[2].axes;
        const auto& electronicAxes = tracks[3].axes;
        const std::array<std::pair<float, std::u32string_view>, 10> frequencyLabels{{
            {20.0f, U"20"}, {50.0f, U"50"}, {100.0f, U"100"}, {200.0f, U"200"},
            {500.0f, U"500"}, {1'000.0f, U"1k"}, {2'000.0f, U"2k"},
            {5'000.0f, U"5k"}, {10'000.0f, U"10k"}, {20'000.0f, U"20k"}}};
        for (const auto& [frequency, label] : frequencyLabels) {
            DrawText(list, std::u32string(label), eqAxes.ToScreenX(plot, frequency) + 3.0f,
                plot.y + plot.height - 17.0f, 8.0f, kMuted, 30.0f);
        }
        const std::array<std::pair<float, std::u32string_view>, 6> f0Labels{{
            {80.0f, U"80"}, {100.0f, U"100"}, {150.0f, U"150"},
            {200.0f, U"200"}, {300.0f, U"300"}, {400.0f, U"400"}}};
        for (const auto& [frequency, label] : f0Labels) {
            DrawText(list, std::u32string(label), pitchAxes.ToScreenX(plot, frequency) + 2.0f,
                plot.y + 5.0f, 8.0f, kMuted, 30.0f);
        }
        for (int semitone = -12; semitone <= 12; semitone += 6) {
            DrawText(list, ToText((semitone > 0 ? "+" : "") + std::to_string(semitone) + " st"),
                plot.x + 7.0f, pitchAxes.ToScreenY(plot, static_cast<float>(semitone)) + 4.0f,
                8.0f, kMuted, 52.0f);
        }
        if (m_UnifiedEditor.ActiveTrack() == 3) {
            DrawText(list, U"Electronic Noise", plot.x + 12.0f, plot.y + 6.0f,
                9.0f, kElectronicMist, 140.0f);
            for (int amount = 0; amount <= 100; amount += 25) {
                DrawText(list, ToText(std::to_string(amount) + "%"),
                    plot.x + plot.width - 36.0f,
                    electronicAxes.ToScreenY(plot, static_cast<float>(amount) / 100.0f) + 4.0f,
                    8.0f, kMuted, 42.0f);
            }
        } else {
            for (int db = -24; db <= 24; db += 12) {
                DrawText(list, ToText((db > 0 ? "+" : "") + std::to_string(db) + " dB"),
                    plot.x + plot.width - 48.0f, eqAxes.ToScreenY(plot, static_cast<float>(db)) + 4.0f,
                    8.0f, kMuted, 54.0f);
            }
        }
        if (m_UnifiedEditor.ActiveTrack() == 2) m_EQEditor.DrawControls(list);
        if (m_UnifiedEditor.ActiveTrack() == 2) if (const auto* band = m_EQEditor.SelectedBand()) {
            const auto& controls = m_EQEditor.Controls();
            const bool hasGain = band->type == Engine::Audio::EQBandType::Bell ||
                band->type == Engine::Audio::EQBandType::LowShelf ||
                band->type == Engine::Audio::EQBandType::HighShelf;
            const auto controlText = [&](RectF rect, std::u32string label, std::u32string value) {
                DrawText(list, std::move(label), rect.x + 7.0f, rect.y + 4.0f,
                    7.5f, kMuted, rect.width - 12.0f);
                DrawText(list, std::move(value), rect.x + 7.0f, rect.y + 18.0f,
                    10.0f, {0.90f, 0.95f, 1.0f, 1.0f}, rect.width - 12.0f);
            };
            controlText(controls.enabled, std::u32string(Localized(U"启用", U"ENABLED")),
                std::u32string(Localized(band->enabled ? U"开" : U"关", band->enabled ? U"ON" : U"OFF")));
            controlText(controls.type, std::u32string(Localized(U"类型", U"TYPE")), EQTypeText(band->type));
            controlText(controls.frequency, std::u32string(Localized(U"频率", U"FREQUENCY")),
                ToText(Fixed(band->frequencyHz, band->frequencyHz < 100.0f ? 1 : 0) + " Hz"));
            controlText(controls.gain, std::u32string(Localized(U"增益", U"GAIN")), hasGain
                ? ToText(Fixed(band->gainDb, 1) + " dB") : U"--");
            controlText(controls.q, U"Q", ToText(Fixed(band->q, 2)));
            controlText(controls.slope, std::u32string(Localized(U"斜率", U"SLOPE")),
                (band->type == Engine::Audio::EQBandType::LowCut ||
                 band->type == Engine::Audio::EQBandType::HighCut)
                    ? ToText(Fixed(band->slopeDbPerOctave, 0)) : U"--");
            controlText(controls.remove, std::u32string(Localized(U"删除", U"DELETE")), U"x");
        }
    }

    DrawFader(list, Slider::Wet, Localized(U"混合", U"WET"), m_Preset.parameters.wet, 0.0f, 1.0f);
    DrawFader(list, Slider::Pitch, Localized(U"音高偏移", U"PITCH"), m_Preset.parameters.pitchSemitones,
        -12.0f, 12.0f, U" st");
    DrawFader(list, Slider::Formant, Localized(U"共振峰偏移", U"FORMANT"), m_Preset.parameters.formantSemitones,
        -6.0f, 6.0f, U" st");

    DrawDiagnostics(list);
    DrawDropdown(list);
}

void VoiceMixerWindow::LoadPresetIntoEditor() {
    const std::size_t activeTrack = m_CurveEditor.ActiveTrack();
    m_UnifiedEditor.ClearSelection();
    m_CurveEditor.SetModel(CombinedCurveModel(m_Preset));
    m_CurveEditor.SetActiveTrack(activeTrack);
    m_CurveEditor.SetBounds(m_Layout.curve);
    m_CurveEditor.SetChangedCallback([this](std::size_t) { m_CurvesDirty = true; });
    m_EQEditor.SetModel(m_Preset.eq);
    m_EQEditor.SetBounds(m_Layout.curve);
    m_EQEditor.SetChangedCallback([this] { m_EQDirty = true; });
    SetEditorPage(m_EditorPage);
    ApplyParameters();
    m_CurvesDirty = true;
    m_EQDirty = true;
    PublishCurves();
    PublishParametricEQ();
}

void VoiceMixerWindow::PublishParametricEQ() {
    const Engine::Audio::AudioStatus status = m_Audio.ApplyParametricEQ(m_EQEditor.Model());
    if (!status) {
        m_Message = Localized(U"EQ 发布失败", U"EQ publish failed");
        return;
    }
    m_Preset.eq = m_EQEditor.Model();
    m_EQDirty = false;
    m_LastEQPublish = std::chrono::steady_clock::now();
}

void VoiceMixerWindow::PublishCurves() {
    auto pitch = m_CurveEditor.BuildSnapshot(0);
    auto formant = m_CurveEditor.BuildSnapshot(1);
    auto electronicNoise = Engine::Curve::BuildCurveSnapshot(
        m_Preset.electronicNoiseCurve, Engine::Audio::kElectronicMistCurveLutSize,
        Engine::Audio::kElectronicMistMinimumFrequencyHz,
        Engine::Audio::kElectronicMistMaximumFrequencyHz);
    const Engine::Audio::AudioStatus status =
        m_Audio.ApplyCurveSnapshots(std::move(pitch), std::move(formant),
            std::move(electronicNoise));
    if (!status) {
        m_Message = Localized(U"曲线发布失败", U"Curve publish failed");
        return;
    }
    m_Preset.pitchCurve = m_CurveEditor.Model().tracks[0].curve;
    m_Preset.formantCurve = m_CurveEditor.Model().tracks[1].curve;
    m_CurvesDirty = false;
    m_LastCurvePublish = std::chrono::steady_clock::now();
}

void VoiceMixerWindow::ApplyParameters() {
    // Input/output gain is intentionally controlled by the external mixer.
    // Normalize loaded legacy presets so hidden gain values cannot affect audio.
    m_Preset.parameters.inputGain = 1.0f;
    m_Preset.parameters.outputGain = 1.0f;
    m_Audio.ApplyParameters(m_Preset.parameters);
}

void VoiceMixerWindow::Activate(Vec2F point) {
    const auto state = m_Audio.State();
    if (m_OpenDropdown != Dropdown::None) {
        const std::size_t count = m_OpenDropdown == Dropdown::Input
            ? m_Audio.InputDevices().size()
            : m_Audio.OutputDevices().size();
        for (std::size_t index = 0; index < DropdownVisibleCount(count); ++index) {
            if (Contains(DropdownItemRect(index), point)) {
                if (m_OpenDropdown == Dropdown::Input) {
                    (void)m_Audio.SelectInput(index);
                } else {
                    (void)m_Audio.SelectOutput(index);
                }
                m_OpenDropdown = Dropdown::None;
                return;
            }
        }
        m_OpenDropdown = Dropdown::None;
        return;
    }

    if (Contains(m_Layout.language, point)) {
        m_Language = m_Language == Language::Chinese
            ? Language::English
            : Language::Chinese;
        SaveLanguagePreference();
        m_Message = Localized(U"界面语言已切换为中文", U"Interface language changed to English");
    } else if (Contains(m_Layout.close, point)) {
        m_HideRequested = true;
    } else if (Contains(m_Layout.minimize, point)) {
        m_MinimizeRequested = true;
    } else if (Contains(m_Layout.inputDevice, point) && state == VoiceMixerAudioState::Stopped &&
        !m_Audio.InputDevices().empty()) {
        m_OpenDropdown = Dropdown::Input;
    } else if (Contains(m_Layout.outputDevice, point) && state == VoiceMixerAudioState::Stopped &&
        !m_Audio.OutputDevices().empty()) {
        m_OpenDropdown = Dropdown::Output;
    } else if (Contains(m_Layout.refresh, point) &&
        state != VoiceMixerAudioState::Running &&
        state != VoiceMixerAudioState::Starting) {
        const auto status = m_Audio.RefreshDevices();
        m_Message = status
            ? Localized(U"设备列表已刷新", U"Devices refreshed")
            : Localized(U"设备刷新失败", U"Device refresh failed");
    } else if (Contains(m_Layout.start, point) && state == VoiceMixerAudioState::Stopped) {
        const auto status = m_Audio.Start();
        if (status && m_Audio.MonitorSafetyActive()) {
            m_Preset.parameters.monitor = false;
            ApplyParameters();
            m_Message = Localized(
                U"检测到可能的音频回授，监听已关闭；输入请改用麦克风",
                U"Possible audio feedback route; Monitor disabled. Select a microphone input.");
        } else {
            ApplyParameters();
            m_Message = status
                ? Localized(U"音频处理已启动", U"Audio started")
                : Localized(U"音频启动失败", U"Audio start failed");
        }
    } else if (Contains(m_Layout.stop, point) && state != VoiceMixerAudioState::Stopped) {
        const auto status = m_Audio.Stop();
        m_Message = status
            ? Localized(U"音频已停止，设备已释放", U"Audio stopped; endpoints released")
            : Localized(U"音频停止失败", U"Audio stop failed");
    } else if (Contains(m_Layout.restart, point)) {
        const auto status = m_Audio.Restart();
        ApplyParameters();
        m_Message = status
            ? Localized(U"音频已重启", U"Audio restarted")
            : Localized(U"音频重启失败", U"Audio restart failed");
    } else if (Contains(m_Layout.monitor, point) && !m_Audio.MonitorSafetyActive()) {
        m_Preset.parameters.monitor = !m_Preset.parameters.monitor;
        ApplyParameters();
        if (m_Audio.MonitorSafetyActive() && m_Preset.parameters.monitor) {
            m_Preset.parameters.monitor = false;
            ApplyParameters();
            m_Message = Localized(
                U"可能存在音频回授，监听保护保持关闭",
                U"Monitor safety kept disabled to prevent audio feedback.");
        }
    } else if (Contains(m_Layout.bypass, point)) {
        m_Preset.parameters.bypass = !m_Preset.parameters.bypass;
        ApplyParameters();
    } else if (Contains(m_Layout.save, point)) {
        PublishCurves();
        PublishParametricEQ();
        std::string error;
        if (m_Repository.Save(m_PresetPath, m_Preset, error)) {
            m_Message = Localized(U"预设已保存", U"Preset saved");
        } else {
            m_Message = std::u32string(Localized(U"保存失败：", U"Save failed: ")) +
                ToText(error);
        }
    } else if (Contains(m_Layout.load, point)) {
        VoicePreset loaded;
        std::string error;
        if (m_Repository.Load(m_PresetPath, loaded, error)) {
            m_Preset = std::move(loaded);
            LoadPresetIntoEditor();
            m_Message = Localized(U"预设已加载", U"Preset loaded");
        } else {
            m_Message = std::u32string(Localized(U"加载失败：", U"Load failed: ")) +
                ToText(error);
        }
    } else if (Contains(m_Layout.neutral, point)) {
        m_Preset = MakeNeutralPreset();
        LoadPresetIntoEditor();
        m_Message = Localized(U"已恢复中性预设", U"Neutral preset restored");
    } else if (Contains(m_Layout.noiseTrack, point)) {
        SetEditorPage(VoiceEditorPage::Noise);
    } else if (Contains(m_Layout.pitchTrack, point)) {
        SetEditorPage(VoiceEditorPage::Pitch);
    } else if (Contains(m_Layout.formantTrack, point)) {
        SetEditorPage(VoiceEditorPage::Formant);
    } else if (Contains(m_Layout.eqTrack, point)) {
        SetEditorPage(VoiceEditorPage::EQ);
    } else if (Contains(m_Layout.fxTrack, point)) {
        SetEditorPage(VoiceEditorPage::FX);
    } else if (m_EditorPage == VoiceEditorPage::Noise &&
        Contains(m_Layout.noiseEnable, point)) {
        m_Preset.parameters.noiseEnabled = !m_Preset.parameters.noiseEnabled;
        ApplyParameters();
    } else if (m_EditorPage == VoiceEditorPage::Noise &&
        Contains(m_Layout.noiseBypass, point)) {
        m_Preset.parameters.noiseBypassed = !m_Preset.parameters.noiseBypassed;
        ApplyParameters();
    } else if (m_EditorPage == VoiceEditorPage::Noise &&
        Contains(m_Layout.resetNoise, point)) {
        m_Preset.parameters.noiseEnabled = false;
        m_Preset.parameters.noiseBypassed = false;
        m_Preset.parameters.noiseStrength = 1.0f;
        m_Preset.parameters.noiseWet = 1.0f;
        ApplyParameters();
        m_Message = Localized(U"降噪已重置", U"Noise reset");
    } else if (m_EditorPage == VoiceEditorPage::Noise &&
        Contains(m_Layout.noiseStrength, point)) {
        m_ActiveNoiseSlider = NoiseSlider::Strength;
        UpdateNoiseSlider(*m_ActiveNoiseSlider, point.x);
    } else if (m_EditorPage == VoiceEditorPage::Noise &&
        Contains(m_Layout.noiseWet, point)) {
        m_ActiveNoiseSlider = NoiseSlider::Wet;
        UpdateNoiseSlider(*m_ActiveNoiseSlider, point.x);
    } else if (m_EditorPage == VoiceEditorPage::EQ &&
        Contains(m_Layout.eqBypass, point)) {
        m_EQEditor.Model().SetEnabled(!m_EQEditor.Model().Enabled());
        m_EQDirty = true;
        PublishParametricEQ();
    } else if (m_EditorPage == VoiceEditorPage::EQ &&
        Contains(m_Layout.resetEq, point)) {
        m_EQEditor.Reset();
        PublishParametricEQ();
        m_Message = Localized(U"EQ 已重置", U"EQ reset");
    } else if (m_EditorPage == VoiceEditorPage::FX &&
        Contains(m_Layout.electronicEnable, point)) {
        m_Preset.parameters.electronicMist.enabled =
            !m_Preset.parameters.electronicMist.enabled;
        ApplyParameters();
    } else if (m_EditorPage == VoiceEditorPage::FX &&
        Contains(m_Layout.editNoiseCurve, point)) {
        SetEditorPage(VoiceEditorPage::Pitch);
        m_UnifiedEditor.SetActiveTrack(3);
    } else if (m_EditorPage == VoiceEditorPage::FX &&
        Contains(m_Layout.resetNoiseCurve, point)) {
        m_Preset.electronicNoiseCurve.Clear();
        (void)m_Preset.electronicNoiseCurve.AddPoint({1, 20.0f, 0.0f,
            Engine::Curve::CurveInterpolation::Smooth});
        (void)m_Preset.electronicNoiseCurve.AddPoint({2, 20'000.0f, 0.0f,
            Engine::Curve::CurveInterpolation::Smooth});
        m_CurvesDirty = true;
        PublishCurves();
        m_Message = Localized(U"电子噪声曲线已重置", U"Electronic noise curve reset");
    } else if (m_EditorPage == VoiceEditorPage::FX &&
        Contains(m_Layout.softEnable, point)) {
        m_Preset.parameters.softHaze.enabled = !m_Preset.parameters.softHaze.enabled;
        ApplyParameters();
    } else if (m_EditorPage == VoiceEditorPage::FX &&
        Contains(m_Layout.spatialEnable, point)) {
        m_Preset.parameters.spatialHaze.enabled = !m_Preset.parameters.spatialHaze.enabled;
        ApplyParameters();
    } else if (m_EditorPage == VoiceEditorPage::FX) {
        if (Contains(m_Layout.syntheticEnable, point)) {
            m_Preset.parameters.syntheticVoice.enabled =
                !m_Preset.parameters.syntheticVoice.enabled;
            ApplyParameters();
            return;
        }
        for (std::size_t index = 0; index < m_Layout.electronicSliders.size(); ++index) {
            if (Contains(m_Layout.electronicSliders[index], point)) {
                m_ActiveFXSlider = static_cast<FXSlider>(
                    static_cast<std::size_t>(FXSlider::MistAmount) + index);
                UpdateFXSlider(*m_ActiveFXSlider, point.x);
                return;
            }
        }
        for (std::size_t index = 0; index < m_Layout.syntheticSliders.size(); ++index) {
            if (Contains(m_Layout.syntheticSliders[index], point)) {
                m_ActiveFXSlider = static_cast<FXSlider>(
                    static_cast<std::size_t>(FXSlider::SyntheticAmount) + index);
                UpdateFXSlider(*m_ActiveFXSlider, point.x);
                return;
            }
        }
        for (std::size_t index = 0; index < m_Layout.softSliders.size(); ++index) {
            if (Contains(m_Layout.softSliders[index], point)) {
                m_ActiveFXSlider = static_cast<FXSlider>(
                    static_cast<std::size_t>(FXSlider::SoftAmount) + index);
                UpdateFXSlider(*m_ActiveFXSlider, point.x);
                return;
            }
        }
        for (std::size_t index = 0; index < m_Layout.spatialSliders.size(); ++index) {
            if (Contains(m_Layout.spatialSliders[index], point)) {
                m_ActiveFXSlider = static_cast<FXSlider>(
                    static_cast<std::size_t>(FXSlider::SpatialAmount) + index);
                UpdateFXSlider(*m_ActiveFXSlider, point.x);
                return;
            }
        }
    } else {
        for (std::size_t index = 0; index < m_Layout.faders.size(); ++index) {
            if (Contains(m_Layout.faders[index], point)) {
                m_ActiveSlider = static_cast<Slider>(index);
                UpdateSlider(*m_ActiveSlider, point.y);
                break;
            }
        }
    }
}

void VoiceMixerWindow::UpdateNoiseSlider(NoiseSlider slider, float x) {
    const RectF rect = slider == NoiseSlider::Strength
        ? m_Layout.noiseStrength : m_Layout.noiseWet;
    const float value = std::clamp((x - rect.x - 18.0f) /
        std::max(1.0f, rect.width - 36.0f), 0.0f, 1.0f);
    if (slider == NoiseSlider::Strength) m_Preset.parameters.noiseStrength = value;
    else m_Preset.parameters.noiseWet = value;
    ApplyParameters();
}

void VoiceMixerWindow::DrawNoiseSlider(
    Engine::DrawList2D& list,
    RectF rect,
    std::u32string_view label,
    float value) {
    list.AddRoundedRect({rect, 8.0f, kPanel});
    DrawText(list, std::u32string(label), rect.x + 14.0f, rect.y + 9.0f,
        9.0f, kMuted, 150.0f);
    DrawText(list, ToText(Fixed(value * 100.0f, 0) + "%"),
        rect.x + rect.width - 64.0f, rect.y + 8.0f, 11.0f,
        {0.88f, 0.94f, 1.0f, 1.0f}, 52.0f);
    const RectF rail{rect.x + 18.0f, rect.y + 38.0f, rect.width - 36.0f, 7.0f};
    list.AddRoundedRect({rail, 3.5f, {0.08f, 0.11f, 0.14f, 1.0f}});
    list.AddRoundedRect({{rail.x, rail.y, rail.width * value, rail.height}, 3.5f,
        {0.27f, 0.82f, 0.67f, 1.0f}});
    list.AddCircle({{rail.x + rail.width * value, rail.y + rail.height * 0.5f},
        7.0f, {0.88f, 0.94f, 0.98f, 1.0f}});
}

void VoiceMixerWindow::DrawNoisePage(Engine::DrawList2D& list) {
    list.AddRoundedRect({m_Layout.curve, 10.0f, {0.010f, 0.052f, 0.061f, 1.0f}});
    DrawButton(list, m_Layout.noiseEnable,
        Localized(U"启用降噪", U"Enable Noise"), true,
        m_Preset.parameters.noiseEnabled, true);
    DrawButton(list, m_Layout.noiseMode, U"RNNoise", false, true, true);
    DrawButton(list, m_Layout.noiseBypass,
        Localized(U"单独旁路", U"Bypass"), true,
        m_Preset.parameters.noiseBypassed, true);
    DrawButton(list, m_Layout.resetNoise,
        Localized(U"重置降噪", U"Reset Noise"), true, false, true);
    DrawNoiseSlider(list, m_Layout.noiseStrength,
        Localized(U"降噪强度", U"Strength"), m_Preset.parameters.noiseStrength);
    DrawNoiseSlider(list, m_Layout.noiseWet,
        Localized(U"降噪混合", U"Wet"), m_Preset.parameters.noiseWet);

    const auto& noise = m_Snapshot.voice.noise;
    const float top = m_Layout.noiseWet.y + m_Layout.noiseWet.height + 28.0f;
    const float width = (m_Layout.curve.width - 76.0f) / 3.0f;
    const float height = 76.0f;
    const std::array<std::pair<std::u32string, std::u32string>, 6> values{{
        {std::u32string(Localized(U"状态", U"STATUS")), NoiseStateText(noise.state)},
        {std::u32string(Localized(U"语音概率", U"VOICE PROBABILITY")), ToText(Fixed(noise.voiceProbability * 100.0f, 1) + "%")},
        {std::u32string(Localized(U"平均 / 峰值衰减", U"AVG / PEAK ATTENUATION")), ToText(Fixed(noise.averageAttenuationDb, 1) + " / " + Fixed(noise.peakAttenuationDb, 1) + " dB")},
        {std::u32string(Localized(U"算法耗时", U"PROCESS AVG / P99")), ToText(Fixed(static_cast<float>(noise.averageProcessMilliseconds), 3) + " / " + Fixed(static_cast<float>(noise.p99ProcessMilliseconds), 3) + " ms")},
        {std::u32string(Localized(U"新增延迟", U"ADDED LATENCY")), ToText(Fixed(static_cast<float>(noise.algorithmicLatencyMilliseconds), 1) + " ms")},
        {std::u32string(Localized(U"输入 / 输出电平", U"INPUT / OUTPUT LEVEL")), ToText(Fixed(m_Snapshot.inputPeak, 3) + " / " + Fixed(m_Snapshot.outputPeak, 3))}
    }};
    for (std::size_t index = 0; index < values.size(); ++index) {
        const float x = m_Layout.curve.x + 28.0f +
            static_cast<float>(index % 3) * (width + 10.0f);
        const float y = top + static_cast<float>(index / 3) * (height + 10.0f);
        list.AddRoundedRect({{x, y, width, height}, 7.0f, kPanelRaised});
        DrawText(list, values[index].first, x + 12.0f, y + 12.0f, 8.0f, kMuted, width - 20.0f);
        DrawText(list, values[index].second, x + 12.0f, y + 37.0f, 12.0f,
            {0.88f, 0.94f, 1.0f, 1.0f}, width - 20.0f);
    }
}

void VoiceMixerWindow::UpdateFXSlider(FXSlider slider, float x) {
    RectF rect{};
    switch (slider) {
    case FXSlider::MistAmount: rect = m_Layout.electronicSliders[0]; break;
    case FXSlider::MistFollow: rect = m_Layout.electronicSliders[1]; break;
    case FXSlider::MistSharpness: rect = m_Layout.electronicSliders[2]; break;
    case FXSlider::MistWidth: rect = m_Layout.electronicSliders[3]; break;
    case FXSlider::MistWet: rect = m_Layout.electronicSliders[4]; break;
    case FXSlider::SyntheticAmount: rect = m_Layout.syntheticSliders[0]; break;
    case FXSlider::SyntheticStability: rect = m_Layout.syntheticSliders[1]; break;
    case FXSlider::SyntheticElectronic: rect = m_Layout.syntheticSliders[2]; break;
    case FXSlider::SyntheticWidth: rect = m_Layout.syntheticSliders[3]; break;
    case FXSlider::SyntheticWet: rect = m_Layout.syntheticSliders[4]; break;
    case FXSlider::SoftAmount: rect = m_Layout.softSliders[0]; break;
    case FXSlider::SoftBlur: rect = m_Layout.softSliders[1]; break;
    case FXSlider::SoftDamping: rect = m_Layout.softSliders[2]; break;
    case FXSlider::SoftWet: rect = m_Layout.softSliders[3]; break;
    case FXSlider::SpatialAmount: rect = m_Layout.spatialSliders[0]; break;
    case FXSlider::SpatialSize: rect = m_Layout.spatialSliders[1]; break;
    case FXSlider::SpatialDiffusion: rect = m_Layout.spatialSliders[2]; break;
    case FXSlider::SpatialDamping: rect = m_Layout.spatialSliders[3]; break;
    case FXSlider::SpatialWidth: rect = m_Layout.spatialSliders[4]; break;
    case FXSlider::SpatialWet: rect = m_Layout.spatialSliders[5]; break;
    case FXSlider::Count: return;
    }
    const float value = std::clamp((x - rect.x - 84.0f) /
        std::max(1.0f, rect.width - 104.0f), 0.0f, 1.0f);
    switch (slider) {
    case FXSlider::MistAmount: m_Preset.parameters.electronicMist.amount = value; break;
    case FXSlider::MistFollow: m_Preset.parameters.electronicMist.follow = value; break;
    case FXSlider::MistSharpness: m_Preset.parameters.electronicMist.sharpness = value; break;
    case FXSlider::MistWidth: m_Preset.parameters.electronicMist.width = value; break;
    case FXSlider::MistWet: m_Preset.parameters.electronicMist.wet = value; break;
    case FXSlider::SyntheticAmount: m_Preset.parameters.syntheticVoice.amount = value; break;
    case FXSlider::SyntheticStability: m_Preset.parameters.syntheticVoice.stability = value; break;
    case FXSlider::SyntheticElectronic: m_Preset.parameters.syntheticVoice.electronic = value; break;
    case FXSlider::SyntheticWidth: m_Preset.parameters.syntheticVoice.width = value; break;
    case FXSlider::SyntheticWet: m_Preset.parameters.syntheticVoice.wet = value; break;
    case FXSlider::SoftAmount: m_Preset.parameters.softHaze.amount = value; break;
    case FXSlider::SoftBlur: m_Preset.parameters.softHaze.blur = value; break;
    case FXSlider::SoftDamping: m_Preset.parameters.softHaze.damping = value; break;
    case FXSlider::SoftWet: m_Preset.parameters.softHaze.wet = value; break;
    case FXSlider::SpatialAmount: m_Preset.parameters.spatialHaze.amount = value; break;
    case FXSlider::SpatialSize: m_Preset.parameters.spatialHaze.size = value; break;
    case FXSlider::SpatialDiffusion: m_Preset.parameters.spatialHaze.diffusion = value; break;
    case FXSlider::SpatialDamping: m_Preset.parameters.spatialHaze.damping = value; break;
    case FXSlider::SpatialWidth: m_Preset.parameters.spatialHaze.width = value; break;
    case FXSlider::SpatialWet: m_Preset.parameters.spatialHaze.wet = value; break;
    case FXSlider::Count: return;
    }
    ApplyParameters();
}

void VoiceMixerWindow::AdjustFXSlider(FXSlider slider, float delta) {
    auto adjust = [delta](float& value) {
        value = std::clamp(value + delta * 0.02f, 0.0f, 1.0f);
    };
    switch (slider) {
    case FXSlider::MistAmount: adjust(m_Preset.parameters.electronicMist.amount); break;
    case FXSlider::MistFollow: adjust(m_Preset.parameters.electronicMist.follow); break;
    case FXSlider::MistSharpness: adjust(m_Preset.parameters.electronicMist.sharpness); break;
    case FXSlider::MistWidth: adjust(m_Preset.parameters.electronicMist.width); break;
    case FXSlider::MistWet: adjust(m_Preset.parameters.electronicMist.wet); break;
    case FXSlider::SyntheticAmount: adjust(m_Preset.parameters.syntheticVoice.amount); break;
    case FXSlider::SyntheticStability: adjust(m_Preset.parameters.syntheticVoice.stability); break;
    case FXSlider::SyntheticElectronic: adjust(m_Preset.parameters.syntheticVoice.electronic); break;
    case FXSlider::SyntheticWidth: adjust(m_Preset.parameters.syntheticVoice.width); break;
    case FXSlider::SyntheticWet: adjust(m_Preset.parameters.syntheticVoice.wet); break;
    case FXSlider::SoftAmount: adjust(m_Preset.parameters.softHaze.amount); break;
    case FXSlider::SoftBlur: adjust(m_Preset.parameters.softHaze.blur); break;
    case FXSlider::SoftDamping: adjust(m_Preset.parameters.softHaze.damping); break;
    case FXSlider::SoftWet: adjust(m_Preset.parameters.softHaze.wet); break;
    case FXSlider::SpatialAmount: adjust(m_Preset.parameters.spatialHaze.amount); break;
    case FXSlider::SpatialSize: adjust(m_Preset.parameters.spatialHaze.size); break;
    case FXSlider::SpatialDiffusion: adjust(m_Preset.parameters.spatialHaze.diffusion); break;
    case FXSlider::SpatialDamping: adjust(m_Preset.parameters.spatialHaze.damping); break;
    case FXSlider::SpatialWidth: adjust(m_Preset.parameters.spatialHaze.width); break;
    case FXSlider::SpatialWet: adjust(m_Preset.parameters.spatialHaze.wet); break;
    case FXSlider::Count: return;
    }
    ApplyParameters();
}

void VoiceMixerWindow::DrawFXSlider(
    Engine::DrawList2D& list,
    RectF rect,
    std::u32string_view label,
    float value) {
    const bool compact = rect.height < 24.0f;
    const float labelY = rect.y + (compact ? 1.0f : 4.0f);
    DrawText(list, std::u32string(label), rect.x, labelY, compact ? 7.5f : 9.0f,
        kMuted, 78.0f);
    DrawText(list, ToText(Fixed(value * 100.0f, 0) + "%"),
        rect.x + rect.width - 42.0f, labelY, compact ? 7.5f : 9.0f,
        {0.88f, 0.94f, 1.0f, 1.0f}, 42.0f);
    const RectF rail{rect.x + 84.0f, rect.y + (compact ? 3.0f : 14.0f),
        rect.width - 104.0f, compact ? 5.0f : 7.0f};
    list.AddRoundedRect({rail, 3.5f, {0.08f, 0.11f, 0.14f, 1.0f}});
    list.AddRoundedRect({{rail.x, rail.y, rail.width * value, rail.height}, 3.5f,
        {0.86f, 0.54f, 0.28f, 1.0f}});
    list.AddCircle({{rail.x + rail.width * value, rail.y + rail.height * 0.5f},
        compact ? 4.5f : 7.0f, {0.90f, 0.94f, 0.98f, 1.0f}});
}

void VoiceMixerWindow::DrawFXPage(Engine::DrawList2D& list) {
    list.AddRoundedRect({m_Layout.electronicCard, 10.0f, {0.038f, 0.048f, 0.070f, 1.0f}});
    list.AddRoundedRect({m_Layout.syntheticCard, 10.0f, {0.022f, 0.052f, 0.070f, 1.0f}});
    list.AddRoundedRect({m_Layout.softCard, 10.0f, {0.022f, 0.052f, 0.070f, 1.0f}});
    list.AddRoundedRect({m_Layout.spatialCard, 10.0f, {0.022f, 0.052f, 0.070f, 1.0f}});

    DrawText(list, U"Electronic Mist", m_Layout.electronicCard.x + 12.0f,
        m_Layout.electronicCard.y + 10.0f, 13.0f, {0.96f, 0.66f, 0.34f, 1.0f}, 190.0f);
    DrawText(list, std::u32string(Localized(U"跟随人声的电子颗粒与频谱尘埃", U"Voice-following spectral dust")),
        m_Layout.electronicCard.x + 12.0f, m_Layout.electronicCard.y + 27.0f,
        7.5f, kMuted, m_Layout.electronicCard.width - 24.0f);
    DrawButton(list, m_Layout.electronicEnable, Localized(U"启用", U"Enable"), true,
        m_Preset.parameters.electronicMist.enabled, true);
    DrawButton(list, m_Layout.editNoiseCurve, Localized(U"编辑噪声曲线", U"Edit Noise Curve"), true,
        false, true);
    DrawButton(list, m_Layout.resetNoiseCurve, Localized(U"重置曲线", U"Reset Curve"), true,
        false, true);
    const std::array<std::pair<std::u32string_view, float>, 5> mistValues{{
        {Localized(U"强度", U"Amount"), m_Preset.parameters.electronicMist.amount},
        {Localized(U"跟随", U"Follow"), m_Preset.parameters.electronicMist.follow},
        {Localized(U"锐度", U"Sharpness"), m_Preset.parameters.electronicMist.sharpness},
        {Localized(U"宽度", U"Width"), m_Preset.parameters.electronicMist.width},
        {Localized(U"混合", U"Wet"), m_Preset.parameters.electronicMist.wet}}};
    for (std::size_t index = 0; index < mistValues.size(); ++index) {
        DrawFXSlider(list, m_Layout.electronicSliders[index], mistValues[index].first,
            mistValues[index].second);
    }

    DrawText(list, U"Synthetic Voice", m_Layout.syntheticCard.x + 12.0f,
        m_Layout.syntheticCard.y + 10.0f, 13.0f, {0.62f, 0.82f, 1.0f, 1.0f}, 190.0f);
    DrawText(list, std::u32string(Localized(U"轻量音高稳定与微调", U"Pitch stability and micro pitch")),
        m_Layout.syntheticCard.x + 12.0f, m_Layout.syntheticCard.y + 27.0f,
        7.5f, kMuted, m_Layout.syntheticCard.width - 24.0f);
    DrawButton(list, m_Layout.syntheticEnable, Localized(U"启用", U"Enable"), true,
        m_Preset.parameters.syntheticVoice.enabled, true);
    const std::array<std::pair<std::u32string_view, float>, 5> syntheticValuesNew{{
        {Localized(U"强度", U"Amount"), m_Preset.parameters.syntheticVoice.amount},
        {Localized(U"稳定", U"Stability"), m_Preset.parameters.syntheticVoice.stability},
        {Localized(U"电子", U"Electronic"), m_Preset.parameters.syntheticVoice.electronic},
        {Localized(U"宽度", U"Width"), m_Preset.parameters.syntheticVoice.width},
        {Localized(U"混合", U"Wet"), m_Preset.parameters.syntheticVoice.wet}}};
    for (std::size_t index = 0; index < syntheticValuesNew.size(); ++index) {
        DrawFXSlider(list, m_Layout.syntheticSliders[index], syntheticValuesNew[index].first,
            syntheticValuesNew[index].second);
    }

    DrawText(list, U"Soft Haze", m_Layout.softCard.x + 12.0f,
        m_Layout.softCard.y + 10.0f, 13.0f, {0.92f, 0.76f, 0.58f, 1.0f}, 220.0f);
    DrawText(list, std::u32string(Localized(U"近场柔化与微扩散", U"Near-field softening")),
        m_Layout.softCard.x + 12.0f, m_Layout.softCard.y + 27.0f, 7.5f, kMuted,
        m_Layout.softCard.width - 24.0f);
    DrawButton(list, m_Layout.softEnable, Localized(U"启用", U"Enable"), true,
        m_Preset.parameters.softHaze.enabled, true);
    const std::array<std::pair<std::u32string_view, float>, 4> softValuesNew{{
        {Localized(U"强度", U"Amount"), m_Preset.parameters.softHaze.amount},
        {Localized(U"模糊", U"Blur"), m_Preset.parameters.softHaze.blur},
        {Localized(U"阻尼", U"Damping"), m_Preset.parameters.softHaze.damping},
        {Localized(U"混合", U"Wet"), m_Preset.parameters.softHaze.wet}}};
    for (std::size_t index = 0; index < softValuesNew.size(); ++index) {
        DrawFXSlider(list, m_Layout.softSliders[index], softValuesNew[index].first,
            softValuesNew[index].second);
    }

    DrawText(list, U"Spatial Haze", m_Layout.spatialCard.x + 12.0f,
        m_Layout.spatialCard.y + 10.0f, 13.0f, {0.92f, 0.76f, 0.58f, 1.0f}, 240.0f);
    DrawText(list, std::u32string(Localized(U"短空间扩散与立体空气感", U"Short diffusion and stereo air")),
        m_Layout.spatialCard.x + 12.0f, m_Layout.spatialCard.y + 27.0f, 7.5f, kMuted,
        m_Layout.spatialCard.width - 24.0f);
    DrawButton(list, m_Layout.spatialEnable, Localized(U"启用", U"Enable"), true,
        m_Preset.parameters.spatialHaze.enabled, true);
    const std::array<std::pair<std::u32string_view, float>, 6> spatialValuesNew{{
        {Localized(U"强度", U"Amount"), m_Preset.parameters.spatialHaze.amount},
        {Localized(U"尺寸", U"Size"), m_Preset.parameters.spatialHaze.size},
        {Localized(U"扩散", U"Diffusion"), m_Preset.parameters.spatialHaze.diffusion},
        {Localized(U"阻尼", U"Damping"), m_Preset.parameters.spatialHaze.damping},
        {Localized(U"宽度", U"Width"), m_Preset.parameters.spatialHaze.width},
        {Localized(U"混合", U"Wet"), m_Preset.parameters.spatialHaze.wet}}};
    for (std::size_t index = 0; index < spatialValuesNew.size(); ++index) {
        DrawFXSlider(list, m_Layout.spatialSliders[index], spatialValuesNew[index].first,
            spatialValuesNew[index].second);
    }
    return;
    list.AddRoundedRect({m_Layout.syntheticCard, 10.0f, {0.022f, 0.052f, 0.070f, 1.0f}});
    list.AddRoundedRect({m_Layout.softCard, 10.0f, {0.022f, 0.052f, 0.070f, 1.0f}});
    list.AddRoundedRect({m_Layout.spatialCard, 10.0f, {0.022f, 0.052f, 0.070f, 1.0f}});
    DrawText(list, U"Synthetic Voice", m_Layout.syntheticCard.x + 16.0f,
        m_Layout.syntheticCard.y + 14.0f, 14.0f, {0.62f, 0.82f, 1.0f, 1.0f}, 190.0f);
    DrawText(list, std::u32string(Localized(U"轻合成人声稳定与微调", U"Low-intensity pitch stability and micro pitch")),
        m_Layout.syntheticCard.x + 16.0f, m_Layout.syntheticCard.y + 33.0f,
        8.0f, kMuted, m_Layout.syntheticCard.width - 32.0f);
    DrawButton(list, m_Layout.syntheticEnable, Localized(U"鍚敤", U"Enable"), true,
        m_Preset.parameters.syntheticVoice.enabled, true);
    const std::array<std::pair<std::u32string_view, float>, 5> syntheticValues{{
        {Localized(U"强度", U"Amount"), m_Preset.parameters.syntheticVoice.amount},
        {Localized(U"稳定", U"Stability"), m_Preset.parameters.syntheticVoice.stability},
        {Localized(U"电子", U"Electronic"), m_Preset.parameters.syntheticVoice.electronic},
        {Localized(U"宽度", U"Width"), m_Preset.parameters.syntheticVoice.width},
        {Localized(U"混合", U"Wet"), m_Preset.parameters.syntheticVoice.wet}}};
    for (std::size_t index = 0; index < syntheticValues.size(); ++index) {
        DrawFXSlider(list, m_Layout.syntheticSliders[index], syntheticValues[index].first,
            syntheticValues[index].second);
    }
    DrawText(list, U"Soft Haze", m_Layout.softCard.x + 16.0f,
        m_Layout.softCard.y + 14.0f, 15.0f, {0.92f, 0.76f, 0.58f, 1.0f}, 220.0f);
    DrawText(list, std::u32string(Localized(U"近距离柔化与微扩散", U"Near-field softening and micro diffusion")),
        m_Layout.softCard.x + 16.0f, m_Layout.softCard.y + 33.0f, 8.0f, kMuted,
        m_Layout.softCard.width - 32.0f);
    DrawButton(list, m_Layout.softEnable, Localized(U"启用", U"Enable"), true,
        m_Preset.parameters.softHaze.enabled, true);
    const std::array<std::pair<std::u32string_view, float>, 4> softValues{{
        {Localized(U"强度", U"Amount"), m_Preset.parameters.softHaze.amount},
        {Localized(U"模糊", U"Blur"), m_Preset.parameters.softHaze.blur},
        {Localized(U"阻尼", U"Damping"), m_Preset.parameters.softHaze.damping},
        {Localized(U"混合", U"Wet"), m_Preset.parameters.softHaze.wet}}};
    for (std::size_t index = 0; index < softValues.size(); ++index) {
        DrawFXSlider(list, m_Layout.softSliders[index], softValues[index].first,
            softValues[index].second);
    }

    DrawText(list, U"Spatial Haze", m_Layout.spatialCard.x + 16.0f,
        m_Layout.spatialCard.y + 14.0f, 15.0f, {0.92f, 0.76f, 0.58f, 1.0f}, 240.0f);
    DrawText(list, std::u32string(Localized(U"短空间扩散与立体空气感", U"Short diffusion and stereo air")),
        m_Layout.spatialCard.x + 16.0f, m_Layout.spatialCard.y + 33.0f, 8.0f, kMuted,
        m_Layout.spatialCard.width - 32.0f);
    DrawButton(list, m_Layout.spatialEnable, Localized(U"启用", U"Enable"), true,
        m_Preset.parameters.spatialHaze.enabled, true);
    const std::array<std::pair<std::u32string_view, float>, 6> spatialValues{{
        {Localized(U"强度", U"Amount"), m_Preset.parameters.spatialHaze.amount},
        {Localized(U"尺寸", U"Size"), m_Preset.parameters.spatialHaze.size},
        {Localized(U"扩散", U"Diffusion"), m_Preset.parameters.spatialHaze.diffusion},
        {Localized(U"阻尼", U"Damping"), m_Preset.parameters.spatialHaze.damping},
        {Localized(U"宽度", U"Width"), m_Preset.parameters.spatialHaze.width},
        {Localized(U"混合", U"Wet"), m_Preset.parameters.spatialHaze.wet}}};
    for (std::size_t index = 0; index < spatialValues.size(); ++index) {
        DrawFXSlider(list, m_Layout.spatialSliders[index], spatialValues[index].first,
            spatialValues[index].second);
    }
}

Engine::RectF VoiceMixerWindow::FaderRect(Slider slider) const noexcept {
    return m_Layout.faders[static_cast<std::size_t>(slider)];
}

void VoiceMixerWindow::UpdateSlider(Slider slider, float y) {
    const RectF rect = FaderRect(slider);
    const float top = rect.y + 70.0f;
    const float bottom = rect.y + rect.height - 48.0f;
    const float t = std::clamp((bottom - y) / std::max(1.0f, bottom - top), 0.0f, 1.0f);
    float minimum = 0.0f;
    float maximum = 1.0f;
    float* value = nullptr;
    switch (slider) {
    case Slider::Wet:
        value = &m_Preset.parameters.wet; break;
    case Slider::Pitch:
        minimum = -12.0f; maximum = 12.0f; value = &m_Preset.parameters.pitchSemitones; break;
    case Slider::Formant:
        minimum = -6.0f; maximum = 6.0f; value = &m_Preset.parameters.formantSemitones; break;
    case Slider::Count:
        return;
    }
    *value = std::lerp(minimum, maximum, t);
    ApplyParameters();
}

void VoiceMixerWindow::AdjustSlider(Slider slider, float delta) {
    float step = 0.02f;
    switch (slider) {
    case Slider::Pitch:
    case Slider::Formant:
        step = 0.25f; break;
    default:
        break;
    }
    float* value = nullptr;
    float minimum = 0.0f;
    float maximum = 1.0f;
    switch (slider) {
    case Slider::Wet: value = &m_Preset.parameters.wet; break;
    case Slider::Pitch: value = &m_Preset.parameters.pitchSemitones; minimum = -12.0f; maximum = 12.0f; break;
    case Slider::Formant: value = &m_Preset.parameters.formantSemitones; minimum = -6.0f; maximum = 6.0f; break;
    case Slider::Count: return;
    }
    *value = std::clamp(*value + delta * step, minimum, maximum);
    ApplyParameters();
}

void VoiceMixerWindow::DrawButton(
    Engine::DrawList2D& list,
    RectF rect,
    std::u32string_view text,
    bool enabled,
    bool active,
    bool compact) {
    const Color4f color = !enabled
        ? Color4f{0.070f, 0.080f, 0.100f, 1.0f}
        : active ? Color4f{0.12f, 0.50f, 0.34f, 1.0f}
                 : (m_HoverValid && Contains(rect, m_HoverPoint)
                     ? Color4f{0.075f, 0.18f, 0.24f, 1.0f}
                     : kPanelRaised);
    list.AddRoundedRect({rect, compact ? 5.0f : 7.0f, color});
    DrawText(list, std::u32string(text), rect.x + (compact ? 10.0f : 12.0f),
        rect.y + (compact ? 7.0f : 9.0f), compact ? 11.0f : 13.0f,
        enabled ? Color4f{0.90f, 0.93f, 0.98f, 1.0f} : kMuted,
        rect.width - 16.0f);
}

void VoiceMixerWindow::DrawDeviceSelect(
    Engine::DrawList2D& list,
    RectF rect,
    std::u32string_view label,
    std::u32string value,
    bool enabled,
    bool open) {
    list.AddRoundedRect({rect, 7.0f, open ? Color4f{0.10f, 0.16f, 0.23f, 1.0f} : kPanelRaised});
    DrawText(list, std::u32string(label), rect.x + 12.0f, rect.y + 7.0f,
        9.0f, enabled ? kMuted : Color4f{0.34f, 0.37f, 0.43f, 1.0f}, 70.0f);
    DrawText(list, std::move(value), rect.x + 78.0f, rect.y + 11.0f,
        12.0f, enabled ? Color4f{0.92f, 0.95f, 0.99f, 1.0f} : kMuted,
        rect.width - 112.0f);
    DrawText(list, open ? U"^" : U"v", rect.x + rect.width - 24.0f,
        rect.y + 10.0f, 12.0f, kMuted, 16.0f);
}

RectF VoiceMixerWindow::DropdownItemRect(std::size_t index) const noexcept {
    const RectF anchor = m_OpenDropdown == Dropdown::Input
        ? m_Layout.inputDevice
        : m_Layout.outputDevice;
    return {anchor.x, anchor.y + anchor.height + static_cast<float>(index) * 34.0f,
        anchor.width, 34.0f};
}

std::size_t VoiceMixerWindow::DropdownVisibleCount(std::size_t itemCount) const noexcept {
    const RectF anchor = m_OpenDropdown == Dropdown::Input
        ? m_Layout.inputDevice
        : m_Layout.outputDevice;
    const float available = std::max(34.0f,
        m_Height - (anchor.y + anchor.height) - 12.0f);
    return std::min(itemCount,
        static_cast<std::size_t>(std::floor(available / 34.0f)));
}

void VoiceMixerWindow::DrawDropdown(Engine::DrawList2D& list) {
    if (m_OpenDropdown == Dropdown::None) return;
    const auto& devices = m_OpenDropdown == Dropdown::Input
        ? m_Audio.InputDevices()
        : m_Audio.OutputDevices();
    const std::size_t selected = m_OpenDropdown == Dropdown::Input
        ? m_Audio.SelectedInput()
        : m_Audio.SelectedOutput();
    for (std::size_t index = 0; index < DropdownVisibleCount(devices.size()); ++index) {
        const RectF rect = DropdownItemRect(index);
        list.AddSolidRect({rect, index == selected
            ? Color4f{0.12f, 0.30f, 0.43f, 1.0f}
            : Color4f{0.075f, 0.09f, 0.12f, 1.0f}});
        DrawText(list, ToText(devices[index].name), rect.x + 12.0f, rect.y + 8.0f,
            11.0f, {0.91f, 0.94f, 0.98f, 1.0f}, rect.width - 22.0f);
    }
}

void VoiceMixerWindow::DrawFader(
    Engine::DrawList2D& list,
    Slider slider,
    std::u32string_view label,
    float value,
    float minimum,
    float maximum,
    std::u32string_view unit) {
    const RectF rect = FaderRect(slider);
    list.AddRoundedRect({rect, 8.0f, kPanel});
    DrawText(list, std::u32string(label), rect.x + 7.0f, rect.y + 10.0f,
        9.0f, kMuted, rect.width - 10.0f);
    DrawText(list, ToText(Fixed(value, slider == Slider::Wet ? 2 : 1)) + std::u32string(unit),
        rect.x + 7.0f, rect.y + 30.0f, 13.0f,
        {0.91f, 0.94f, 0.99f, 1.0f}, rect.width - 10.0f);

    const float center = rect.x + rect.width * 0.5f;
    const float top = rect.y + 70.0f;
    const float bottom = rect.y + rect.height - 48.0f;
    const float t = std::clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f);
    const float thumbY = std::lerp(bottom, top, t);
    for (int tick = 0; tick <= 10; ++tick) {
        const float tickY = std::lerp(bottom, top, static_cast<float>(tick) / 10.0f);
        const float length = tick % 5 == 0 ? 12.0f : 7.0f;
        list.AddLine({{center - 18.0f, tickY}, {center - 18.0f + length, tickY},
            1.0f, {0.34f, 0.38f, 0.46f, 0.9f}});
        list.AddLine({{center + 18.0f - length, tickY}, {center + 18.0f, tickY},
            1.0f, {0.34f, 0.38f, 0.46f, 0.9f}});
    }
    list.AddRoundedRect({{center - 3.0f, top, 6.0f, bottom - top}, 3.0f,
        {0.11f, 0.13f, 0.17f, 1.0f}});
    const Color4f accent = slider == Slider::Formant ? kFormant
        : slider == Slider::Pitch ? kPitch
        : Color4f{0.30f, 0.67f, 1.0f, 1.0f};
    list.AddRoundedRect({{center - 2.0f, thumbY, 4.0f, bottom - thumbY}, 2.0f, accent});
    list.AddRoundedRect({{center - 15.0f, thumbY - 7.0f, 30.0f, 14.0f}, 4.0f,
        {0.84f, 0.88f, 0.94f, 1.0f}});
    list.AddLine({{center - 10.0f, thumbY}, {center + 10.0f, thumbY}, 1.0f,
        {0.24f, 0.28f, 0.34f, 1.0f}});
    DrawText(list, ToText(Fixed(minimum, 0)), rect.x + 7.0f, bottom + 15.0f,
        8.0f, kMuted, rect.width - 8.0f);
    DrawText(list, ToText(Fixed(maximum, 0)), rect.x + 7.0f, top - 15.0f,
        8.0f, kMuted, rect.width - 8.0f);
}

void VoiceMixerWindow::DrawMeter(
    Engine::DrawList2D& list,
    RectF rect,
    float value,
    Color4f color) {
    const RectF rail = rect;
    list.AddRoundedRect({rail, rail.height * 0.5f, {0.085f, 0.10f, 0.13f, 1.0f}});
    list.AddRoundedRect({{rail.x, rail.y,
        rail.width * std::clamp(value, 0.0f, 1.0f), rail.height},
        rail.height * 0.5f, color});
}

void VoiceMixerWindow::DrawDiagnostics(Engine::DrawList2D& list) {
    const RectF panel = m_Layout.diagnostics;
    list.AddRoundedRect({panel, 9.0f, kPanel});
    const float latency = static_cast<float>(
        m_Snapshot.diagnostics.latency.TotalEstimatedMilliseconds());
    const auto& noise = m_Snapshot.voice.noise;
    const auto& synthetic = m_Snapshot.voice.syntheticVoice;
    const auto& mist = m_Snapshot.voice.electronicMist;
    const std::array<std::pair<std::u32string, std::u32string>, 36> values{{
        {std::u32string(Localized(U"总延迟", U"TOTAL LATENCY")), ToText(Fixed(latency, 1) + " ms")},
        {std::u32string(Localized(U"回调 / 最大", U"CALLBACK / MAX")), ToText(Fixed(static_cast<float>(m_Snapshot.diagnostics.callbackMilliseconds), 2) + " / " + Fixed(static_cast<float>(m_Snapshot.diagnostics.maxCallbackMilliseconds), 2))},
        {std::u32string(Localized(U"实时负载", U"DEADLINE")), ToText(Fixed(static_cast<float>(m_Snapshot.deadlinePercent), 1) + "%")},
        {std::u32string(Localized(U"过载 / 欠载", U"XRUN / UNDER")), ToText(std::to_string(m_Snapshot.diagnostics.xRuns) + " / " + std::to_string(m_Snapshot.underruns))},
        {std::u32string(Localized(U"基频 / 置信度", U"F0 / CONFIDENCE")), ToText(Fixed(m_Snapshot.voice.state.pitch.frequencyHz, 1) + " / " + Fixed(m_Snapshot.voice.state.pitch.confidence, 2))},
        {std::u32string(Localized(U"缓冲 / 实时分配", U"BUFFER / RT ALLOC")), ToText(std::to_string(m_Snapshot.format.framesPerBuffer) + " / " + std::to_string(m_Snapshot.rtAllocationsObserved))},
        {std::u32string(Localized(U"分析线程", U"ANALYZER WORKER")), ToText(Fixed(static_cast<float>(m_Snapshot.voice.analyzer.workerMilliseconds), 2) + " ms")},
        {std::u32string(Localized(U"频谱 FFT / 版本", U"SPECTRUM FFT / REV")), ToText(std::to_string(m_Snapshot.voice.analyzer.fftSize) + " / " + std::to_string(m_Snapshot.voice.analyzer.revision))},
        {std::u32string(Localized(U"EQ 频段 / 耗时", U"EQ BANDS / TIME")), ToText(std::to_string(m_Snapshot.voice.eq.bandCount) + " / " + Fixed(static_cast<float>(m_Snapshot.voice.eq.processMilliseconds), 3))},
        {std::u32string(Localized(U"降噪模式 / 状态", U"NOISE MODE / STATE")), ToText(Engine::Audio::ToString(noise.mode)) + U" / " + NoiseStateText(noise.state)},
        {std::u32string(Localized(U"降噪启用 / 旁路", U"NOISE ENABLE / BYPASS")), ToText(std::string(noise.enabled ? "ON" : "OFF") + " / " + (noise.bypassed ? "ON" : "OFF"))},
        {std::u32string(Localized(U"强度 / 混合", U"STRENGTH / WET")), ToText(Fixed(noise.strength, 2) + " / " + Fixed(noise.wet, 2))},
        {std::u32string(Localized(U"语音概率 / 衰减", U"VOICE PROB / ATTEN")), ToText(Fixed(noise.voiceProbability, 2) + " / " + Fixed(noise.averageAttenuationDb, 1) + " dB")},
        {std::u32string(Localized(U"帧 / 输入输出缓冲", U"FRAME / IN-OUT BUFFER")), ToText(std::to_string(noise.frameSize) + " / " + std::to_string(noise.bufferedInputFrames) + "-" + std::to_string(noise.bufferedOutputFrames))},
        {std::u32string(Localized(U"降噪新增延迟", U"NOISE LATENCY")), ToText(Fixed(static_cast<float>(noise.algorithmicLatencyMilliseconds), 1) + " ms")},
        {std::u32string(Localized(U"降噪平均 / P95", U"NOISE AVG / P95")), ToText(Fixed(static_cast<float>(noise.averageProcessMilliseconds), 3) + " / " + Fixed(static_cast<float>(noise.p95ProcessMilliseconds), 3))},
        {std::u32string(Localized(U"降噪 P99 / 最大", U"NOISE P99 / MAX")), ToText(Fixed(static_cast<float>(noise.p99ProcessMilliseconds), 3) + " / " + Fixed(static_cast<float>(noise.maximumProcessMilliseconds), 3))},
        {std::u32string(Localized(U"降噪丢帧 / RT 分配", U"NOISE DROP / RT ALLOC")), ToText(std::to_string(noise.droppedFrames) + " / " + std::to_string(noise.rtAllocations))},
        {std::u32string(Localized(U"柔雾状态 / 耗时", U"SOFT HAZE / TIME")), ToText(std::string(m_Snapshot.voice.softHaze.enabled ? "ON" : "OFF") + " / " + Fixed(static_cast<float>(m_Snapshot.voice.softHaze.processMilliseconds), 3) + " ms")},
        {std::u32string(Localized(U"柔雾延迟 / 快照", U"SOFT DELAY / SNAPSHOT")), ToText(Fixed(m_Snapshot.voice.softHaze.effectiveDelayMilliseconds, 2) + " ms / " + std::to_string(m_Snapshot.voice.softHaze.snapshotRevision))},
        {std::u32string(Localized(U"空间雾状态 / 耗时", U"SPATIAL HAZE / TIME")), ToText(std::string(m_Snapshot.voice.spatialHaze.enabled ? "ON" : "OFF") + " / " + Fixed(static_cast<float>(m_Snapshot.voice.spatialHaze.processMilliseconds), 3) + " ms")},
        {std::u32string(Localized(U"空间尾长 / 宽度", U"SPATIAL TAIL / WIDTH")), ToText(Fixed(m_Snapshot.voice.spatialHaze.effectiveTailMilliseconds, 1) + " ms / " + Fixed(m_Snapshot.voice.spatialHaze.width, 2))},
        {std::u32string(Localized(U"柔雾 / 空间快照", U"SOFT / SPATIAL REV")), ToText(std::to_string(m_Snapshot.voice.softHaze.snapshotRevision) + " / " + std::to_string(m_Snapshot.voice.spatialHaze.snapshotRevision))},
        {std::u32string(Localized(U"柔雾 / 空间内存", U"SOFT / SPATIAL MEMORY")), ToText(std::to_string(m_Snapshot.voice.softHaze.delayMemoryBytes) + " / " + std::to_string(m_Snapshot.voice.spatialHaze.delayMemoryBytes))}
        , {U"SYNTHETIC ENABLE / REV", ToText(std::string(synthetic.enabled ? "ON" : "OFF") + " / " + std::to_string(synthetic.snapshotRevision))},
        {U"RAW / STABILIZED F0", ToText(Fixed(synthetic.rawFrequencyHz, 1) + " / " + Fixed(synthetic.stabilizedFrequencyHz, 1) + " Hz")},
        {U"STABILIZER CENTS / CONFIDENCE", ToText(Fixed(synthetic.correctionCents, 1) + " / " + Fixed(synthetic.confidence, 2))},
        {U"MICRO DETUNE / DELAY", ToText(Fixed(synthetic.micro.detuneCents, 1) + " ct / " + std::to_string(synthetic.micro.delaySamples) + " smp")},
        {U"SYNTH PROCESS / LATENCY", ToText(Fixed(static_cast<float>(synthetic.processMilliseconds), 3) + " ms / " + std::to_string(synthetic.algorithmicLatencyFrames))},
        {U"CONTROL SMOOTH / MICRO ACTIVE", ToText(Fixed(synthetic.controlSmoothingMilliseconds, 1) + " ms / " + (synthetic.micro.enabled ? "ON" : "OFF"))},
        {U"MIST ENABLE / REV", ToText(std::string(mist.enabled ? "ON" : "OFF") + " / " + std::to_string(mist.snapshotRevision))},
        {U"MIST AMOUNT / FOLLOW", ToText(Fixed(mist.amount, 2) + " / " + Fixed(mist.follow, 2))},
        {U"MIST SHARP / WIDTH", ToText(Fixed(mist.sharpness, 2) + " / " + Fixed(mist.width, 2))},
        {U"MIST BANDS / ACTIVE", ToText(std::to_string(mist.bandCount) + " / " + std::to_string(mist.activeBandCount))},
        {U"MIST VOICE / GATE", ToText(Fixed(mist.broadbandVoiceEnvelope, 3) + " / " + Fixed(mist.voiceGateActivity, 3))},
        {U"MIST RMS / VOICE RATIO", ToText(Fixed(mist.mistRms, 5) + " / " + Fixed(mist.mistToVoiceDb, 1) + " dB")}
    }};
    constexpr std::size_t columns = 6;
    const float cellWidth = panel.width / static_cast<float>(columns);
    const float rowHeight = panel.height / 6.0f;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::size_t column = index % columns;
        const std::size_t row = index / columns;
        const float x = panel.x + cellWidth * static_cast<float>(column) + 12.0f;
        const float y = panel.y + rowHeight * static_cast<float>(row);
        if (column > 0) {
            const float separator = panel.x + cellWidth * static_cast<float>(column);
            list.AddLine({{separator, y + 7.0f}, {separator, y + rowHeight - 7.0f},
                1.0f, {0.15f, 0.18f, 0.23f, 0.8f}});
        }
        if (row > 0 && column == 0) {
            list.AddLine({{panel.x + 8.0f, y}, {panel.x + panel.width - 8.0f, y},
                1.0f, {0.12f, 0.19f, 0.23f, 0.7f}});
        }
        DrawText(list, values[index].first, x, y + 7.0f, 7.5f, kMuted,
            cellWidth - 24.0f);
        const float valueSize = index == 0 ? 17.0f : 11.5f;
        const Color4f valueColor = index == 0
            ? Color4f{0.36f, 0.78f, 1.0f, 1.0f}
            : Color4f{0.88f, 0.91f, 0.96f, 1.0f};
        DrawText(list, values[index].second, x, y + (rowHeight >= 28.0f ? 20.0f : 16.0f), valueSize,
            valueColor, cellWidth - 24.0f);
    }
}

void VoiceMixerWindow::DrawText(
    Engine::DrawList2D& list,
    std::u32string text,
    float x,
    float y,
    float size,
    Color4f color,
    float width) {
    const Engine::TextLayoutHandle layout =
        m_TextLayouts.CreateLayout({}, text, width, size);
    list.AddText({{x, y}, layout, color});
}

bool VoiceMixerWindow::Contains(RectF rect, Vec2F point) noexcept {
    return point.x >= rect.x && point.y >= rect.y &&
        point.x <= rect.x + rect.width && point.y <= rect.y + rect.height;
}

std::u32string_view VoiceMixerWindow::Localized(
    std::u32string_view chinese,
    std::u32string_view english) const noexcept {
    return m_Language == Language::Chinese ? chinese : english;
}

std::u32string VoiceMixerWindow::StateText() const {
    switch (m_Snapshot.state) {
    case VoiceMixerAudioState::Stopped:
        return std::u32string(Localized(U"已停止", U"Stopped"));
    case VoiceMixerAudioState::Starting:
        return std::u32string(Localized(U"启动中", U"Starting"));
    case VoiceMixerAudioState::Running:
        return std::u32string(Localized(U"运行中", U"Running"));
    case VoiceMixerAudioState::Stopping:
        return std::u32string(Localized(U"停止中", U"Stopping"));
    case VoiceMixerAudioState::DeviceLost:
        return std::u32string(Localized(U"设备丢失", U"Device lost"));
    case VoiceMixerAudioState::Error:
        return std::u32string(Localized(U"错误", U"Error"));
    }
    return {};
}

std::u32string VoiceMixerWindow::NoiseStateText(
    Engine::Audio::NoiseSuppressorState state) const {
    using Engine::Audio::NoiseSuppressorState;
    switch (state) {
    case NoiseSuppressorState::Disabled:
        return std::u32string(Localized(U"已禁用", U"Disabled"));
    case NoiseSuppressorState::Bypassed:
        return std::u32string(Localized(U"已旁路", U"Bypassed"));
    case NoiseSuppressorState::Active:
        return std::u32string(Localized(U"处理中", U"Active"));
    case NoiseSuppressorState::UnsupportedFormat:
        return std::u32string(Localized(U"不支持的音频格式", U"Unsupported Format"));
    case NoiseSuppressorState::InitializationFailed:
        return std::u32string(Localized(U"初始化失败", U"Initialization Failed"));
    case NoiseSuppressorState::Error:
        return std::u32string(Localized(U"错误", U"Error"));
    }
    return {};
}

std::u32string VoiceMixerWindow::EQTypeText(Engine::Audio::EQBandType type) const {
    using Engine::Audio::EQBandType;
    switch (type) {
    case EQBandType::Bell: return std::u32string(Localized(U"钟形", U"Bell"));
    case EQBandType::LowShelf: return std::u32string(Localized(U"低架", U"Low Shelf"));
    case EQBandType::HighShelf: return std::u32string(Localized(U"高架", U"High Shelf"));
    case EQBandType::LowCut: return std::u32string(Localized(U"低切", U"Low Cut"));
    case EQBandType::HighCut: return std::u32string(Localized(U"高切", U"High Cut"));
    case EQBandType::Notch: return std::u32string(Localized(U"陷波", U"Notch"));
    }
    return U"Bell";
}

std::u32string VoiceMixerWindow::ToText(std::string_view text) {
    std::u32string result;
    result.reserve(text.size());
    for (const unsigned char value : text) result.push_back(value);
    return result;
}

std::u32string VoiceMixerWindow::ToText(std::wstring_view text) {
    std::u32string result;
    result.reserve(text.size());
    for (const wchar_t value : text) result.push_back(static_cast<char32_t>(value));
    return result;
}

} // namespace XiE::VoiceMixer
