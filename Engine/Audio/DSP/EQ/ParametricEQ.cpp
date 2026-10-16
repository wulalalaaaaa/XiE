#include "Audio/DSP/EQ/ParametricEQ.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>

namespace Engine::Audio {
namespace {

constexpr double kMinimumMagnitude = 1.0e-12;

BiquadCoefficients Normalize(
    double b0, double b1, double b2, double a0, double a1, double a2) noexcept {
    if (!std::isfinite(a0) || std::abs(a0) < kMinimumMagnitude) {
        return BiquadCoefficients::Identity();
    }
    const double inverse = 1.0 / a0;
    BiquadCoefficients result{b0 * inverse, b1 * inverse, b2 * inverse,
        a1 * inverse, a2 * inverse};
    if (!std::isfinite(result.b0) || !std::isfinite(result.b1) ||
        !std::isfinite(result.b2) || !std::isfinite(result.a1) ||
        !std::isfinite(result.a2)) {
        return BiquadCoefficients::Identity();
    }
    return result;
}

BiquadCoefficients Difference(
    const BiquadCoefficients& target,
    const BiquadCoefficients& current,
    double divisor) noexcept {
    return {(target.b0 - current.b0) / divisor,
        (target.b1 - current.b1) / divisor,
        (target.b2 - current.b2) / divisor,
        (target.a1 - current.a1) / divisor,
        (target.a2 - current.a2) / divisor};
}

} // namespace

ParametricEQBand ClampParametricEQBand(ParametricEQBand band, float sampleRate) noexcept {
    const float nyquistLimit = std::max(kParametricEQMinimumFrequencyHz,
        std::min(kParametricEQMaximumFrequencyHz, sampleRate * 0.49f));
    band.frequencyHz = std::clamp(
        std::isfinite(band.frequencyHz) ? band.frequencyHz : 1'000.0f,
        kParametricEQMinimumFrequencyHz, nyquistLimit);
    band.gainDb = std::clamp(std::isfinite(band.gainDb) ? band.gainDb : 0.0f,
        kParametricEQMinimumGainDb, kParametricEQMaximumGainDb);
    band.q = std::clamp(std::isfinite(band.q) ? band.q : kParametricEQDefaultQ,
        kParametricEQMinimumQ, kParametricEQMaximumQ);
    band.slopeDbPerOctave = std::clamp(
        std::isfinite(band.slopeDbPerOctave) ? band.slopeDbPerOctave : 12.0f,
        kParametricEQMinimumSlopeDbPerOctave, kParametricEQMaximumSlopeDbPerOctave);
    return band;
}

std::optional<EQBandId> ParametricEQModel::AddBand(ParametricEQBand band) {
    if (m_Bands.size() >= kParametricEQMaxBands) return std::nullopt;
    if (band.id == 0) band.id = NextId();
    if (FindBand(band.id) != nullptr) return std::nullopt;
    band = ClampParametricEQBand(band);
    m_Bands.push_back(band);
    return band.id;
}

bool ParametricEQModel::UpdateBand(ParametricEQBand band) {
    ParametricEQBand* existing = FindBand(band.id);
    if (existing == nullptr) return false;
    *existing = ClampParametricEQBand(band);
    return true;
}

bool ParametricEQModel::RemoveBand(EQBandId id) noexcept {
    const auto found = std::find_if(m_Bands.begin(), m_Bands.end(),
        [id](const ParametricEQBand& band) { return band.id == id; });
    if (found == m_Bands.end()) return false;
    m_Bands.erase(found);
    return true;
}

ParametricEQBand* ParametricEQModel::FindBand(EQBandId id) noexcept {
    const auto found = std::find_if(m_Bands.begin(), m_Bands.end(),
        [id](const ParametricEQBand& band) { return band.id == id; });
    return found == m_Bands.end() ? nullptr : &*found;
}

const ParametricEQBand* ParametricEQModel::FindBand(EQBandId id) const noexcept {
    const auto found = std::find_if(m_Bands.begin(), m_Bands.end(),
        [id](const ParametricEQBand& band) { return band.id == id; });
    return found == m_Bands.end() ? nullptr : &*found;
}

EQBandId ParametricEQModel::NextId() const noexcept {
    EQBandId candidate = 1;
    for (const ParametricEQBand& band : m_Bands) candidate = std::max(candidate, band.id + 1);
    return candidate;
}

BiquadCoefficients CalculateParametricEQCoefficients(
    const ParametricEQBand& source,
    double sampleRate) noexcept {
    if (sampleRate <= 0.0 || !source.enabled) return BiquadCoefficients::Identity();
    const ParametricEQBand band = ClampParametricEQBand(source, static_cast<float>(sampleRate));
    const double omega = 2.0 * std::numbers::pi * band.frequencyHz / sampleRate;
    const double cosine = std::cos(omega);
    const double sine = std::sin(omega);
    const double alpha = sine / (2.0 * band.q);
    const double a = std::pow(10.0, static_cast<double>(band.gainDb) / 40.0);

    switch (band.type) {
    case EQBandType::Bell:
        return Normalize(1.0 + alpha * a, -2.0 * cosine, 1.0 - alpha * a,
            1.0 + alpha / a, -2.0 * cosine, 1.0 - alpha / a);
    case EQBandType::LowShelf: {
        const double root = 2.0 * std::sqrt(a) * alpha;
        return Normalize(a * ((a + 1.0) - (a - 1.0) * cosine + root),
            2.0 * a * ((a - 1.0) - (a + 1.0) * cosine),
            a * ((a + 1.0) - (a - 1.0) * cosine - root),
            (a + 1.0) + (a - 1.0) * cosine + root,
            -2.0 * ((a - 1.0) + (a + 1.0) * cosine),
            (a + 1.0) + (a - 1.0) * cosine - root);
    }
    case EQBandType::HighShelf: {
        const double root = 2.0 * std::sqrt(a) * alpha;
        return Normalize(a * ((a + 1.0) + (a - 1.0) * cosine + root),
            -2.0 * a * ((a - 1.0) + (a + 1.0) * cosine),
            a * ((a + 1.0) + (a - 1.0) * cosine - root),
            (a + 1.0) - (a - 1.0) * cosine + root,
            2.0 * ((a - 1.0) - (a + 1.0) * cosine),
            (a + 1.0) - (a - 1.0) * cosine - root);
    }
    case EQBandType::LowCut:
        return Normalize((1.0 + cosine) * 0.5, -(1.0 + cosine),
            (1.0 + cosine) * 0.5, 1.0 + alpha, -2.0 * cosine, 1.0 - alpha);
    case EQBandType::HighCut:
        return Normalize((1.0 - cosine) * 0.5, 1.0 - cosine,
            (1.0 - cosine) * 0.5, 1.0 + alpha, -2.0 * cosine, 1.0 - alpha);
    case EQBandType::Notch:
        return Normalize(1.0, -2.0 * cosine, 1.0,
            1.0 + alpha, -2.0 * cosine, 1.0 - alpha);
    }
    return BiquadCoefficients::Identity();
}

std::shared_ptr<const ParametricEQSnapshot> BuildParametricEQSnapshot(
    const ParametricEQModel& model,
    double sampleRate,
    std::uint64_t revision) {
    if (sampleRate <= 0.0) return {};
    auto snapshot = std::make_shared<ParametricEQSnapshot>();
    snapshot->enabled = model.Enabled();
    snapshot->sampleRate = sampleRate;
    snapshot->revision = revision;
    snapshot->bandCount = std::min(model.Bands().size(), kParametricEQMaxBands);
    for (std::size_t index = 0; index < snapshot->bandCount; ++index) {
        snapshot->bands[index] = ClampParametricEQBand(
            model.Bands()[index], static_cast<float>(sampleRate));
        snapshot->coefficients[index] = snapshot->enabled
            ? CalculateParametricEQCoefficients(snapshot->bands[index], sampleRate)
            : BiquadCoefficients::Identity();
    }
    return snapshot;
}

float EQResponseCalculator::MagnitudeDb(
    const BiquadCoefficients& coefficients,
    double sampleRate,
    double frequencyHz) noexcept {
    const double omega = 2.0 * std::numbers::pi * frequencyHz / sampleRate;
    const std::complex<double> z1 = std::polar(1.0, -omega);
    const std::complex<double> z2 = z1 * z1;
    const std::complex<double> numerator = coefficients.b0 +
        coefficients.b1 * z1 + coefficients.b2 * z2;
    const std::complex<double> denominator = 1.0 +
        coefficients.a1 * z1 + coefficients.a2 * z2;
    const double magnitude = std::abs(numerator / denominator);
    return static_cast<float>(20.0 * std::log10(std::max(kMinimumMagnitude, magnitude)));
}

EQResponseData EQResponseCalculator::Calculate(
    const ParametricEQModel& model,
    double sampleRate,
    std::size_t sampleCount,
    float minimumFrequencyHz,
    float maximumFrequencyHz) {
    EQResponseData result;
    sampleCount = std::max<std::size_t>(2, sampleCount);
    maximumFrequencyHz = std::min(maximumFrequencyHz, static_cast<float>(sampleRate * 0.49));
    minimumFrequencyHz = std::clamp(minimumFrequencyHz, 1.0f, maximumFrequencyHz);
    result.frequenciesHz.resize(sampleCount);
    result.combinedResponseDb.assign(sampleCount, 0.0f);
    result.bandResponseDb.resize(model.Bands().size(), std::vector<float>(sampleCount, 0.0f));
    const double ratio = maximumFrequencyHz / minimumFrequencyHz;
    for (std::size_t point = 0; point < sampleCount; ++point) {
        const double t = static_cast<double>(point) / static_cast<double>(sampleCount - 1);
        const float frequency = static_cast<float>(minimumFrequencyHz * std::pow(ratio, t));
        result.frequenciesHz[point] = frequency;
        if (!model.Enabled()) continue;
        for (std::size_t bandIndex = 0; bandIndex < model.Bands().size(); ++bandIndex) {
            const ParametricEQBand& band = model.Bands()[bandIndex];
            if (!band.enabled) continue;
            const float db = MagnitudeDb(
                CalculateParametricEQCoefficients(band, sampleRate), sampleRate, frequency);
            result.bandResponseDb[bandIndex][point] = db;
            result.combinedResponseDb[point] += db;
        }
    }
    return result;
}

AudioStatus ParametricEQProcessor::Prepare(const AudioFormat& format) {
    if (!format.IsValid() || format.channelCount > kParametricEQMaxChannels) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    m_Format = format;
    m_SmoothingFrames = std::max<std::uint32_t>(32, format.sampleRate / 500); // about 2 ms
    m_Prepared = true;
    Reset();
    return {};
}

AudioStatus ParametricEQProcessor::Publish(
    std::shared_ptr<const ParametricEQSnapshot> snapshot) {
    if (!snapshot || snapshot->sampleRate <= 0.0 ||
        snapshot->bandCount > kParametricEQMaxBands) {
        return {AudioStatusCode::InvalidArgument, 0};
    }
    const ParametricEQSnapshot* raw = snapshot.get();
    m_Enabled.store(raw->enabled, std::memory_order_release);
    m_BandCount.store(raw->bandCount, std::memory_order_release);
    m_Revision.store(raw->revision, std::memory_order_release);
    m_Retained.push_back(std::move(snapshot));
    m_Current.store(raw, std::memory_order_release);
    return {};
}

void ParametricEQProcessor::AdoptSnapshot(const ParametricEQSnapshot* snapshot) noexcept {
    m_AudioSnapshot = snapshot;
    const bool enabled = snapshot != nullptr && snapshot->enabled;
    const std::size_t bandCount = snapshot == nullptr ? 0 : snapshot->bandCount;
    for (std::size_t index = 0; index < kParametricEQMaxBands; ++index) {
        FilterState& filter = m_Filters[index];
        filter.target = enabled && index < bandCount
            ? snapshot->coefficients[index]
            : BiquadCoefficients::Identity();
        filter.step = Difference(filter.target, filter.current,
            static_cast<double>(m_SmoothingFrames));
        filter.remaining = m_SmoothingFrames;
    }
    m_Enabled.store(enabled, std::memory_order_relaxed);
    m_BandCount.store(bandCount, std::memory_order_relaxed);
    m_Revision.store(snapshot == nullptr ? 0 : snapshot->revision, std::memory_order_relaxed);
}

void ParametricEQProcessor::AdvanceCoefficients(FilterState& state) noexcept {
    if (state.remaining == 0) return;
    state.current.b0 += state.step.b0;
    state.current.b1 += state.step.b1;
    state.current.b2 += state.step.b2;
    state.current.a1 += state.step.a1;
    state.current.a2 += state.step.a2;
    if (--state.remaining == 0) state.current = state.target;
}

void ParametricEQProcessor::Process(AudioProcessContext& context) noexcept {
    const auto begin = std::chrono::steady_clock::now();
    if (!m_Prepared || Bypassed()) return;
    const ParametricEQSnapshot* published = m_Current.load(std::memory_order_acquire);
    if (published != m_AudioSnapshot) AdoptSnapshot(published);
    const std::size_t activeCount = m_AudioSnapshot == nullptr
        ? 0 : m_AudioSnapshot->bandCount;
    const std::size_t processCount = std::max<std::size_t>(activeCount,
        std::count_if(m_Filters.begin(), m_Filters.end(),
            [](const FilterState& state) { return state.remaining > 0; }));
    for (std::size_t frame = 0; frame < context.output.FrameCount(); ++frame) {
        for (std::size_t band = 0; band < std::min(processCount, kParametricEQMaxBands); ++band) {
            FilterState& filter = m_Filters[band];
            AdvanceCoefficients(filter);
            const BiquadCoefficients& c = filter.current;
            for (std::size_t channel = 0; channel < context.output.ChannelCount(); ++channel) {
                const double input = context.output.Sample(channel, frame);
                const double output = c.b0 * input + filter.z1[channel];
                filter.z1[channel] = c.b1 * input - c.a1 * output + filter.z2[channel];
                filter.z2[channel] = c.b2 * input - c.a2 * output;
                context.output.Sample(channel, frame) = static_cast<float>(output);
            }
        }
    }
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - begin).count();
    m_ProcessMilliseconds.store(elapsed, std::memory_order_relaxed);
    double maximum = m_MaximumProcessMilliseconds.load(std::memory_order_relaxed);
    while (elapsed > maximum && !m_MaximumProcessMilliseconds.compare_exchange_weak(
        maximum, elapsed, std::memory_order_relaxed)) {}
}

void ParametricEQProcessor::Reset() noexcept {
    for (FilterState& filter : m_Filters) {
        filter.current = BiquadCoefficients::Identity();
        filter.target = BiquadCoefficients::Identity();
        filter.step = {};
        filter.remaining = 0;
        filter.z1.fill(0.0);
        filter.z2.fill(0.0);
    }
    m_AudioSnapshot = nullptr;
    m_ProcessMilliseconds.store(0.0, std::memory_order_release);
    m_MaximumProcessMilliseconds.store(0.0, std::memory_order_release);
}

ParametricEQMetrics ParametricEQProcessor::CaptureMetrics() const noexcept {
    return {m_Enabled.load(std::memory_order_acquire),
        m_BandCount.load(std::memory_order_acquire),
        m_Revision.load(std::memory_order_acquire),
        m_ProcessMilliseconds.load(std::memory_order_acquire),
        m_MaximumProcessMilliseconds.load(std::memory_order_acquire)};
}

const char* ToString(EQBandType type) noexcept {
    switch (type) {
    case EQBandType::Bell: return "Bell";
    case EQBandType::LowShelf: return "Low Shelf";
    case EQBandType::HighShelf: return "High Shelf";
    case EQBandType::LowCut: return "Low Cut";
    case EQBandType::HighCut: return "High Cut";
    case EQBandType::Notch: return "Notch";
    }
    return "Bell";
}

} // namespace Engine::Audio
