#pragma once

#include "Audio/Graph/AudioProcessor.h"

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Engine::Audio {

class AudioGraphSnapshot {
public:
    AudioGraphSnapshot(AudioFormat format, std::vector<std::unique_ptr<IAudioProcessor>> processors);

    [[nodiscard]] AudioStatus Prepare();
    void Process(AudioProcessContext& context) noexcept;
    void Reset() noexcept;
    [[nodiscard]] const AudioFormat& Format() const noexcept { return m_Format; }
    [[nodiscard]] std::size_t ProcessorCount() const noexcept { return m_Processors.size(); }
    [[nodiscard]] IAudioProcessor* Processor(std::size_t index) noexcept;
    [[nodiscard]] std::string Describe() const;

private:
    AudioFormat m_Format{};
    std::vector<std::unique_ptr<IAudioProcessor>> m_Processors;
    bool m_Prepared = false;
};

class AudioGraphBuilder {
public:
    AudioGraphBuilder& Add(std::unique_ptr<IAudioProcessor> processor);
    [[nodiscard]] std::unique_ptr<AudioGraphSnapshot> Build(
        const AudioFormat& format,
        AudioStatus* status = nullptr);

private:
    std::vector<std::unique_ptr<IAudioProcessor>> m_Processors;
};

class AudioGraph {
public:
    AudioGraph() = default;

    AudioStatus Prepare(const AudioFormat& format, std::uint32_t crossfadeFrames = 64);
    AudioStatus Publish(std::unique_ptr<AudioGraphSnapshot> snapshot);
    void Process(AudioProcessContext& context) noexcept;
    void Reset() noexcept;
    void Stop() noexcept;

    [[nodiscard]] const AudioGraphSnapshot* CurrentSnapshot() const noexcept {
        return m_Current.load(std::memory_order_acquire);
    }

private:
    AudioFormat m_Format{};
    AudioBuffer m_TransitionBuffer;
    std::vector<std::unique_ptr<AudioGraphSnapshot>> m_RetainedSnapshots;
    std::atomic<AudioGraphSnapshot*> m_Current{nullptr};
    AudioGraphSnapshot* m_AudioSnapshot = nullptr;
    AudioGraphSnapshot* m_PreviousSnapshot = nullptr;
    std::uint32_t m_CrossfadeFrames = 64;
    std::uint32_t m_CrossfadePosition = 0;
    bool m_Prepared = false;
};

} // namespace Engine::Audio
