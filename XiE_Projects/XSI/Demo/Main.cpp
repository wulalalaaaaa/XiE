#include "XSI/SensoryAdapter.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <thread>

namespace {

std::int64_t SteadyNowNanoseconds() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

const char* EventKindName(XSI::EventKind kind) {
    switch (kind) {
    case XSI::EventKind::Discrete: return "discrete";
    case XSI::EventKind::StreamStarted: return "stream-start";
    case XSI::EventKind::StreamData: return "stream-data";
    case XSI::EventKind::StreamEnded: return "stream-end";
    }
    return "unknown";
}

const char* MediaKindName(XSI::MediaKind kind) {
    switch (kind) {
    case XSI::MediaKind::Image: return "image";
    case XSI::MediaKind::Audio: return "audio";
    case XSI::MediaKind::Video: return "video";
    case XSI::MediaKind::Haptic: return "haptic";
    case XSI::MediaKind::Extension: return "extension";
    }
    return "unknown";
}

int ParseRunSeconds(int argc, char** argv) {
    if (argc < 2) return 2;
    try {
        const int value = std::stoi(argv[1]);
        return std::clamp(value, 1, 60);
    } catch (...) {
        return 2;
    }
}

} // namespace

int main(int argc, char** argv) {
    XSI::QueueOptions options;
    options.capacity = 16;
    options.policy = XSI::QueuePolicy::LatestOnly;
    XSI::SensoryEventChannel channel(options);

    std::atomic<std::uint64_t> eventCount{0};
    std::atomic<std::uint64_t> mediaCount{0};
    std::atomic<std::uint64_t> lifecycleErrors{0};
    std::atomic<std::uint64_t> activeStreams{0};
    std::atomic<std::int64_t> latencyTotalNs{0};
    std::atomic<std::int64_t> latencyMinNs{std::numeric_limits<std::int64_t>::max()};
    std::atomic<std::int64_t> latencyMaxNs{0};
    const auto startedAt = std::chrono::steady_clock::now();

    std::jthread consumer([&](std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            auto received = channel.Receive(stopToken);
            if (received.status == XSI::ReceiveStatus::Closed ||
                received.status == XSI::ReceiveStatus::Cancelled) {
                break;
            }
            if (received.status != XSI::ReceiveStatus::Event || !received.event) {
                lifecycleErrors.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            const auto& event = *received.event;
            bool sequenceAccepted = event.kind == XSI::EventKind::Discrete;
            if (event.kind != XSI::EventKind::Discrete) {
                sequenceAccepted = received.streamSequence &&
                    received.streamSequence->accepted;
            }
            if (!sequenceAccepted) {
                lifecycleErrors.fetch_add(1, std::memory_order_relaxed);
            } else if (event.kind == XSI::EventKind::StreamStarted) {
                activeStreams.fetch_add(1, std::memory_order_relaxed);
            } else if (event.kind == XSI::EventKind::StreamEnded) {
                activeStreams.fetch_sub(1, std::memory_order_relaxed);
            }

            std::cout << "event " << XSI::ToString(event.id)
                      << " source=" << event.source.adapter << '/'
                      << event.source.instance
                      << " channel=" << event.channel
                      << " kind=" << EventKindName(event.kind)
                      << " parts=" << event.parts.size();
            if (event.streamId) {
                std::cout << " stream=" << *event.streamId
                          << " sequence=" << *event.sequence;
            }
            std::cout << '\n';

            for (const auto& part : event.parts) {
                if (const auto* text = std::get_if<XSI::TextPart>(&part)) {
                    std::cout << "  text bytes=" << text->utf8.size() << '\n';
                    continue;
                }
                const auto* media = std::get_if<XSI::MediaPart>(&part);
                if (!media) continue;
                const auto bytes = media->resource.Bytes();
                const bool live = media->resource.IsValid() &&
                    !bytes.empty() && media->resource.Data() == bytes.data();
                if (live) mediaCount.fetch_add(1, std::memory_order_relaxed);
                else lifecycleErrors.fetch_add(1, std::memory_order_relaxed);
                std::cout << "  media kind=" << MediaKindName(media->kind)
                          << " format=" << media->formatId
                          << " bytes=" << bytes.size()
                          << " lifecycle=" << (live ? "valid" : "invalid")
                          << '\n';
            }

            const auto latency = std::max<std::int64_t>(
                0, SteadyNowNanoseconds() - event.receivedTime.monotonicNanoseconds);
            latencyTotalNs.fetch_add(latency, std::memory_order_relaxed);
            auto minimum = latencyMinNs.load(std::memory_order_relaxed);
            while (latency < minimum && !latencyMinNs.compare_exchange_weak(
                minimum, latency, std::memory_order_relaxed)) {
            }
            auto maximum = latencyMaxNs.load(std::memory_order_relaxed);
            while (latency > maximum && !latencyMaxNs.compare_exchange_weak(
                maximum, latency, std::memory_order_relaxed)) {
            }
            eventCount.fetch_add(1, std::memory_order_relaxed);
        }
    });

    XSI::SimulatedTextImageAdapter chat;
    XSI::SimulatedVideoAdapter camera(std::chrono::milliseconds(33), 64 * 1024);
    const auto chatResult = chat.Start(channel);
    if (!chatResult.Started()) {
        std::cerr << "simulated chat adapter failed to publish its event\n";
        channel.Close();
        consumer.join();
        return EXIT_FAILURE;
    }
    const auto chatStop = chat.StopAndReport();
    if (chatStop.status != XSI::AdapterStopStatus::Stopped) {
        lifecycleErrors.fetch_add(1, std::memory_order_relaxed);
    }

    const auto videoResult = camera.Start(channel);
    if (!videoResult.Started()) {
        std::cerr << "simulated video adapter failed to start\n";
        channel.Close();
        consumer.join();
        return EXIT_FAILURE;
    }
    const auto runSeconds = ParseRunSeconds(argc, argv);
    std::this_thread::sleep_for(std::chrono::seconds(runSeconds));
    const auto cameraStop = camera.StopAndReport();
    if (cameraStop.status != XSI::AdapterStopStatus::Stopped ||
        !cameraStop.streamEndPublished) {
        lifecycleErrors.fetch_add(1, std::memory_order_relaxed);
    }
    channel.Close();
    consumer.join();

    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - startedAt).count();
    const auto events = eventCount.load(std::memory_order_relaxed);
    const auto latencyAverage = events == 0 ? 0 :
        latencyTotalNs.load(std::memory_order_relaxed) /
        static_cast<std::int64_t>(events);
    const auto latencyMinimum = events == 0 ? 0 :
        latencyMinNs.load(std::memory_order_relaxed);
    const auto queueStats = channel.Stats();
    const auto mediaStats = XSI::CaptureMediaResourceMetrics();

    std::cout << "summary events=" << events
              << " mediaRefsVerified=" << mediaCount.load()
              << " droppedStaleFrames=" << queueStats.droppedStaleFrames
              << " queuePeakDepth=" << queueStats.peakDepth
              << " queueCapacity=" << channel.Capacity()
              << " eventsPerSecond=" << (elapsed > 0 ? events / elapsed : 0)
              << " latencyMs[min/avg/max]="
              << latencyMinimum / 1'000'000.0 << '/'
              << latencyAverage / 1'000'000.0 << '/'
              << latencyMaxNs.load(std::memory_order_relaxed) / 1'000'000.0
              << " mediaPeakBytes=" << mediaStats.peakLiveBytes
              << " mediaLiveBytes=" << mediaStats.liveBytes
              << " lifecycleErrors=" << lifecycleErrors.load() << '\n';

    return lifecycleErrors.load() == 0 &&
        activeStreams.load() == 0 &&
        mediaStats.liveBytes == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
