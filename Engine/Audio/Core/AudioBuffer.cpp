#include "Audio/Core/AudioBuffer.h"

#include <algorithm>
#include <cassert>

namespace Engine::Audio {

std::span<float> AudioBufferView::Channel(std::size_t channel) const noexcept {
    assert(channel < m_Channels);
    return {m_Data + channel * m_Stride, m_Frames};
}

float& AudioBufferView::Sample(std::size_t channel, std::size_t frame) const noexcept {
    assert(channel < m_Channels && frame < m_Frames);
    return m_Data[channel * m_Stride + frame];
}

void AudioBufferView::Clear() const noexcept {
    for (std::size_t channel = 0; channel < m_Channels; ++channel) {
        std::fill(Channel(channel).begin(), Channel(channel).end(), 0.0f);
    }
}

std::span<const float> ConstAudioBufferView::Channel(std::size_t channel) const noexcept {
    assert(channel < m_Channels);
    return {m_Data + channel * m_Stride, m_Frames};
}

float ConstAudioBufferView::Sample(std::size_t channel, std::size_t frame) const noexcept {
    assert(channel < m_Channels && frame < m_Frames);
    return m_Data[channel * m_Stride + frame];
}

AudioBuffer::AudioBuffer(std::size_t channels, std::size_t frames) {
    Resize(channels, frames);
}

void AudioBuffer::Resize(std::size_t channels, std::size_t frames) {
    m_Channels = channels;
    m_Frames = frames;
    m_Data.assign(channels * frames, 0.0f);
}

AudioBufferView AudioBuffer::View() noexcept {
    return {m_Data.data(), m_Channels, m_Frames, m_Frames};
}

ConstAudioBufferView AudioBuffer::View() const noexcept {
    return {m_Data.data(), m_Channels, m_Frames, m_Frames};
}

void AudioBuffer::Clear() noexcept {
    std::fill(m_Data.begin(), m_Data.end(), 0.0f);
}

bool AudioBuffer::CopyFrom(ConstAudioBufferView source) noexcept {
    if (source.ChannelCount() != m_Channels || source.FrameCount() != m_Frames) {
        return false;
    }
    for (std::size_t channel = 0; channel < m_Channels; ++channel) {
        std::copy(source.Channel(channel).begin(), source.Channel(channel).end(), View().Channel(channel).begin());
    }
    return true;
}

bool AudioBuffer::CopyFrom(AudioBufferView source) noexcept {
    if (source.ChannelCount() != m_Channels || source.FrameCount() != m_Frames) {
        return false;
    }
    for (std::size_t channel = 0; channel < m_Channels; ++channel) {
        std::copy(source.Channel(channel).begin(), source.Channel(channel).end(), View().Channel(channel).begin());
    }
    return true;
}

void AudioBufferPool::Prepare(std::size_t bufferCount, std::size_t channels, std::size_t frames) {
    m_Buffers.clear();
    m_Buffers.reserve(bufferCount);
    for (std::size_t i = 0; i < bufferCount; ++i) {
        m_Buffers.emplace_back(channels, frames);
    }
    m_Next = 0;
}

AudioBufferView AudioBufferPool::Acquire() noexcept {
    if (m_Next >= m_Buffers.size()) {
        return {};
    }
    AudioBufferView result = m_Buffers[m_Next++].View();
    result.Clear();
    return result;
}

} // namespace Engine::Audio
