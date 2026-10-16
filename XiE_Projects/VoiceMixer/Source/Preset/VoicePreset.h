#pragma once

#include "App/VoiceMixerAudioController.h"
#include "Curve/Curve.h"
#include "Infrastructure/Configuration/JsonValue.h"

#include <filesystem>
#include <string>

namespace XiE::VoiceMixer {

struct VoicePreset {
    std::string name = "Neutral";
    VoiceMixerParameters parameters{};
    Engine::Curve::CurveModel pitchCurve;
    Engine::Curve::CurveModel formantCurve;
    Engine::Audio::ParametricEQModel eq;
    Engine::Curve::CurveModel electronicNoiseCurve;
};

[[nodiscard]] VoicePreset MakeNeutralPreset();

class VoicePresetRepository {
public:
    bool Save(const std::filesystem::path& path, const VoicePreset& preset, std::string& error);
    bool Load(const std::filesystem::path& path, VoicePreset& preset, std::string& error);

private:
    EmoExec::Detail::JsonValue m_Document{EmoExec::Detail::JsonValue::Object{}};
};

} // namespace XiE::VoiceMixer
