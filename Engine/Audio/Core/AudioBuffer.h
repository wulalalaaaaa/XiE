#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace Engine::Audio {

class AudioBufferView {
public:
    AudioBufferView() = default;
    AudioBufferView(float* data, std::size_t channels, std::size_t frames, std::size_t stride) noexcept
        : m_Data(data), m_Channels(channels), m_Frames(frames), m_Stride(stride) {}

    [[nodiscard]] std::size_t ChannelCount() const noexcept { return m_Channels; }
    [[nodiscard]] std::size_t FrameCount() const noexcept { return m_Frames; }
    [[nodiscard]] std::size_t ChannelStride() const noexcept { return m_Stride; }
    [[nodiscard]] float* Data() const noexcept { return m_Data; }
    [[nodiscard]] bool Empty() const noexcept { return m_Data == nullptr || m_Frames == 0; }
    [[nodiscard]] std::span<float> Channel(std::size_t channel) const noexcept;
    [[nodiscard]] float& Sample(std::size_t channel, std::size_t frame) const noexcept;
    void Clear() const noexcept;

private:
    float* m_Data = nullptr;
    std::size_t m_Channels = 0;
    std::size_t m_Frames = 0;
    std::size_t m_Stride = 0;
};

class ConstAudioBufferView {
public:
    ConstAudioBufferView() = default;
    ConstAudioBufferView(const float* data, std::size_t channels, std::size_t frames, std::size_t stride) noexcept
        : m_Data(data), m_Channels(channels), m_Frames(frames), m_Stride(stride) {}
    ConstAudioBufferView(AudioBufferView view) = delete;

    [[nodiscard]] std::size_t ChannelCount() const noexcept { return m_Channels; }
    [[nodiscard]] std::size_t FrameCount() const noexcept { return m_Frames; }
    [[nodiscard]] std::size_t ChannelStride() const noexcept { return m_Stride; }
    [[nodiscard]] const float* Data() const noexcept { return m_Data; }
    [[nodiscard]] std::span<const float> Channel(std::size_t channel) const noexcept;
    [[nodiscard]] float Sample(std::size_t channel, std::size_t frame) const noexcept;

private:
    const float* m_Data = nullptr;
    std::size_t m_Channels = 0;
    std::size_t m_Frames = 0;
    std::size_t m_Stride = 0;
};

class AudioBuffer {
public:
    AudioBuffer() = default;
    AudioBuffer(std::size_t channels, std::size_t frames);

    void Resize(std::size_t channels, std::size_t frames);
    [[nodiscard]] std::size_t ChannelCount() const noexcept { return m_Channels; }
    [[nodiscard]] std::size_t FrameCount() const noexcept { return m_Frames; }
    [[nodiscard]] AudioBufferView View() noexcept;
    [[nodiscard]] ConstAudioBufferView View() const noexcept;
    void Clear() noexcept;
    bool CopyFrom(ConstAudioBufferView source) noexcept;
    bool CopyFrom(AudioBufferView source) noexcept;

private:
    std::size_t m_Channels = 0;
    std::size_t m_Frames = 0;
    std::vector<float> m_Data;
};

class AudioBufferPool {
public:
    void Prepare(std::size_t bufferCount, std::size_t channels, std::size_t frames);
    void Reset() noexcept { m_Next = 0; }
    [[nodiscard]] AudioBufferView Acquire() noexcept;

private:
    std::vector<AudioBuffer> m_Buffers;
    std::size_t m_Next = 0;
};

struct AudioProcessContext {
    AudioBufferView input;
    AudioBufferView output;
    std::uint32_t sampleRate = 48'000;
    std::uint64_t absoluteFrame = 0;
};

} // namespace Engine::Audio
