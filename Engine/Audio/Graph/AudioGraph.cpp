#include "Audio/Graph/AudioGraph.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Engine::Audio {
namespace {

void Copy(AudioBufferView source, AudioBufferView destination) noexcept {
    const std::size_t channels = std::min(source.ChannelCount(), destination.ChannelCount());
    const std::size_t frames = std::min(source.FrameCount(), destination.FrameCount());
    for (std::size_t channel = 0; channel < channels; ++channel) {
        std::copy_n(source.Channel(channel).data(), frames, destination.Channel(channel).data());
    }
    for (std::size_t channel = channels; channel < destination.ChannelCount(); ++channel) {
        std::fill_n(destination.Channel(channel).data(), frames, 0.0f);
    }
}

} // namespace

AudioGraphSnapshot::AudioGraphSnapshot(
    AudioFormat format,
    std::vector<std::unique_ptr<IAudioProcessor>> processors)
    : m_Format(format), m_Processors(std::move(processors)) {}

AudioStatus AudioGraphSnapshot::Prepare() {
    if (!m_Format.IsValid()) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    for (const auto& processor : m_Processors) {
        AudioStatus result = processor->Prepare(m_Format);
        if (!result) {
            return result;
        }
    }
    m_Prepared = true;
    return {};
}

void AudioGraphSnapshot::Process(AudioProcessContext& context) noexcept {
    if (!m_Prepared) {
        context.output.Clear();
        return;
    }
    Copy(context.input, context.output);
    AudioProcessContext inPlace = context;
    inPlace.input = context.output;
    inPlace.output = context.output;
    for (const auto& processor : m_Processors) {
        if (!processor->Bypassed()) {
            processor->Process(inPlace);
        }
    }
}

void AudioGraphSnapshot::Reset() noexcept {
    for (const auto& processor : m_Processors) {
        processor->Reset();
    }
}

IAudioProcessor* AudioGraphSnapshot::Processor(std::size_t index) noexcept {
    return index < m_Processors.size() ? m_Processors[index].get() : nullptr;
}

std::string AudioGraphSnapshot::Describe() const {
    std::ostringstream output;
    output << "AudioGraph " << m_Format.sampleRate << " Hz, "
           << m_Format.channelCount << " ch, " << m_Processors.size() << " processors\n";
    for (std::size_t i = 0; i < m_Processors.size(); ++i) {
        output << i << ": " << m_Processors[i]->Name()
               << (m_Processors[i]->Bypassed() ? " [bypassed]" : "") << '\n';
    }
    return output.str();
}

AudioGraphBuilder& AudioGraphBuilder::Add(std::unique_ptr<IAudioProcessor> processor) {
    if (processor) {
        m_Processors.push_back(std::move(processor));
    }
    return *this;
}

std::unique_ptr<AudioGraphSnapshot> AudioGraphBuilder::Build(
    const AudioFormat& format,
    AudioStatus* status) {
    auto snapshot = std::make_unique<AudioGraphSnapshot>(format, std::move(m_Processors));
    const AudioStatus result = snapshot->Prepare();
    if (status) {
        *status = result;
    }
    if (!result) {
        return {};
    }
    return snapshot;
}

AudioStatus AudioGraph::Prepare(const AudioFormat& format, std::uint32_t crossfadeFrames) {
    if (!format.IsValid()) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_Format = format;
    m_TransitionBuffer.Resize(format.channelCount, format.framesPerBuffer);
    m_CrossfadeFrames = std::max<std::uint32_t>(1, crossfadeFrames);
    m_CrossfadePosition = m_CrossfadeFrames;
    m_Prepared = true;
    return {};
}

AudioStatus AudioGraph::Publish(std::unique_ptr<AudioGraphSnapshot> snapshot) {
    if (!m_Prepared || !snapshot ||
        snapshot->Format().sampleRate != m_Format.sampleRate ||
        snapshot->Format().channelCount != m_Format.channelCount ||
        snapshot->Format().framesPerBuffer > m_Format.framesPerBuffer) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    AudioGraphSnapshot* raw = snapshot.get();
    m_RetainedSnapshots.push_back(std::move(snapshot));
    m_Current.store(raw, std::memory_order_release);
    return {};
}

void AudioGraph::Process(AudioProcessContext& context) noexcept {
    AudioGraphSnapshot* published = m_Current.load(std::memory_order_acquire);
    if (!published) {
        Copy(context.input, context.output);
        return;
    }
    if (published != m_AudioSnapshot) {
        m_PreviousSnapshot = m_AudioSnapshot;
        m_AudioSnapshot = published;
        m_CrossfadePosition = 0;
    }

    m_AudioSnapshot->Process(context);
    if (!m_PreviousSnapshot || m_CrossfadePosition >= m_CrossfadeFrames) {
        return;
    }

    AudioBufferView transitionFull = m_TransitionBuffer.View();
    AudioBufferView transition(
        transitionFull.Channel(0).data(),
        transitionFull.ChannelCount(),
        context.output.FrameCount(),
        transitionFull.FrameCount());
    AudioProcessContext oldContext = context;
    oldContext.output = transition;
    m_PreviousSnapshot->Process(oldContext);

    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        const float mix = std::min(
            1.0f,
            static_cast<float>(m_CrossfadePosition + frame + 1) /
                static_cast<float>(m_CrossfadeFrames));
        for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
            context.output.Sample(channel, frame) =
                std::lerp(transition.Sample(channel, frame), context.output.Sample(channel, frame), mix);
        }
    }
    m_CrossfadePosition = std::min<std::uint32_t>(
        m_CrossfadeFrames,
        m_CrossfadePosition + static_cast<std::uint32_t>(context.output.FrameCount()));
    if (m_CrossfadePosition >= m_CrossfadeFrames) {
        m_PreviousSnapshot = nullptr;
    }
}

void AudioGraph::Reset() noexcept {
    for (const auto& snapshot : m_RetainedSnapshots) {
        snapshot->Reset();
    }
    m_PreviousSnapshot = nullptr;
    m_CrossfadePosition = m_CrossfadeFrames;
}

void AudioGraph::Stop() noexcept {
    m_Current.store(nullptr, std::memory_order_release);
    m_AudioSnapshot = nullptr;
    m_PreviousSnapshot = nullptr;
    m_RetainedSnapshots.clear();
    m_Prepared = false;
}

} // namespace Engine::Audio
