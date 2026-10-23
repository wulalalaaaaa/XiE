#include "XSI/SensoryEventChannel.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <latch>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

XSI::SensoryEvent MakeFrame(std::uint64_t sequence, unsigned producer = 0) {
    XSI::SensoryEvent event;
    event.source = {"benchmark", "camera-" + std::to_string(producer)};
    event.channel = "benchmark-room";
    event.occurredTime = XSI::CurrentUtcTime();
    event.kind = XSI::EventKind::StreamData;
    event.streamId = "benchmark-stream-" + std::to_string(producer);
    event.sequence = sequence;
    event.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Video,
        XSI::MediaResourceRef::Adopt(std::vector<std::byte>(64, std::byte{0x2a})),
        "video/x-benchmark-frame",
        0,
        std::nullopt});
    return event;
}

XSI::SensoryEvent MakeControl(std::uint64_t sequence) {
    XSI::SensoryEvent event;
    event.source = {"benchmark", "control"};
    event.channel = "benchmark-room";
    event.occurredTime = XSI::CurrentUtcTime();
    event.parts.emplace_back(XSI::TextPart{"control-" + std::to_string(sequence)});
    return event;
}

double Percentile(std::vector<double>& samples, double percentile) {
    const auto index = static_cast<std::size_t>(
        percentile * static_cast<double>(samples.size() - 1));
    std::nth_element(samples.begin(), samples.begin() + index, samples.end());
    return samples[index];
}

void RunCase(std::size_t capacity, unsigned controlPercent) {
    XSI::SensoryEventChannel channel({capacity, XSI::QueuePolicy::LatestOnly, {}});
    std::uint64_t frameSequence = 1;
    std::uint64_t controlSequence = 1;
    for (std::size_t i = 0; i < capacity; ++i) {
        const bool control = ((i * 100) / capacity) % 100 < controlPercent;
        const auto result = control
            ? channel.TryPublish(MakeControl(controlSequence++))
            : channel.TryPublish(MakeFrame(frameSequence++));
        if (!result.Accepted()) {
            std::cerr << "queue seed failed capacity=" << capacity
                      << " controlPercent=" << controlPercent << '\n';
            return;
        }
    }

    const std::size_t iterations = capacity >= 65'536 ? 128 : 512;
    std::vector<double> latencyMicros;
    latencyMicros.reserve(iterations);
    const auto began = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        auto event = MakeFrame(frameSequence++);
        const auto operationBegan = Clock::now();
        const auto result = channel.TryPublish(std::move(event));
        const auto operationEnded = Clock::now();
        if (!result.Accepted()) {
            std::cerr << "replacement failed capacity=" << capacity
                      << " controlPercent=" << controlPercent << '\n';
            return;
        }
        latencyMicros.push_back(std::chrono::duration<double, std::micro>(
            operationEnded - operationBegan).count());
    }
    const auto elapsed = std::chrono::duration<double>(Clock::now() - began).count();
    auto p50 = latencyMicros;
    auto p95 = latencyMicros;
    auto p99 = latencyMicros;
    std::cout << "capacity=" << capacity
              << " controlPercent=" << controlPercent
              << " operations=" << iterations
              << " throughputPerSecond=" << iterations / elapsed
              << " publishLatencyUs[p50/p95/p99]="
              << Percentile(p50, 0.50) << '/'
              << Percentile(p95, 0.95) << '/'
              << Percentile(p99, 0.99) << '\n';
    channel.Close();
}

void RunHighWaterContentionCase(unsigned controlPercent) {
    constexpr std::size_t capacity = 65'536;
    constexpr unsigned producerCount = 4;
    constexpr std::size_t operationsPerProducer = 2'048;
    XSI::SensoryEventChannel channel({capacity, XSI::QueuePolicy::LatestOnly, {}});
    std::array<std::uint64_t, producerCount> nextSequence{};
    for (std::size_t i = 0; i < capacity; ++i) {
        const bool control = ((i * 100) / capacity) % 100 < controlPercent;
        const unsigned producer = static_cast<unsigned>(i % producerCount);
        const auto result = control
            ? channel.TryPublish(MakeControl(i + 1))
            : channel.TryPublish(MakeFrame(++nextSequence[producer], producer));
        if (!result.Accepted()) {
            std::cerr << "contention queue seed failed controlPercent="
                      << controlPercent << '\n';
            return;
        }
    }

    std::array<std::vector<double>, producerCount> latencies;
    for (auto& samples : latencies) samples.reserve(operationsPerProducer);
    std::atomic<std::size_t> failures{0};
    std::latch ready(producerCount);
    std::latch go(1);
    std::array<std::thread, producerCount> producers;
    for (unsigned producer = 0; producer < producerCount; ++producer) {
        producers[producer] = std::thread([&, producer] {
            ready.count_down();
            go.wait();
            auto& samples = latencies[producer];
            for (std::size_t i = 0; i < operationsPerProducer; ++i) {
                auto event = MakeFrame(++nextSequence[producer], producer);
                const auto operationBegan = Clock::now();
                const auto result = channel.TryPublish(std::move(event));
                const auto operationEnded = Clock::now();
                samples.push_back(std::chrono::duration<double, std::micro>(
                    operationEnded - operationBegan).count());
                if (!result.Accepted()) failures.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    ready.wait();
    const auto began = Clock::now();
    go.count_down();
    for (auto& producer : producers) producer.join();
    const auto elapsed = std::chrono::duration<double>(Clock::now() - began).count();

    std::vector<double> samples;
    samples.reserve(producerCount * operationsPerProducer);
    for (auto& producerSamples : latencies) {
        samples.insert(samples.end(), producerSamples.begin(), producerSamples.end());
    }
    auto p50 = samples;
    auto p95 = samples;
    auto p99 = samples;
    const auto operations = producerCount * operationsPerProducer;
    std::cout << "mode=4P-contention capacity=" << capacity
              << " controlPercent=" << controlPercent
              << " operations=" << operations
              << " failures=" << failures.load(std::memory_order_relaxed)
              << " throughputPerSecond=" << operations / elapsed
              << " publishLatencyUs[p50/p95/p99]="
              << Percentile(p50, 0.50) << '/'
              << Percentile(p95, 0.95) << '/'
              << Percentile(p99, 0.99) << '\n';
    channel.Close();
}

void RunTrackingSaturationFallbackCase() {
    constexpr std::size_t capacity = 65'536;
    constexpr std::size_t fallbackSearchLimit = 1'024;
    constexpr std::size_t iterations = 8;
    XSI::SensoryEventChannel channel({
        capacity, XSI::QueuePolicy::LatestOnly, {}, 1});
    if (!channel.TryPublish(MakeFrame(1, 0)).Accepted()) {
        std::cerr << "saturation queue seed failed at first stream frame\n";
        return;
    }
    for (std::size_t sequence = 1; sequence <= fallbackSearchLimit; ++sequence) {
        if (!channel.TryPublish(MakeFrame(sequence, 1)).Accepted()) {
            std::cerr << "saturation queue seed failed at second stream\n";
            return;
        }
    }
    std::uint64_t controlSequence = 1;
    while (channel.Stats().depth < capacity) {
        if (!channel.TryPublish(MakeControl(controlSequence++)).Accepted()) {
            std::cerr << "saturation queue seed failed at control event\n";
            return;
        }
    }
    if (channel.TryPublish(MakeFrame(2, 0)).status !=
        XSI::PublishStatus::AcceptedReplacingStaleFrame) {
        std::cerr << "saturation queue failed to fill dropped-frame tracking\n";
        return;
    }

    std::vector<double> latencyMicros;
    latencyMicros.reserve(iterations);
    const auto began = Clock::now();
    for (std::size_t index = 0; index < iterations; ++index) {
        auto event = MakeFrame(index + 3, 0);
        const auto operationBegan = Clock::now();
        const auto result = channel.TryPublish(std::move(event));
        const auto operationEnded = Clock::now();
        if (result.status != XSI::PublishStatus::Full ||
            !result.rejectedEvent.has_value()) {
            std::cerr << "saturation bounded search did not retain backpressure event\n";
            return;
        }
        latencyMicros.push_back(std::chrono::duration<double, std::micro>(
            operationEnded - operationBegan).count());
    }
    const auto elapsed = std::chrono::duration<double>(Clock::now() - began).count();
    auto p50 = latencyMicros;
    auto p95 = latencyMicros;
    auto p99 = latencyMicros;
    std::cout << "mode=tracking-saturation-fallback capacity=" << capacity
              << " trackedStreams=1 queuedUntrackedFrames=" << fallbackSearchLimit
              << " laterTrackableCandidateBeyondSearchLimit=1"
              << " rejectedAndRetained=" << iterations
              << " operations=" << iterations
              << " throughputPerSecond=" << iterations / elapsed
              << " publishLatencyUs[p50/p95/p99]="
              << Percentile(p50, 0.50) << '/'
              << Percentile(p95, 0.95) << '/'
              << Percentile(p99, 0.99) << '\n';
    channel.Close();
}

} // namespace

int main() {
    constexpr std::array<std::size_t, 4> capacities{128, 1024, 8192, 65'536};
    constexpr std::array<unsigned, 3> controlPercentages{0, 10, 50};
    for (const auto capacity : capacities) {
        for (const auto controlPercent : controlPercentages) {
            RunCase(capacity, controlPercent);
        }
    }
    for (const auto controlPercent : controlPercentages) {
        RunHighWaterContentionCase(controlPercent);
    }
    RunTrackingSaturationFallbackCase();
}
