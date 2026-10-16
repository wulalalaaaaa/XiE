#include "App/VoiceMixerAudioController.h"
#include "Preset/VoicePreset.h"
#include "UI/UICurveEditor.h"
#include "UI/VoiceMixerWindow.h"
#include "UI2D/Audio/UIParametricEQEditor.h"
#include "UI2D/Audio/UIAudioCurveEditor.h"

#include <cmath>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

namespace {

namespace Audio = Engine::Audio;
using XiE::VoiceMixer::UICurveEditor;
using XiE::VoiceMixer::VoiceMixerAudioController;
using XiE::VoiceMixer::VoiceMixerAudioState;
using XiE::VoiceMixer::VoicePreset;
using XiE::VoiceMixer::VoicePresetRepository;

int failures = 0;

void Check(bool condition, const char* name) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    if (!condition) ++failures;
}

class TestAudioTrackAdapter final : public Engine::UI::IAudioEditorTrackAdapter {
public:
    struct Node { std::uint64_t id; float x; float y; };
    explicit TestAudioTrackAdapter(std::vector<Node> nodes) : nodes(std::move(nodes)) {}

    std::size_t NodeCount() const noexcept override { return nodes.size(); }
    std::uint64_t NodeIdAt(std::size_t index) const noexcept override {
        return index < nodes.size() ? nodes[index].id : 0;
    }
    Engine::Vec2F NodePosition(std::uint64_t id,
        const Engine::UI::AudioEditorAxisBinding& axes,
        Engine::RectF bounds) const noexcept override {
        for (const Node& node : nodes) if (node.id == id) return axes.ToScreen(bounds, node.x, node.y);
        return {};
    }
    std::optional<std::uint64_t> HitTest(Engine::Vec2F point,
        const Engine::UI::AudioEditorAxisBinding& axes,
        Engine::RectF bounds, float radius) const noexcept override {
        std::optional<std::uint64_t> result;
        float closest = radius * radius;
        for (const Node& node : nodes) {
            const Engine::Vec2F center = axes.ToScreen(bounds, node.x, node.y);
            const float dx = point.x - center.x;
            const float dy = point.y - center.y;
            const float distance = dx * dx + dy * dy;
            if (distance <= closest) { closest = distance; result = node.id; }
        }
        return result;
    }
    bool PointerDown(Engine::Vec2F, bool secondary, double,
        const Engine::UI::AudioEditorAxisBinding&, Engine::RectF,
        std::optional<std::uint64_t> id) override {
        m_Dragging = id.has_value() && !secondary;
        return id.has_value();
    }
    bool PointerMove(Engine::Vec2F point,
        const Engine::UI::AudioEditorAxisBinding& axes,
        Engine::RectF bounds, std::uint64_t id) override {
        if (!m_Dragging) return false;
        for (Node& node : nodes) if (node.id == id) {
            node.x = axes.FromScreenX(bounds, point.x);
            node.y = axes.FromScreenY(bounds, point.y);
            return true;
        }
        return false;
    }
    bool PointerWheel(Engine::Vec2F, float,
        const Engine::UI::AudioEditorAxisBinding&, Engine::RectF) override { return false; }
    void PointerUp() noexcept override { m_Dragging = false; }
    bool CreatePointAt(Engine::Vec2F point,
        const Engine::UI::AudioEditorAxisBinding& axes, Engine::RectF bounds) override {
        nodes.push_back({nextId++, axes.FromScreenX(bounds, point.x), axes.FromScreenY(bounds, point.y)});
        return true;
    }
    bool RemoveNode(std::uint64_t id) override {
        const auto it = std::find_if(nodes.begin(), nodes.end(), [id](const Node& node) { return node.id == id; });
        if (it == nodes.end()) return false;
        nodes.erase(it);
        return true;
    }
    void DrawOverlay(Engine::DrawList2D&, const Engine::UI::AudioEditorAxisBinding&,
        Engine::RectF, bool) const override {}

    std::vector<Node> nodes;
    std::uint64_t nextId = 10;
    bool m_Dragging = false;
};

class FakeInput final : public Audio::IAudioInputStream {
public:
    Audio::AudioStatus Start(Audio::AudioInputCallback callback, void* user) override {
        m_Callback = callback; m_User = user; m_State = Audio::AudioStreamState::Running; return {};
    }
    Audio::AudioStatus Stop() override {
        ++stopCount; m_State = Audio::AudioStreamState::Stopped; return {};
    }
    void Reset() noexcept override {}
    Audio::AudioStreamState State() const noexcept override { return m_State; }
    Audio::AudioFormat Format() const noexcept override { return m_Format; }
    Audio::AudioLatency Latency() const noexcept override {
        Audio::AudioLatency value; value.captureMilliseconds = 3.0; return value;
    }
    void Emit() noexcept {
        Audio::AudioBuffer buffer(1, 128);
        for (std::size_t frame = 0; frame < 128; ++frame) {
            buffer.View().Sample(0, frame) = 0.1f * std::sin(
                2.0f * 3.14159265f * 220.0f * static_cast<float>(frame) / 48'000.0f);
        }
        const Audio::AudioBuffer& input = buffer;
        m_Callback(m_User, input.View(), streamFrame);
        streamFrame += 128;
    }
    void SetState(Audio::AudioStreamState state) noexcept { m_State = state; }
    int stopCount = 0;
    std::uint64_t streamFrame = 0;

private:
    Audio::AudioInputCallback m_Callback = nullptr;
    void* m_User = nullptr;
    Audio::AudioStreamState m_State = Audio::AudioStreamState::Stopped;
    Audio::AudioFormat m_Format{48'000, 1, Audio::AudioSampleFormat::Float32, 128};
};

class FakeOutput final : public Audio::IAudioOutputStream {
public:
    Audio::AudioStatus Start(Audio::AudioOutputCallback callback, void* user) override {
        m_Callback = callback; m_User = user; m_State = Audio::AudioStreamState::Running; return {};
    }
    Audio::AudioStatus Stop() override {
        ++stopCount; m_State = Audio::AudioStreamState::Stopped; return {};
    }
    void Reset() noexcept override {}
    Audio::AudioStreamState State() const noexcept override { return m_State; }
    Audio::AudioFormat Format() const noexcept override { return m_Format; }
    Audio::AudioLatency Latency() const noexcept override {
        Audio::AudioLatency value; value.outputMilliseconds = 3.0; return value;
    }
    float Render() noexcept {
        Audio::AudioBuffer buffer(1, 128);
        m_Callback(m_User, buffer.View(), streamFrame);
        streamFrame += 128;
        float peak = 0;
        for (float sample : buffer.View().Channel(0)) peak = std::max(peak, std::abs(sample));
        return peak;
    }
    int stopCount = 0;
    std::uint64_t streamFrame = 0;

private:
    Audio::AudioOutputCallback m_Callback = nullptr;
    void* m_User = nullptr;
    Audio::AudioStreamState m_State = Audio::AudioStreamState::Stopped;
    Audio::AudioFormat m_Format{48'000, 1, Audio::AudioSampleFormat::Float32, 128};
};

class FakeBackend final : public Audio::IAudioBackend {
public:
    std::vector<Audio::AudioDeviceInfo> EnumerateDevices(
        Audio::AudioDeviceDirection direction, Audio::AudioStatus* status) override {
        ++enumerations;
        if (status) *status = {};
        return {{direction == Audio::AudioDeviceDirection::Input ? L"in" : L"out",
            direction == Audio::AudioDeviceDirection::Input ? inputName : outputName,
            direction, true, {48'000, 1, Audio::AudioSampleFormat::Float32, 128}}};
    }
    std::unique_ptr<Audio::IAudioInputStream> OpenInput(
        const Audio::AudioStreamConfig&, Audio::AudioStatus* status) override {
        if (status) *status = {};
        auto value = std::make_unique<FakeInput>();
        input = value.get();
        return value;
    }
    std::unique_ptr<Audio::IAudioOutputStream> OpenOutput(
        const Audio::AudioStreamConfig&, Audio::AudioStatus* status) override {
        if (status) *status = {};
        auto value = std::make_unique<FakeOutput>();
        output = value.get();
        return value;
    }
    int enumerations = 0;
    FakeInput* input = nullptr;
    FakeOutput* output = nullptr;
    std::wstring inputName = L"Fake Mic";
    std::wstring outputName = L"Fake Output";
};

void ControllerTests() {
    auto backend = std::make_unique<FakeBackend>();
    FakeBackend* fake = backend.get();
    VoiceMixerAudioController controller(std::move(backend));
    Check(static_cast<bool>(controller.RefreshDevices()), "enumerate and refresh endpoints");
    Check(fake->enumerations == 2, "input and output enumerated");
    Check(controller.SelectInput(0) && controller.SelectOutput(0), "select endpoints");

    for (int cycle = 0; cycle < 10; ++cycle) {
        Check(static_cast<bool>(controller.Start()), "start cycle");
        fake->input->Emit();
        (void)fake->output->Render();
        Check(controller.State() == VoiceMixerAudioState::Running, "running state");
        Check(static_cast<bool>(controller.Stop()), "stop cycle");
        Check(controller.State() == VoiceMixerAudioState::Stopped, "stopped state");
    }
    Check(static_cast<bool>(controller.Start()), "start before restart loop");
    for (int cycle = 0; cycle < 10; ++cycle) {
        Check(static_cast<bool>(controller.Restart()), "restart cycle");
        fake->input->Emit();
        (void)fake->output->Render();
    }
    Check(static_cast<bool>(controller.Stop()), "final stop");
    Check(static_cast<bool>(controller.Start()), "start before device-loss recovery");
    fake->input->SetState(Audio::AudioStreamState::DeviceLost);
    controller.Poll();
    Check(controller.State() == VoiceMixerAudioState::DeviceLost,
        "device loss reaches controller state");
    Check(static_cast<bool>(controller.RefreshDevices()),
        "refresh releases lost stream and enumerates");
    Check(controller.State() == VoiceMixerAudioState::Stopped,
        "refresh makes restart available after device loss");
    Check(static_cast<bool>(controller.Restart()), "restart after device loss");
    Check(static_cast<bool>(controller.Stop()), "stop after device-loss recovery");

    auto neutral = XiE::VoiceMixer::MakeNeutralPreset();
    bool publicationsPassed = true;
    for (int update = 0; update < 500; ++update) {
        neutral.parameters.bypass = (update & 1) != 0;
        neutral.parameters.wet = static_cast<float>(update % 101) / 100.0f;
        neutral.parameters.softHaze.enabled = update % 5 != 0;
        neutral.parameters.softHaze.amount = static_cast<float>((update * 7) % 101) / 100.0f;
        neutral.parameters.spatialHaze.enabled = update % 7 != 0;
        neutral.parameters.spatialHaze.width = static_cast<float>((update * 11) % 101) / 100.0f;
        controller.ApplyParameters(neutral.parameters);
        auto pitch = Engine::Curve::BuildCurveSnapshot(
            neutral.pitchCurve, 1024, 70, 500);
        auto formant = Engine::Curve::BuildCurveSnapshot(
            neutral.formantCurve, 1024, 70, 500);
        publicationsPassed = publicationsPassed &&
            static_cast<bool>(controller.ApplyCurveSnapshots(
                std::move(pitch), std::move(formant)));
        if ((update % 25) == 0) {
            auto eq = neutral.eq;
            (void)eq.AddBand({0, true, Audio::EQBandType::Bell,
                200.0f + update, static_cast<float>((update % 13) - 6), 1.0f, 12.0f});
            publicationsPassed = publicationsPassed &&
                static_cast<bool>(controller.ApplyParametricEQ(eq));
        }
    }
    Check(publicationsPassed, "frequent parameter and curve publication");
}

void MonitorFeedbackSafetyTests() {
    auto backend = std::make_unique<FakeBackend>();
    FakeBackend* fake = backend.get();
    fake->inputName = L"VoiceMeeter Output (VB-Audio VoiceMeeter VAIO)";
    fake->outputName = L"VoiceMeeter Input (VB-Audio VoiceMeeter VAIO)";
    VoiceMixerAudioController controller(std::move(backend));
    Check(static_cast<bool>(controller.RefreshDevices()), "feedback route devices refresh");
    XiE::VoiceMixer::VoiceMixerParameters parameters;
    parameters.monitor = true;
    controller.ApplyParameters(parameters);
    Check(static_cast<bool>(controller.Start()), "feedback route start");
    Check(controller.MonitorSafetyActive(), "feedback route monitor safety active");
    fake->input->Emit();
    Check(fake->output->Render() <= 1.0e-6f,
        "feedback route monitor is muted");
    Check(static_cast<bool>(controller.Stop()), "feedback route stop");
    Check(!controller.MonitorSafetyActive(), "feedback route safety clears on stop");
}

void ParametricEQEditorTests() {
    Engine::UI::UIParametricEQEditor editor;
    editor.SetBounds({10, 20, 900, 420});
    const float frequency = 1'000.0f;
    const float gain = 6.0f;
    Check(std::abs(editor.XToFrequency(editor.FrequencyToX(frequency)) - frequency) < 0.1f,
        "EQ editor logarithmic frequency mapping roundtrip");
    Check(std::abs(editor.YToGain(editor.GainToY(gain)) - gain) < 0.01f,
        "EQ editor gain mapping roundtrip");
    int changes = 0;
    editor.SetChangedCallback([&] { ++changes; });
    const Engine::Vec2F point{editor.FrequencyToX(1'000), editor.GainToY(3.0f)};
    Check(editor.PointerDown(point, false, 1.0), "EQ editor consumes first blank click");
    Check(editor.PointerDown(point, false, 1.2) && editor.Model().Bands().size() == 1,
        "EQ editor double click adds Bell band");
    editor.PointerUp();
    Check(editor.PointerDown(point, true, 2.0) &&
        editor.SelectedBand()->type == Audio::EQBandType::LowShelf,
        "EQ editor right click changes band type");
    Check(editor.PointerWheel(point, 1.0f) && editor.SelectedBand()->q > 0.707f,
        "EQ editor wheel adjusts Q");
    Check(editor.PointerDown(point, false, 3.0), "EQ editor selects band");
    Check(editor.PointerMove({editor.FrequencyToX(2'000), editor.GainToY(-4.0f)}),
        "EQ editor drag changes frequency and gain");
    editor.PointerUp();
    Check(editor.KeyDown(Engine::InputKeyCode::Delete) && editor.Model().Bands().empty(),
        "EQ editor Delete removes selected band");
    Check(changes >= 4, "EQ editor change notifications");
    Engine::DrawList2D list;
    editor.Draw(list);
    Check(!list.Empty(), "EQ editor draws spectrum grid and response surface");

    Engine::Audio::ParametricEQModel dense;
    for (std::size_t band = 0; band < Engine::Audio::kParametricEQMaxBands; ++band) {
        (void)dense.AddBand({0, true, Audio::EQBandType::Bell,
            40.0f * std::pow(1.45f, static_cast<float>(band)),
            band % 2 == 0 ? 2.0f : -2.0f, 1.0f, 12.0f});
    }
    editor.SetModel(std::move(dense));
    Engine::DrawList2D benchmarkList;
    const auto begin = std::chrono::steady_clock::now();
    for (int frame = 0; frame < 100; ++frame) {
        benchmarkList.Clear();
        editor.Draw(benchmarkList);
    }
    const double averageMilliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count() / 100.0;
    std::cout << "UI EQ response/spectrum draw average_ms=" << averageMilliseconds
              << "; commands=" << benchmarkList.Commands().size() << '\n';
}

void UnifiedAudioEditorTests() {
    using Engine::UI::AudioEditorAxisBinding;
    using Engine::UI::AudioEditorAxisScale;
    using Engine::UI::AudioEditorTrack;
    using Engine::UI::AudioEditorTrackType;
    const Engine::RectF plot{10.0f, 20.0f, 800.0f, 360.0f};
    const AudioEditorAxisBinding voice{
        60.0f, 600.0f, -12.0f, 12.0f,
        AudioEditorAxisScale::Logarithmic, AudioEditorAxisScale::Linear};
    const AudioEditorAxisBinding eq{
        20.0f, 20'000.0f, -24.0f, 24.0f,
        AudioEditorAxisScale::Logarithmic, AudioEditorAxisScale::Linear};
    Check(std::abs(voice.FromScreenX(plot, voice.ToScreenX(plot, 220.0f)) - 220.0f) < 0.01f,
        "unified editor pitch F0 mapping roundtrip");
    Check(std::abs(eq.FromScreenX(plot, eq.ToScreenX(plot, 2'000.0f)) - 2'000.0f) < 0.1f,
        "unified editor EQ frequency mapping roundtrip");
    Check(std::abs(eq.FromScreenY(plot, eq.ToScreenY(plot, 6.0f)) - 6.0f) < 0.01f,
        "unified editor EQ gain mapping roundtrip");

    auto pitchAdapter = std::make_shared<TestAudioTrackAdapter>(
        std::vector<TestAudioTrackAdapter::Node>{{1, 180.0f, 2.0f}});
    auto formantAdapter = std::make_shared<TestAudioTrackAdapter>(
        std::vector<TestAudioTrackAdapter::Node>{{2, 260.0f, -2.0f}});
    auto eqAdapter = std::make_shared<TestAudioTrackAdapter>(
        std::vector<TestAudioTrackAdapter::Node>{{3, 1'000.0f, 4.0f}});
    Engine::UI::UIAudioCurveEditor editor;
    editor.SetBounds(plot);
    editor.SetTracks({
        {"pitch", "Pitch", AudioEditorTrackType::Pitch, voice, {}, true, true, 20, pitchAdapter},
        {"formant", "Formant", AudioEditorTrackType::Formant, voice, {}, true, true, 30, formantAdapter},
        {"eq", "EQ", AudioEditorTrackType::ParametricEQ, eq, {}, true, true, 10, eqAdapter}});
    editor.SetActiveTrack(0);
    const Engine::Vec2F pitchPoint = voice.ToScreen(plot, 180.0f, 2.0f);
    Check(editor.PointerDown(pitchPoint, false, 1.0), "unified pitch node hit test");
    Check(editor.Selection().trackIndex == 0 && editor.Selection().nodeId == 1,
        "unified pitch selection");
    Check(editor.PointerMove({plot.x + plot.width + 50.0f, plot.y + 30.0f}),
        "unified pointer capture drags outside bounds");
    editor.PointerUp();

    const Engine::Vec2F formantPoint = voice.ToScreen(plot, 260.0f, -2.0f);
    Check(editor.PointerDown(formantPoint, false, 2.0),
        "unified non-active track hit auto activates");
    Check(editor.ActiveTrack() == 1 && editor.Selection().nodeId == 2,
        "unified non-active track selection");
    editor.PointerUp();
    const std::size_t beforeCreate = formantAdapter->nodes.size();
    const Engine::Vec2F blank = voice.ToScreen(plot, 340.0f, 0.0f);
    Check(editor.PointerDown(blank, false, 3.0) && editor.PointerDown(blank, false, 3.2),
        "unified active track double click creates point");
    Check(formantAdapter->nodes.size() == beforeCreate + 1,
        "unified creation is owned by active adapter");
    Check(editor.KeyDown(Engine::InputKeyCode::Delete), "unified Delete removes selection");
    editor.ClearSelection();
    Check(!editor.Selection().IsValid(), "unified Escape/clear selection");
    Check(editor.PointerDown({plot.x + 400.0f, plot.y + 100.0f}, false, 4.0),
        "unified grid consumes no node hit");
    Check(!editor.Selection().IsValid(), "grid and spectrum do not select nodes");
    (void)eqAdapter;
}

void EditorPageStressTests() {
    auto backend = std::make_unique<FakeBackend>();
    VoiceMixerAudioController controller(std::move(backend));
    const std::filesystem::path presetPath =
        std::filesystem::path(VOICEMIXER_TEST_ROOT) / "page-state" / "Presets" / "Current.json";
    XiE::VoiceMixer::VoiceMixerWindow window(controller, presetPath);
    Engine::PointerButtonEvent click;
    click.button = Engine::PointerButton::Primary;
    click.pressed = true;
    click.logicalPosition = {60, 183};
    window.HandleInput(Engine::InputEvent{click});
    click.logicalPosition = {82, 250};
    window.HandleInput(Engine::InputEvent{click});
    for (int iteration = 0; iteration < 1000; ++iteration) {
        const int page = iteration % 5;
        click.logicalPosition = page == 0 ? Engine::Vec2F{60, 183}
            : page == 1 ? Engine::Vec2F{165, 183}
            : page == 2 ? Engine::Vec2F{275, 183}
            : page == 3 ? Engine::Vec2F{390, 183}
            : Engine::Vec2F{500, 183};
        click.timestampSeconds = iteration * 0.01;
        window.HandleInput(Engine::InputEvent{click});
        window.Tick();
    }
    Check(window.EditorPage() == XiE::VoiceMixer::VoiceMixerWindow::VoiceEditorPage::FX,
        "1000 Noise/Pitch/Formant/EQ/FX page switches preserve explicit page state");
    Check(window.EQEditor().Model().Enabled(),
        "hidden EQ page does not bypass or destroy EQ model");
    Check(window.Preset().parameters.noiseEnabled &&
        controller.CaptureSnapshot().voice.noise.enabled,
        "hidden Noise page keeps noise configuration enabled");
}

void CurveEditorTests() {
    auto neutral = XiE::VoiceMixer::MakeNeutralPreset();
    XiE::VoiceMixer::CurveEditorModel model;
    model.domainMinimum = 70; model.domainMaximum = 500;
    model.rangeMinimum = -12; model.rangeMaximum = 12;
    model.tracks.push_back({"pitch", "Pitch", neutral.pitchCurve, {0, 1, 1, 1}, true});
    model.tracks.push_back({"overlay", "Overlay", neutral.pitchCurve, {1, 0, 1, 1}, true});
    UICurveEditor editor(std::move(model));
    editor.SetBounds({0, 0, 430, 240});
    int changes = 0;
    editor.SetChangedCallback([&](std::size_t) { ++changes; });
    Check(editor.Model().tracks.size() == 2, "curve editor supports multiple overlay tracks");
    Check(editor.PointerDown({215, 80}, false), "curve editor adds point");
    Check(editor.PointerMove({250, 100}), "curve editor drags selected point");
    editor.PointerUp();
    Check(changes >= 2, "curve editor publishes edit notifications");
    const std::size_t overlayPoints = editor.Model().tracks[1].curve.Points().size();
    editor.SetActiveTrack(1);
    Check(editor.ActiveTrack() == 1, "curve editor selects active overlay track");
    Check(editor.PointerDown({140, 45}, false), "curve editor adds to active overlay track");
    editor.PointerUp();
    Check(editor.Model().tracks[1].curve.Points().size() == overlayPoints + 1,
        "new curve point belongs to selected overlay track");
    Check(editor.BuildSnapshot(0) != nullptr, "curve editor rebuilds LUT snapshot");
    Engine::DrawList2D list;
    editor.Draw(list);
    Check(!list.Empty(), "curve editor draws axes curve and control points");
}

void PresetTests() {
    const std::filesystem::path root = VOICEMIXER_TEST_ROOT;
    const std::filesystem::path path = root / "preset.json";
    std::filesystem::create_directories(root);
    VoicePresetRepository repository;
    VoicePreset preset = XiE::VoiceMixer::MakeNeutralPreset();
    preset.parameters.wet = 0.42f;
    preset.parameters.pitchSemitones = 3.0f;
    preset.parameters.noiseEnabled = true;
    preset.parameters.noiseBypassed = true;
    preset.parameters.noiseStrength = 0.63f;
    preset.parameters.noiseWet = 0.81f;
    preset.parameters.softHaze.enabled = true;
    preset.parameters.softHaze.amount = 0.73f;
    preset.parameters.softHaze.blur = 0.44f;
    preset.parameters.spatialHaze.enabled = true;
    preset.parameters.spatialHaze.width = 0.91f;
    preset.parameters.spatialHaze.wet = 0.18f;
    preset.parameters.syntheticVoice.enabled = true;
    preset.parameters.syntheticVoice.amount = 0.37f;
    preset.parameters.syntheticVoice.stability = 0.82f;
    preset.parameters.syntheticVoice.electronic = 0.24f;
    preset.parameters.syntheticVoice.width = 0.64f;
    preset.parameters.syntheticVoice.wet = 0.77f;
    (void)preset.eq.AddBand({7, true, Audio::EQBandType::Bell, 1'200.0f, 4.0f, 1.2f, 12.0f});
    std::string error;
    Check(repository.Save(path, preset, error), "preset save");

    VoicePreset loaded;
    Check(repository.Load(path, loaded, error), "preset load");
    Check(std::abs(loaded.parameters.wet - 0.42f) < 0.001f, "preset parameter roundtrip");
    Check(loaded.pitchCurve.Points().size() == preset.pitchCurve.Points().size(),
        "preset curve roundtrip");
    Check(loaded.eq.Bands().size() == 1 && loaded.eq.Bands()[0].id == 7,
        "preset EQ roundtrip with stable BandId");
    Check(loaded.parameters.noiseEnabled && loaded.parameters.noiseBypassed &&
        std::abs(loaded.parameters.noiseStrength - 0.63f) < 0.001f &&
        std::abs(loaded.parameters.noiseWet - 0.81f) < 0.001f,
        "preset V3 noise suppression roundtrip");
    Check(loaded.parameters.softHaze.enabled &&
        std::abs(loaded.parameters.softHaze.amount - 0.73f) < 0.001f &&
        std::abs(loaded.parameters.softHaze.blur - 0.44f) < 0.001f &&
        loaded.parameters.spatialHaze.enabled &&
        std::abs(loaded.parameters.spatialHaze.width - 0.91f) < 0.001f &&
        std::abs(loaded.parameters.spatialHaze.wet - 0.18f) < 0.001f,
        "preset V4 Soft Haze and Spatial Haze roundtrip");
    Check(loaded.parameters.syntheticVoice.enabled &&
        std::abs(loaded.parameters.syntheticVoice.amount - 0.37f) < 0.001f &&
        std::abs(loaded.parameters.syntheticVoice.stability - 0.82f) < 0.001f &&
        std::abs(loaded.parameters.syntheticVoice.electronic - 0.24f) < 0.001f &&
        std::abs(loaded.parameters.syntheticVoice.width - 0.64f) < 0.001f &&
        std::abs(loaded.parameters.syntheticVoice.wet - 0.77f) < 0.001f,
        "preset V5 Synthetic Voice roundtrip");

    std::ifstream savedInput(path, std::ios::binary);
    std::string document{
        std::istreambuf_iterator<char>(savedInput), std::istreambuf_iterator<char>()};
    savedInput.close();
    const std::size_t closingBrace = document.find_last_of('}');
    document.insert(closingBrace, ",\n  \"futureField\": {\"enabled\": true}\n");
    {
        std::ofstream futurePreset(path, std::ios::binary | std::ios::trunc);
        futurePreset << document;
    }
    Check(repository.Load(path, loaded, error), "preset with unknown field loads");
    Check(repository.Save(path, loaded, error), "preset with unknown field saves");
    std::ifstream preservedInput(path, std::ios::binary);
    const std::string preserved{
        std::istreambuf_iterator<char>(preservedInput), std::istreambuf_iterator<char>()};
    Check(preserved.find("\"futureField\"") != std::string::npos,
        "preset preserves unknown fields");

    const std::filesystem::path legacyPath = root / "legacy-v1.json";
    {
        std::ofstream output(legacyPath, std::ios::binary | std::ios::trunc);
        output << R"({
  "schemaVersion": 1,
  "name": "Legacy",
  "parameters": {"wet": 1, "bypass": false, "monitor": true},
  "pitchCurve": [
    {"id": 1, "x": 70, "y": 0, "interpolation": "smooth"},
    {"id": 2, "x": 500, "y": 0, "interpolation": "smooth"}
  ],
  "formantCurve": [
    {"id": 1, "x": 70, "y": 0, "interpolation": "smooth"},
    {"id": 2, "x": 500, "y": 0, "interpolation": "smooth"}
  ],
  "unknownV1Field": 42
})";
    }
    VoicePreset legacyPreset;
    Check(repository.Load(legacyPath, legacyPreset, error) && legacyPreset.eq.Bands().empty() &&
        !legacyPreset.parameters.noiseEnabled && legacyPreset.parameters.noiseStrength == 1.0f &&
        legacyPreset.parameters.noiseWet == 1.0f &&
        !legacyPreset.parameters.softHaze.enabled &&
        !legacyPreset.parameters.spatialHaze.enabled &&
        !legacyPreset.parameters.syntheticVoice.enabled &&
        legacyPreset.parameters.syntheticVoice.amount == 0.0f,
        "legacy preset without EQ/noise/haze/synthetic migrates to neutral disabled effects");
}

} // namespace

int main() {
    ControllerTests();
    MonitorFeedbackSafetyTests();
    CurveEditorTests();
    ParametricEQEditorTests();
    UnifiedAudioEditorTests();
    EditorPageStressTests();
    PresetTests();
    std::cout << (failures == 0 ? "VoiceMixer tests passed\n" : "VoiceMixer tests failed\n");
    return failures == 0 ? 0 : 1;
}
