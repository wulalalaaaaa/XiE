#include "Audio/Analysis/SpectrumAnalyzer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>

namespace Engine::Audio {

SpectrumAnalyzer::~SpectrumAnalyzer() {
    StopWorker();
}

void SpectrumAnalyzer::SampleRing::Prepare(std::size_t capacity) {
    m_Capacity = std::max<std::size_t>(2, capacity + 1);
    m_Data.assign(m_Capacity, 0.0f);
    Reset();
}

void SpectrumAnalyzer::SampleRing::Reset() noexcept {
    m_Read.store(0, std::memory_order_release);
    m_Write.store(0, std::memory_order_release);
}

std::size_t SpectrumAnalyzer::SampleRing::Push(ConstAudioBufferView input) noexcept {
    if (m_Capacity == 0 || input.ChannelCount() == 0) return 0;
    std::size_t write = m_Write.load(std::memory_order_relaxed);
    const std::size_t read = m_Read.load(std::memory_order_acquire);
    std::size_t pushed = 0;
    for (; pushed < input.FrameCount(); ++pushed) {
        const std::size_t next = (write + 1) % m_Capacity;
        if (next == read) break;
        float mono = 0.0f;
        for (std::size_t channel = 0; channel < input.ChannelCount(); ++channel) {
            mono += input.Sample(channel, pushed);
        }
        m_Data[write] = mono / static_cast<float>(input.ChannelCount());
        write = next;
    }
    m_Write.store(write, std::memory_order_release);
    return pushed;
}

std::size_t SpectrumAnalyzer::SampleRing::Pop(float* destination, std::size_t count) noexcept {
    if (m_Capacity == 0 || destination == nullptr) return 0;
    std::size_t read = m_Read.load(std::memory_order_relaxed);
    const std::size_t write = m_Write.load(std::memory_order_acquire);
    std::size_t popped = 0;
    while (popped < count && read != write) {
        destination[popped++] = m_Data[read];
        read = (read + 1) % m_Capacity;
    }
    m_Read.store(read, std::memory_order_release);
    return popped;
}

std::size_t SpectrumAnalyzer::SampleRing::Available() const noexcept {
    if (m_Capacity == 0) return 0;
    const std::size_t read = m_Read.load(std::memory_order_acquire);
    const std::size_t write = m_Write.load(std::memory_order_acquire);
    return write >= read ? write - read : m_Capacity - read + write;
}

AudioStatus SpectrumAnalyzer::Prepare(const AudioFormat& format, SpectrumAnalyzerConfig config) {
    StopWorker();
    const bool powerOfTwo = config.fftSize >= 256 &&
        (config.fftSize & (config.fftSize - 1)) == 0;
    if (!format.IsValid() || !powerOfTwo || config.updateRateHz <= 0.0f ||
        config.minimumDecibels >= config.maximumDecibels) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_Format = format;
    m_Config = config;
    m_HopSize = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::lround(format.sampleRate / config.updateRateHz)),
        1, config.fftSize);
    m_InputRing.Prepare(config.fftSize * 8);
    m_OutputRing.Prepare(config.fftSize * 8);
    m_Hann = BuildHannWindow(config.fftSize);
    m_InputWindow.assign(config.fftSize, 0.0f);
    m_OutputWindow.assign(config.fftSize, 0.0f);
    m_InputScratch.assign(config.fftSize, 0.0f);
    m_OutputScratch.assign(config.fftSize, 0.0f);
    m_FftWork.assign(config.fftSize, {});
    const std::size_t binCount = config.fftSize / 2 + 1;
    m_SmoothedInput.assign(binCount, config.minimumDecibels);
    m_SmoothedOutput.assign(binCount, config.minimumDecibels);
    auto frequencies = std::make_shared<std::vector<float>>(binCount);
    for (std::size_t bin = 0; bin < binCount; ++bin) {
        (*frequencies)[bin] = static_cast<float>(bin) * format.sampleRate /
            static_cast<float>(config.fftSize);
    }
    m_FrequencyBins = std::move(frequencies);
    m_Latest.store({}, std::memory_order_release);
    m_DroppedWindows.store(0, std::memory_order_release);
    m_Revision.store(0, std::memory_order_release);
    m_WorkerMilliseconds.store(0.0, std::memory_order_release);
    m_MaximumWorkerMilliseconds.store(0.0, std::memory_order_release);
    m_Primed.store(false, std::memory_order_release);
    m_StopRequested.store(false, std::memory_order_release);
    m_Enabled.store(config.enabled, std::memory_order_release);
    m_Worker = std::thread([this] { WorkerLoop(); });
    return {};
}

void SpectrumAnalyzer::StopWorker() noexcept {
    m_StopRequested.store(true, std::memory_order_release);
    if (m_Worker.joinable()) m_Worker.join();
}

void SpectrumAnalyzer::Reset() noexcept {
    m_InputRing.Reset();
    m_OutputRing.Reset();
    m_Primed.store(false, std::memory_order_release);
    m_Latest.store({}, std::memory_order_release);
    m_Revision.store(0, std::memory_order_release);
}

void SpectrumAnalyzer::PushInput(ConstAudioBufferView input) noexcept {
    if (!Enabled()) return;
    if (m_InputRing.Push(input) < input.FrameCount()) {
        m_DroppedWindows.fetch_add(1, std::memory_order_relaxed);
    }
}

void SpectrumAnalyzer::PushOutput(ConstAudioBufferView output) noexcept {
    if (!Enabled()) return;
    if (m_OutputRing.Push(output) < output.FrameCount()) {
        m_DroppedWindows.fetch_add(1, std::memory_order_relaxed);
    }
}

std::shared_ptr<const SpectrumSnapshot> SpectrumAnalyzer::CaptureSnapshot() const noexcept {
    return m_Latest.load(std::memory_order_acquire);
}

SpectrumAnalyzerMetrics SpectrumAnalyzer::CaptureMetrics() const noexcept {
    return {m_Config.fftSize, m_Config.updateRateHz,
        m_WorkerMilliseconds.load(std::memory_order_acquire),
        m_MaximumWorkerMilliseconds.load(std::memory_order_acquire),
        m_DroppedWindows.load(std::memory_order_acquire),
        m_Revision.load(std::memory_order_acquire)};
}

std::vector<float> SpectrumAnalyzer::BuildHannWindow(std::size_t size) {
    std::vector<float> result(size, 0.0f);
    if (size <= 1) return result;
    for (std::size_t index = 0; index < size; ++index) {
        result[index] = 0.5f - 0.5f * std::cos(
            2.0f * std::numbers::pi_v<float> * static_cast<float>(index) /
            static_cast<float>(size - 1));
    }
    return result;
}

void SpectrumAnalyzer::Transform(std::vector<std::complex<float>>& values) noexcept {
    const std::size_t size = values.size();
    for (std::size_t i = 1, j = 0; i < size; ++i) {
        std::size_t bit = size >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }
    for (std::size_t length = 2; length <= size; length <<= 1) {
        const float angle = -2.0f * std::numbers::pi_v<float> /
            static_cast<float>(length);
        const std::complex<float> root(std::cos(angle), std::sin(angle));
        for (std::size_t offset = 0; offset < size; offset += length) {
            std::complex<float> factor(1.0f, 0.0f);
            for (std::size_t index = 0; index < length / 2; ++index) {
                const auto even = values[offset + index];
                const auto odd = values[offset + index + length / 2] * factor;
                values[offset + index] = even + odd;
                values[offset + index + length / 2] = even - odd;
                factor *= root;
            }
        }
    }
}

std::vector<float> SpectrumAnalyzer::AnalyzeWindow(
    const std::vector<float>& samples,
    const std::vector<float>& window,
    float minimumDecibels) {
    if (samples.empty() || samples.size() != window.size() ||
        (samples.size() & (samples.size() - 1)) != 0) return {};
    std::vector<std::complex<float>> work(samples.size());
    double windowSum = 0.0;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        work[index] = {samples[index] * window[index], 0.0f};
        windowSum += window[index];
    }
    Transform(work);
    const float scale = static_cast<float>(2.0 / std::max(1.0, windowSum));
    std::vector<float> result(samples.size() / 2 + 1, minimumDecibels);
    for (std::size_t bin = 0; bin < result.size(); ++bin) {
        const float magnitude = std::abs(work[bin]) * (bin == 0 ? scale * 0.5f : scale);
        result[bin] = std::max(minimumDecibels,
            20.0f * std::log10(std::max(1.0e-10f, magnitude)));
    }
    return result;
}

float SpectrumAnalyzer::SmoothMagnitudeDb(
    float currentDb, float targetDb, float attack, float release) noexcept {
    const float coefficient = targetDb >= currentDb ? attack : release;
    return currentDb + (targetDb - currentDb) * std::clamp(coefficient, 0.0f, 1.0f);
}

bool SpectrumAnalyzer::AdvanceWindows() noexcept {
    const bool primed = m_Primed.load(std::memory_order_acquire);
    const std::size_t needed = primed ? m_HopSize : m_Config.fftSize;
    if (m_InputRing.Available() < needed || m_OutputRing.Available() < needed) return false;
    if (!primed) {
        (void)m_InputRing.Pop(m_InputWindow.data(), m_Config.fftSize);
        (void)m_OutputRing.Pop(m_OutputWindow.data(), m_Config.fftSize);
        m_Primed.store(true, std::memory_order_release);
        return true;
    }
    std::move(m_InputWindow.begin() + static_cast<std::ptrdiff_t>(m_HopSize),
        m_InputWindow.end(), m_InputWindow.begin());
    std::move(m_OutputWindow.begin() + static_cast<std::ptrdiff_t>(m_HopSize),
        m_OutputWindow.end(), m_OutputWindow.begin());
    const std::size_t tail = m_Config.fftSize - m_HopSize;
    (void)m_InputRing.Pop(m_InputWindow.data() + tail, m_HopSize);
    (void)m_OutputRing.Pop(m_OutputWindow.data() + tail, m_HopSize);
    return true;
}

void SpectrumAnalyzer::Smooth(
    const std::vector<float>& source,
    std::vector<float>& destination) const noexcept {
    for (std::size_t index = 0; index < source.size(); ++index) {
        destination[index] = SmoothMagnitudeDb(destination[index], source[index],
            m_Config.attack, m_Config.release);
    }
}

void SpectrumAnalyzer::PublishAnalysis() noexcept {
    const std::vector<float> input = AnalyzeWindow(m_InputWindow, m_Hann, m_Config.minimumDecibels);
    const std::vector<float> output = AnalyzeWindow(m_OutputWindow, m_Hann, m_Config.minimumDecibels);
    Smooth(input, m_SmoothedInput);
    Smooth(output, m_SmoothedOutput);
    auto snapshot = std::make_shared<SpectrumSnapshot>();
    snapshot->frequencyBins = m_FrequencyBins;
    snapshot->inputMagnitudeDb = m_SmoothedInput;
    snapshot->outputMagnitudeDb = m_SmoothedOutput;
    snapshot->revision = m_Revision.fetch_add(1, std::memory_order_acq_rel) + 1;
    snapshot->timestampSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    snapshot->peakDb = output.empty() ? m_Config.minimumDecibels
        : *std::max_element(output.begin(), output.end());
    m_Latest.store(std::move(snapshot), std::memory_order_release);
}

void SpectrumAnalyzer::WorkerLoop() noexcept {
    while (!m_StopRequested.load(std::memory_order_acquire)) {
        if (!Enabled() || !AdvanceWindows()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        const auto begin = std::chrono::steady_clock::now();
        PublishAnalysis();
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        m_WorkerMilliseconds.store(elapsed, std::memory_order_relaxed);
        double maximum = m_MaximumWorkerMilliseconds.load(std::memory_order_relaxed);
        while (elapsed > maximum && !m_MaximumWorkerMilliseconds.compare_exchange_weak(
            maximum, elapsed, std::memory_order_relaxed)) {}
    }
}

} // namespace Engine::Audio
