#pragma once

#include "App/VoiceMixerAudioController.h"
#include "Input/InputEvents.h"
#include "Preset/VoicePreset.h"
#include "Renderer2D/DrawList2D.h"
#include "Renderer2D/TextLayoutService.h"
#include "UI/UICurveEditor.h"
#include "UI2D/Audio/UIAudioCurveEditor.h"
#include "UI2D/Audio/UIParametricEQEditor.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <optional>

namespace XiE::VoiceMixer {

class VoiceMixerWindow {
public:
    enum class VoiceEditorPage : std::uint8_t {
        Noise,
        Pitch,
        Formant,
        EQ,
        FX
    };

    VoiceMixerWindow(
        VoiceMixerAudioController& audio,
        std::filesystem::path presetPath);

    void SetViewport(float width, float height, float dpiScale);
    void HandleInput(const Engine::InputEvent& event);
    void Tick();
    void BuildDrawList(Engine::DrawList2D& list);

    [[nodiscard]] Engine::BasicTextLayoutService& TextLayouts() noexcept {
        return m_TextLayouts;
    }
    [[nodiscard]] bool ConsumeHideRequested() noexcept;
    [[nodiscard]] bool ConsumeMinimizeRequested() noexcept;
    [[nodiscard]] bool IsTitleBarDragRegion(Engine::Vec2F point) const noexcept;
    [[nodiscard]] bool IsChineseLanguage() const noexcept {
        return m_Language == Language::Chinese;
    }
    [[nodiscard]] const VoicePreset& Preset() const noexcept { return m_Preset; }
    [[nodiscard]] const UICurveEditor& CurveEditor() const noexcept { return m_CurveEditor; }
    [[nodiscard]] const Engine::UI::UIParametricEQEditor& EQEditor() const noexcept {
        return m_EQEditor;
    }
    [[nodiscard]] const Engine::UI::UIAudioCurveEditor& UnifiedEditor() const noexcept {
        return m_UnifiedEditor;
    }
    [[nodiscard]] VoiceEditorPage EditorPage() const noexcept { return m_EditorPage; }

private:
    enum class Language : std::uint8_t {
        Chinese,
        English
    };

    enum class Slider : std::uint8_t {
        Wet,
        Pitch,
        Formant,
        Count
    };

    enum class NoiseSlider : std::uint8_t {
        Strength,
        Wet
    };

    enum class FXSlider : std::uint8_t {
        SyntheticAmount,
        SyntheticStability,
        SyntheticElectronic,
        SyntheticWidth,
        SyntheticWet,
        MistAmount,
        MistFollow,
        MistSharpness,
        MistWidth,
        MistWet,
        SoftAmount,
        SoftBlur,
        SoftDamping,
        SoftWet,
        SpatialAmount,
        SpatialSize,
        SpatialDiffusion,
        SpatialDamping,
        SpatialWidth,
        SpatialWet,
        Count
    };

    enum class Dropdown : std::uint8_t {
        None,
        Input,
        Output
    };

    struct Layout {
        Engine::RectF language{};
        Engine::RectF minimize{};
        Engine::RectF close{};
        Engine::RectF inputDevice{};
        Engine::RectF outputDevice{};
        Engine::RectF refresh{};
        Engine::RectF start{};
        Engine::RectF stop{};
        Engine::RectF restart{};
        Engine::RectF monitor{};
        Engine::RectF bypass{};
        Engine::RectF save{};
        Engine::RectF load{};
        Engine::RectF neutral{};
        Engine::RectF editorTabBar{};
        Engine::RectF pitchTrack{};
        Engine::RectF formantTrack{};
        Engine::RectF eqTrack{};
        Engine::RectF fxTrack{};
        Engine::RectF noiseTrack{};
        Engine::RectF noiseEnable{};
        Engine::RectF noiseMode{};
        Engine::RectF noiseBypass{};
        Engine::RectF resetNoise{};
        Engine::RectF noiseStrength{};
        Engine::RectF noiseWet{};
        Engine::RectF eqBypass{};
        Engine::RectF resetEq{};
        Engine::RectF syntheticCard{};
        Engine::RectF electronicCard{};
        Engine::RectF softCard{};
        Engine::RectF spatialCard{};
        Engine::RectF electronicEnable{};
        Engine::RectF editNoiseCurve{};
        Engine::RectF resetNoiseCurve{};
        Engine::RectF syntheticEnable{};
        Engine::RectF softEnable{};
        Engine::RectF spatialEnable{};
        std::array<Engine::RectF, 5> syntheticSliders{};
        std::array<Engine::RectF, 5> electronicSliders{};
        std::array<Engine::RectF, 4> softSliders{};
        std::array<Engine::RectF, 6> spatialSliders{};
        Engine::RectF inputMeter{};
        Engine::RectF outputMeter{};
        Engine::RectF curve{};
        std::array<Engine::RectF, static_cast<std::size_t>(Slider::Count)> faders{};
        Engine::RectF diagnostics{};
    };

    void RecalculateLayout();
    void LoadLanguagePreference();
    void SaveLanguagePreference() const;
    void LoadEditorPagePreference();
    void SaveEditorPagePreference() const;
    void SetEditorPage(VoiceEditorPage page);
    void LoadPresetIntoEditor();
    void PublishCurves();
    void PublishParametricEQ();
    void ApplyParameters();
    void Activate(Engine::Vec2F point);
    void UpdateSlider(Slider slider, float y);
    void AdjustSlider(Slider slider, float delta);
    void UpdateNoiseSlider(NoiseSlider slider, float x);
    void UpdateFXSlider(FXSlider slider, float x);
    void AdjustFXSlider(FXSlider slider, float delta);
    void DrawNoisePage(Engine::DrawList2D& list);
    void DrawFXPage(Engine::DrawList2D& list);
    void DrawFXSlider(Engine::DrawList2D& list, Engine::RectF rect,
        std::u32string_view label, float value);
    void DrawNoiseSlider(
        Engine::DrawList2D& list,
        Engine::RectF rect,
        std::u32string_view label,
        float value);
    void DrawButton(
        Engine::DrawList2D& list,
        Engine::RectF rect,
        std::u32string_view text,
        bool enabled,
        bool active = false,
        bool compact = false);
    void DrawDeviceSelect(
        Engine::DrawList2D& list,
        Engine::RectF rect,
        std::u32string_view label,
        std::u32string value,
        bool enabled,
        bool open);
    void DrawDropdown(Engine::DrawList2D& list);
    void DrawFader(
        Engine::DrawList2D& list,
        Slider slider,
        std::u32string_view label,
        float value,
        float minimum,
        float maximum,
        std::u32string_view unit = U"");
    void DrawMeter(
        Engine::DrawList2D& list,
        Engine::RectF rect,
        float value,
        Engine::Color4f color);
    void DrawDiagnostics(Engine::DrawList2D& list);
    void DrawText(
        Engine::DrawList2D& list,
        std::u32string text,
        float x,
        float y,
        float size = 13.0f,
        Engine::Color4f color = {0.88f, 0.91f, 0.96f, 1.0f},
        float width = 880.0f);

    [[nodiscard]] Engine::RectF DropdownItemRect(std::size_t index) const noexcept;
    [[nodiscard]] std::size_t DropdownVisibleCount(std::size_t itemCount) const noexcept;
    [[nodiscard]] Engine::RectF FaderRect(Slider slider) const noexcept;
    [[nodiscard]] static bool Contains(Engine::RectF rect, Engine::Vec2F point) noexcept;
    [[nodiscard]] std::u32string_view Localized(
        std::u32string_view chinese,
        std::u32string_view english) const noexcept;
    [[nodiscard]] std::u32string StateText() const;
    [[nodiscard]] std::u32string NoiseStateText(
        Engine::Audio::NoiseSuppressorState state) const;
    [[nodiscard]] std::u32string EQTypeText(Engine::Audio::EQBandType type) const;
    [[nodiscard]] static std::u32string ToText(std::string_view text);
    [[nodiscard]] static std::u32string ToText(std::wstring_view text);

    VoiceMixerAudioController& m_Audio;
    std::filesystem::path m_PresetPath;
    VoicePresetRepository m_Repository;
    VoicePreset m_Preset;
    UICurveEditor m_CurveEditor;
    Engine::UI::UIParametricEQEditor m_EQEditor;
    Engine::UI::UIAudioCurveEditor m_UnifiedEditor;
    Engine::BasicTextLayoutService m_TextLayouts;
    VoiceMixerAudioSnapshot m_Snapshot{};
    Layout m_Layout{};
    std::optional<Slider> m_ActiveSlider;
    std::optional<NoiseSlider> m_ActiveNoiseSlider;
    std::optional<FXSlider> m_ActiveFXSlider;
    Dropdown m_OpenDropdown = Dropdown::None;
    float m_Width = 1400.0f;
    float m_Height = 900.0f;
    float m_DpiScale = 1.0f;
    Language m_Language = Language::Chinese;
    VoiceEditorPage m_EditorPage = VoiceEditorPage::Pitch;
    Engine::Vec2F m_HoverPoint{};
    bool m_HoverValid = false;
    bool m_CurvesDirty = true;
    bool m_EQDirty = true;
    bool m_HideRequested = false;
    bool m_MinimizeRequested = false;
    std::u32string m_Message;
    std::chrono::steady_clock::time_point m_LastCurvePublish{};
    std::chrono::steady_clock::time_point m_LastEQPublish{};
};

} // namespace XiE::VoiceMixer
