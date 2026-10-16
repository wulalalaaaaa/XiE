#include "Preset/VoicePreset.h"

#include "Infrastructure/Configuration/AtomicFileWriter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>

namespace XiE::VoiceMixer {
namespace {

using JsonValue = EmoExec::Detail::JsonValue;
using Engine::Curve::CurveInterpolation;
using Engine::Curve::CurveModel;
using Engine::Curve::CurvePoint;

const JsonValue* Child(const JsonValue& value, const char* key) {
    return value.Find(key);
}

double Number(const JsonValue* value, double fallback) {
    return value && value->TryNumber() ? *value->TryNumber() : fallback;
}

bool Boolean(const JsonValue* value, bool fallback) {
    return value && value->TryBool() ? *value->TryBool() : fallback;
}

Engine::Audio::NoiseSuppressorMode ReadNoiseMode(std::string value) noexcept {
    return value == "disabled"
        ? Engine::Audio::NoiseSuppressorMode::Disabled
        : Engine::Audio::NoiseSuppressorMode::RNNoise;
}

const char* WriteNoiseMode(Engine::Audio::NoiseSuppressorMode mode) noexcept {
    return mode == Engine::Audio::NoiseSuppressorMode::Disabled ? "disabled" : "RNNoise";
}

std::string String(const JsonValue* value, std::string fallback) {
    return value && value->TryString() ? *value->TryString() : std::move(fallback);
}

JsonValue WriteCurve(const CurveModel& curve) {
    JsonValue::Array points;
    points.reserve(curve.Points().size());
    for (const CurvePoint& point : curve.Points()) {
        JsonValue value(JsonValue::Object{});
        value["id"] = JsonValue(static_cast<double>(point.id));
        value["x"] = JsonValue(static_cast<double>(point.x));
        value["y"] = JsonValue(static_cast<double>(point.y));
        value["interpolation"] = JsonValue(
            point.interpolation == CurveInterpolation::Smooth ? "smooth" : "linear");
        points.emplace_back(std::move(value));
    }
    return JsonValue(std::move(points));
}

const char* WriteBandType(Engine::Audio::EQBandType type) noexcept {
    using Engine::Audio::EQBandType;
    switch (type) {
    case EQBandType::Bell: return "bell";
    case EQBandType::LowShelf: return "lowShelf";
    case EQBandType::HighShelf: return "highShelf";
    case EQBandType::LowCut: return "lowCut";
    case EQBandType::HighCut: return "highCut";
    case EQBandType::Notch: return "notch";
    }
    return "bell";
}

Engine::Audio::EQBandType ReadBandType(std::string value) noexcept {
    using Engine::Audio::EQBandType;
    if (value == "lowShelf") return EQBandType::LowShelf;
    if (value == "highShelf") return EQBandType::HighShelf;
    if (value == "lowCut") return EQBandType::LowCut;
    if (value == "highCut") return EQBandType::HighCut;
    if (value == "notch") return EQBandType::Notch;
    return EQBandType::Bell;
}

JsonValue WriteEQ(const Engine::Audio::ParametricEQModel& model, JsonValue existing) {
    if (!existing.IsObject()) existing = JsonValue::Object{};
    existing["enabled"] = JsonValue(model.Enabled());
    JsonValue::Array bands;
    bands.reserve(model.Bands().size());
    for (const auto& band : model.Bands()) {
        JsonValue value(JsonValue::Object{});
        value["id"] = JsonValue(static_cast<double>(band.id));
        value["enabled"] = JsonValue(band.enabled);
        value["type"] = JsonValue(WriteBandType(band.type));
        value["frequencyHz"] = JsonValue(static_cast<double>(band.frequencyHz));
        value["gainDb"] = JsonValue(static_cast<double>(band.gainDb));
        value["q"] = JsonValue(static_cast<double>(band.q));
        value["slopeDbPerOctave"] = JsonValue(static_cast<double>(band.slopeDbPerOctave));
        bands.emplace_back(std::move(value));
    }
    existing["bands"] = JsonValue(std::move(bands));
    return existing;
}

bool ReadEQ(const JsonValue* value, Engine::Audio::ParametricEQModel& model, std::string& error) {
    model.Reset();
    model.SetEnabled(true);
    if (value == nullptr) return true; // V1 migration: neutral EQ.
    if (!value->IsObject()) {
        error = "eq must be an object";
        return false;
    }
    model.SetEnabled(Boolean(Child(*value, "enabled"), true));
    const JsonValue* bands = Child(*value, "bands");
    if (bands == nullptr) return true;
    if (!bands->IsArray()) {
        error = "eq bands must be an array";
        return false;
    }
    for (const JsonValue& item : *bands->TryArray()) {
        if (!item.IsObject()) {
            error = "eq band must be an object";
            return false;
        }
        Engine::Audio::ParametricEQBand band;
        band.id = static_cast<Engine::Audio::EQBandId>(
            std::max(0.0, Number(Child(item, "id"), 0.0)));
        band.enabled = Boolean(Child(item, "enabled"), true);
        band.type = ReadBandType(String(Child(item, "type"), "bell"));
        band.frequencyHz = static_cast<float>(Number(Child(item, "frequencyHz"), 1'000.0));
        band.gainDb = static_cast<float>(Number(Child(item, "gainDb"), 0.0));
        band.q = static_cast<float>(Number(Child(item, "q"), Engine::Audio::kParametricEQDefaultQ));
        band.slopeDbPerOctave = static_cast<float>(
            Number(Child(item, "slopeDbPerOctave"), 12.0));
        if (band.id == 0 || !model.AddBand(band)) {
            error = "invalid, duplicate, or excessive eq band";
            return false;
        }
    }
    return true;
}

JsonValue WriteSoftHaze(Engine::Audio::SoftHazeConfig config, JsonValue existing) {
    using Engine::Audio::ClampSoftHazeConfig;
    config = ClampSoftHazeConfig(config);
    if (!existing.IsObject()) existing = JsonValue::Object{};
    existing["enabled"] = JsonValue(config.enabled);
    existing["amount"] = JsonValue(static_cast<double>(config.amount));
    existing["blur"] = JsonValue(static_cast<double>(config.blur));
    existing["damping"] = JsonValue(static_cast<double>(config.damping));
    existing["wet"] = JsonValue(static_cast<double>(config.wet));
    return existing;
}

Engine::Audio::SoftHazeConfig ReadSoftHaze(const JsonValue* value) {
    Engine::Audio::SoftHazeConfig config;
    if (value != nullptr && value->IsObject()) {
        config.enabled = Boolean(Child(*value, "enabled"), config.enabled);
        config.amount = static_cast<float>(Number(Child(*value, "amount"), config.amount));
        config.blur = static_cast<float>(Number(Child(*value, "blur"), config.blur));
        config.damping = static_cast<float>(Number(Child(*value, "damping"), config.damping));
        config.wet = static_cast<float>(Number(Child(*value, "wet"), config.wet));
    }
    return Engine::Audio::ClampSoftHazeConfig(config);
}

JsonValue WriteSpatialHaze(Engine::Audio::SpatialHazeConfig config, JsonValue existing) {
    using Engine::Audio::ClampSpatialHazeConfig;
    config = ClampSpatialHazeConfig(config);
    if (!existing.IsObject()) existing = JsonValue::Object{};
    existing["enabled"] = JsonValue(config.enabled);
    existing["amount"] = JsonValue(static_cast<double>(config.amount));
    existing["size"] = JsonValue(static_cast<double>(config.size));
    existing["diffusion"] = JsonValue(static_cast<double>(config.diffusion));
    existing["damping"] = JsonValue(static_cast<double>(config.damping));
    existing["width"] = JsonValue(static_cast<double>(config.width));
    existing["wet"] = JsonValue(static_cast<double>(config.wet));
    return existing;
}

Engine::Audio::SpatialHazeConfig ReadSpatialHaze(const JsonValue* value) {
    Engine::Audio::SpatialHazeConfig config;
    if (value != nullptr && value->IsObject()) {
        config.enabled = Boolean(Child(*value, "enabled"), config.enabled);
        config.amount = static_cast<float>(Number(Child(*value, "amount"), config.amount));
        config.size = static_cast<float>(Number(Child(*value, "size"), config.size));
        config.diffusion = static_cast<float>(Number(Child(*value, "diffusion"), config.diffusion));
        config.damping = static_cast<float>(Number(Child(*value, "damping"), config.damping));
        config.width = static_cast<float>(Number(Child(*value, "width"), config.width));
        config.wet = static_cast<float>(Number(Child(*value, "wet"), config.wet));
    }
    return Engine::Audio::ClampSpatialHazeConfig(config);
}

JsonValue WriteSyntheticVoice(Engine::Audio::SyntheticVoiceConfig config, JsonValue existing) {
    config = Engine::Audio::ClampSyntheticVoiceConfig(config);
    if (!existing.IsObject()) existing = JsonValue::Object{};
    existing["enabled"] = JsonValue(config.enabled);
    existing["amount"] = JsonValue(static_cast<double>(config.amount));
    existing["stability"] = JsonValue(static_cast<double>(config.stability));
    existing["electronic"] = JsonValue(static_cast<double>(config.electronic));
    existing["width"] = JsonValue(static_cast<double>(config.width));
    existing["wet"] = JsonValue(static_cast<double>(config.wet));
    return existing;
}

Engine::Audio::SyntheticVoiceConfig ReadSyntheticVoice(const JsonValue* value) {
    Engine::Audio::SyntheticVoiceConfig config;
    if (value != nullptr && value->IsObject()) {
        config.enabled = Boolean(Child(*value, "enabled"), config.enabled);
        config.amount = static_cast<float>(Number(Child(*value, "amount"), config.amount));
        config.stability = static_cast<float>(Number(Child(*value, "stability"), config.stability));
        config.electronic = static_cast<float>(Number(Child(*value, "electronic"), config.electronic));
        config.width = static_cast<float>(Number(Child(*value, "width"), config.width));
        config.wet = static_cast<float>(Number(Child(*value, "wet"), config.wet));
    }
    return Engine::Audio::ClampSyntheticVoiceConfig(config);
}

bool ReadCurve(const JsonValue* value, CurveModel& curve, std::string& error);

JsonValue WriteElectronicMist(
    Engine::Audio::ElectronicMistConfig config,
    const CurveModel& curve,
    JsonValue existing) {
    config = Engine::Audio::ClampElectronicMistConfig(config);
    if (!existing.IsObject()) existing = JsonValue::Object{};
    existing["enabled"] = JsonValue(config.enabled);
    existing["amount"] = JsonValue(static_cast<double>(config.amount));
    existing["follow"] = JsonValue(static_cast<double>(config.follow));
    existing["sharpness"] = JsonValue(static_cast<double>(config.sharpness));
    existing["width"] = JsonValue(static_cast<double>(config.width));
    existing["wet"] = JsonValue(static_cast<double>(config.wet));
    existing["noiseType"] = JsonValue(
        config.noiseType == Engine::Audio::ElectronicMistNoiseType::White ? "white" :
        config.noiseType == Engine::Audio::ElectronicMistNoiseType::Digital ? "digital" : "soft");
    existing["curve"] = WriteCurve(curve);
    return existing;
}

Engine::Audio::ElectronicMistNoiseType ReadElectronicMistNoiseType(std::string value) noexcept {
    if (value == "white") return Engine::Audio::ElectronicMistNoiseType::White;
    if (value == "digital") return Engine::Audio::ElectronicMistNoiseType::Digital;
    return Engine::Audio::ElectronicMistNoiseType::Soft;
}

Engine::Audio::ElectronicMistConfig ReadElectronicMist(
    const JsonValue* value,
    CurveModel& curve,
    std::string& error) {
    Engine::Audio::ElectronicMistConfig config;
    if (value == nullptr) return config;
    if (!value->IsObject()) {
        error = "electronicMist must be an object";
        return config;
    }
    config.enabled = Boolean(Child(*value, "enabled"), config.enabled);
    config.amount = static_cast<float>(Number(Child(*value, "amount"), config.amount));
    config.follow = static_cast<float>(Number(Child(*value, "follow"), config.follow));
    config.sharpness = static_cast<float>(Number(Child(*value, "sharpness"), config.sharpness));
    config.width = static_cast<float>(Number(Child(*value, "width"), config.width));
    config.wet = static_cast<float>(Number(Child(*value, "wet"), config.wet));
    config.noiseType = ReadElectronicMistNoiseType(
        String(Child(*value, "noiseType"), "soft"));
    const JsonValue* curveValue = Child(*value, "curve");
    if (curveValue != nullptr && !ReadCurve(curveValue, curve, error)) return config;
    return Engine::Audio::ClampElectronicMistConfig(config);
}

bool ReadCurve(const JsonValue* value, CurveModel& curve, std::string& error) {
    if (value == nullptr || !value->IsArray()) {
        error = "curve must be an array";
        return false;
    }
    CurveModel candidate;
    for (const JsonValue& item : *value->TryArray()) {
        if (!item.IsObject()) {
            error = "curve point must be an object";
            return false;
        }
        CurvePoint point;
        point.id = static_cast<Engine::Curve::CurvePointId>(
            std::max(0.0, Number(Child(item, "id"), 0.0)));
        point.x = static_cast<float>(Number(Child(item, "x"), 0.0));
        point.y = static_cast<float>(Number(Child(item, "y"), 0.0));
        point.interpolation = String(Child(item, "interpolation"), "smooth") == "linear"
            ? CurveInterpolation::Linear
            : CurveInterpolation::Smooth;
        if (point.id == 0 || candidate.AddPoint(point) != Engine::Curve::CurveStatus::Ok) {
            error = "invalid or duplicate curve point";
            return false;
        }
    }
    if (candidate.Points().empty()) {
        error = "curve must contain at least one point";
        return false;
    }
    curve = std::move(candidate);
    return true;
}

} // namespace

VoicePreset MakeNeutralPreset() {
    VoicePreset preset;
    preset.name = "Neutral";
    preset.parameters = {};
    (void)preset.pitchCurve.AddPoint({1, 70.0f, 0.0f, CurveInterpolation::Smooth});
    (void)preset.pitchCurve.AddPoint({2, 500.0f, 0.0f, CurveInterpolation::Smooth});
    (void)preset.formantCurve.AddPoint({1, 70.0f, 0.0f, CurveInterpolation::Smooth});
    (void)preset.formantCurve.AddPoint({2, 500.0f, 0.0f, CurveInterpolation::Smooth});
    (void)preset.electronicNoiseCurve.AddPoint({1, 20.0f, 0.0f, CurveInterpolation::Smooth});
    (void)preset.electronicNoiseCurve.AddPoint({2, 20'000.0f, 0.0f, CurveInterpolation::Smooth});
    return preset;
}

bool VoicePresetRepository::Save(
    const std::filesystem::path& path,
    const VoicePreset& preset,
    std::string& error) {
    JsonValue document = m_Document;
    if (!document.IsObject()) {
        document = JsonValue::Object{};
    }
    document["schemaVersion"] = JsonValue(6.0);
    document["name"] = JsonValue(preset.name);
    auto& parameters = document["parameters"];
    parameters.AsObject();
    parameters["wet"] = JsonValue(static_cast<double>(preset.parameters.wet));
    parameters["inputGain"] = JsonValue(static_cast<double>(preset.parameters.inputGain));
    parameters["outputGain"] = JsonValue(static_cast<double>(preset.parameters.outputGain));
    parameters["pitchSemitones"] = JsonValue(static_cast<double>(preset.parameters.pitchSemitones));
    parameters["formantSemitones"] = JsonValue(static_cast<double>(preset.parameters.formantSemitones));
    parameters["bypass"] = JsonValue(preset.parameters.bypass);
    parameters["monitor"] = JsonValue(preset.parameters.monitor);
    document["pitchCurve"] = WriteCurve(preset.pitchCurve);
    document["formantCurve"] = WriteCurve(preset.formantCurve);
    const JsonValue* existingEQ = Child(document, "eq");
    document["eq"] = WriteEQ(preset.eq,
        existingEQ == nullptr ? JsonValue(JsonValue::Object{}) : *existingEQ);
    const JsonValue* existingSoftHaze = Child(document, "softHaze");
    document["softHaze"] = WriteSoftHaze(preset.parameters.softHaze,
        existingSoftHaze == nullptr ? JsonValue(JsonValue::Object{}) : *existingSoftHaze);
    const JsonValue* existingSpatialHaze = Child(document, "spatialHaze");
    document["spatialHaze"] = WriteSpatialHaze(preset.parameters.spatialHaze,
        existingSpatialHaze == nullptr ? JsonValue(JsonValue::Object{}) : *existingSpatialHaze);
    const JsonValue* existingSyntheticVoice = Child(document, "syntheticVoice");
    document["syntheticVoice"] = WriteSyntheticVoice(preset.parameters.syntheticVoice,
        existingSyntheticVoice == nullptr ? JsonValue(JsonValue::Object{}) : *existingSyntheticVoice);
    const JsonValue* existingElectronicMist = Child(document, "electronicMist");
    document["electronicMist"] = WriteElectronicMist(preset.parameters.electronicMist,
        preset.electronicNoiseCurve,
        existingElectronicMist == nullptr ? JsonValue(JsonValue::Object{}) : *existingElectronicMist);
    auto& noise = document["noiseSuppression"];
    noise.AsObject();
    noise["enabled"] = JsonValue(preset.parameters.noiseEnabled);
    noise["mode"] = JsonValue(WriteNoiseMode(Engine::Audio::NoiseSuppressorMode::RNNoise));
    noise["strength"] = JsonValue(static_cast<double>(preset.parameters.noiseStrength));
    noise["wet"] = JsonValue(static_cast<double>(preset.parameters.noiseWet));
    noise["bypassed"] = JsonValue(preset.parameters.noiseBypassed);

    const std::string text = EmoExec::Detail::WriteJson(document, 2) + "\n";
    std::vector<std::byte> bytes(text.size());
    std::transform(text.begin(), text.end(), bytes.begin(), [](char value) {
        return static_cast<std::byte>(static_cast<unsigned char>(value));
    });
    std::error_code filesystemError;
    std::filesystem::create_directories(path.parent_path(), filesystemError);
    if (filesystemError) {
        error = filesystemError.message();
        return false;
    }
    const auto result = EmoExec::AtomicFileWriter{}.Write(path, bytes);
    if (!result.success) {
        error = result.error;
        return false;
    }
    m_Document = std::move(document);
    error.clear();
    return true;
}

bool VoicePresetRepository::Load(
    const std::filesystem::path& path,
    VoicePreset& preset,
    std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "cannot open preset";
        return false;
    }
    const std::string text{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    auto parsed = EmoExec::Detail::ParseJson(text);
    if (!parsed.success || !parsed.value.IsObject()) {
        error = parsed.success ? "preset root must be an object" : parsed.error;
        return false;
    }
    VoicePreset candidate = MakeNeutralPreset();
    candidate.name = String(Child(parsed.value, "name"), candidate.name);
    if (const JsonValue* parameters = Child(parsed.value, "parameters");
        parameters != nullptr && parameters->IsObject()) {
        candidate.parameters.wet = static_cast<float>(
            Number(Child(*parameters, "wet"), candidate.parameters.wet));
        candidate.parameters.inputGain = static_cast<float>(
            Number(Child(*parameters, "inputGain"), candidate.parameters.inputGain));
        candidate.parameters.outputGain = static_cast<float>(
            Number(Child(*parameters, "outputGain"), candidate.parameters.outputGain));
        candidate.parameters.pitchSemitones = static_cast<float>(
            Number(Child(*parameters, "pitchSemitones"), candidate.parameters.pitchSemitones));
        candidate.parameters.formantSemitones = static_cast<float>(
            Number(Child(*parameters, "formantSemitones"), candidate.parameters.formantSemitones));
        candidate.parameters.bypass = Boolean(
            Child(*parameters, "bypass"), candidate.parameters.bypass);
        candidate.parameters.monitor = Boolean(
            Child(*parameters, "monitor"), candidate.parameters.monitor);
    }
    if (const JsonValue* noise = Child(parsed.value, "noiseSuppression");
        noise != nullptr) {
        if (!noise->IsObject()) {
            error = "noiseSuppression must be an object";
            return false;
        }
        candidate.parameters.noiseEnabled = Boolean(
            Child(*noise, "enabled"), candidate.parameters.noiseEnabled);
        const auto mode = ReadNoiseMode(String(Child(*noise, "mode"), "RNNoise"));
        if (mode == Engine::Audio::NoiseSuppressorMode::Disabled) {
            candidate.parameters.noiseEnabled = false;
        }
        candidate.parameters.noiseStrength = static_cast<float>(Number(
            Child(*noise, "strength"), candidate.parameters.noiseStrength));
        candidate.parameters.noiseWet = static_cast<float>(Number(
            Child(*noise, "wet"), candidate.parameters.noiseWet));
        candidate.parameters.noiseBypassed = Boolean(
            Child(*noise, "bypassed"), candidate.parameters.noiseBypassed);
    }
    if (!ReadCurve(Child(parsed.value, "pitchCurve"), candidate.pitchCurve, error) ||
        !ReadCurve(Child(parsed.value, "formantCurve"), candidate.formantCurve, error) ||
        !ReadEQ(Child(parsed.value, "eq"), candidate.eq, error)) {
        return false;
    }
    candidate.parameters.softHaze = ReadSoftHaze(Child(parsed.value, "softHaze"));
    candidate.parameters.spatialHaze = ReadSpatialHaze(Child(parsed.value, "spatialHaze"));
    candidate.parameters.syntheticVoice = ReadSyntheticVoice(
        Child(parsed.value, "syntheticVoice"));
    candidate.parameters.electronicMist = ReadElectronicMist(
        Child(parsed.value, "electronicMist"), candidate.electronicNoiseCurve, error);
    if (!error.empty()) return false;
    candidate.parameters.wet = std::clamp(candidate.parameters.wet, 0.0f, 1.0f);
    candidate.parameters.inputGain = std::clamp(candidate.parameters.inputGain, 0.0f, 4.0f);
    candidate.parameters.outputGain = std::clamp(candidate.parameters.outputGain, 0.0f, 4.0f);
    candidate.parameters.pitchSemitones =
        std::clamp(candidate.parameters.pitchSemitones, -12.0f, 12.0f);
    candidate.parameters.formantSemitones =
        std::clamp(candidate.parameters.formantSemitones, -6.0f, 6.0f);
    candidate.parameters.noiseStrength = std::clamp(
        std::isfinite(candidate.parameters.noiseStrength)
            ? candidate.parameters.noiseStrength : 1.0f, 0.0f, 1.0f);
    candidate.parameters.noiseWet = std::clamp(
        std::isfinite(candidate.parameters.noiseWet)
            ? candidate.parameters.noiseWet : 1.0f, 0.0f, 1.0f);
    candidate.parameters.syntheticVoice = Engine::Audio::ClampSyntheticVoiceConfig(
        candidate.parameters.syntheticVoice);
    candidate.parameters.electronicMist = Engine::Audio::ClampElectronicMistConfig(
        candidate.parameters.electronicMist);
    m_Document = std::move(parsed.value); // Retains unknown fields on the next save.
    preset = std::move(candidate);
    error.clear();
    return true;
}

} // namespace XiE::VoiceMixer
