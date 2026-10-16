#pragma once

#include "Audio/Core/AudioBuffer.h"
#include "Audio/Core/AudioTypes.h"

namespace Engine::Audio {

class IAudioProcessor {
public:
    virtual ~IAudioProcessor() = default;
    virtual AudioStatus Prepare(const AudioFormat& format) = 0;
    virtual void Process(AudioProcessContext& context) noexcept = 0;
    virtual void Reset() noexcept = 0;
    virtual void SetBypassed(bool bypassed) noexcept = 0;
    [[nodiscard]] virtual bool Bypassed() const noexcept = 0;
    [[nodiscard]] virtual const char* Name() const noexcept = 0;
};

} // namespace Engine::Audio
