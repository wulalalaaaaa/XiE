#pragma once

#include "Audio/DSP/BasicProcessors.h"
#include "Curve/Curve.h"

#include <atomic>
#include <memory>
#include <vector>

namespace Engine::Audio {

class IFormantProcessor {
public:
    virtual ~IFormantProcessor() = default;
    virtual AudioStatus PrepareFormant(const AudioFormat& format) = 0;
    virtual void SetFormantSemitones(float semitones) noexcept = 0;
    virtual void ProcessFormant(AudioProcessContext& context) noexcept = 0;
    virtual void ResetFormant() noexcept = 0;
    [[nodiscard]] virtual bool Experimental() const noexcept = 0;
};

// Experimental low-cost spectral-warp processor. The interface is intentionally
// independent from pitch shifting so a higher-quality LPC/vocoder implementation
// can replace it without changing VoiceProcessor.
class FormantShiftProcessor final : public ProcessorBase, public IFormantProcessor {
public:
    AudioStatus Prepare(const AudioFormat& format) override { return PrepareFormant(format); }
    AudioStatus PrepareFormant(const AudioFormat& format) override;
    void SetFormantSemitones(float semitones) noexcept override;
    void Process(AudioProcessContext& context) noexcept override { ProcessFormant(context); }
    void ProcessFormant(AudioProcessContext& context) noexcept override;
    void Reset() noexcept override { ResetFormant(); }
    void ResetFormant() noexcept override;
    [[nodiscard]] bool Experimental() const noexcept override { return true; }
    [[nodiscard]] const char* Name() const noexcept override { return "FormantShiftExperimental"; }

private:
    std::atomic<float> m_TargetSemitones{0.0f};
    ParameterSmoother m_AmountSmoother;
    std::vector<float> m_PreviousInputA;
    std::vector<float> m_PreviousOutputA;
    std::vector<float> m_PreviousInputB;
    std::vector<float> m_PreviousOutputB;
};

class FormantCurve {
public:
    AudioStatus Publish(std::shared_ptr<const Curve::CurveSnapshot> snapshot);
    [[nodiscard]] float Evaluate(float frequencyHz, float fallbackSemitones = 0.0f) const noexcept;

private:
    std::vector<std::shared_ptr<const Curve::CurveSnapshot>> m_Retained;
    std::atomic<const Curve::CurveSnapshot*> m_Current{nullptr};
};

} // namespace Engine::Audio
