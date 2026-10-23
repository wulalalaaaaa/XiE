#include "XSI/SensoryAdapter.h"
#include "XSI/StreamSequenceValidator.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <iostream>
#include <latch>
#include <limits>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

std::size_t gChecks = 0;
std::size_t gFailures = 0;

void Check(bool condition, const char* message) {
    ++gChecks;
    if (condition) return;
    ++gFailures;
    std::cerr << "FAIL: " << message << '\n';
}

XSI::SensoryEvent MakeEvent(
    const char* adapter = "test-source",
    const char* instance = "instance-01",
    const char* channel = "session-01") {
    XSI::SensoryEvent event;
    event.source = {adapter, instance};
    event.channel = channel;
    event.occurredTime = XSI::CurrentUtcTime();
    return event;
}

XSI::SensoryEvent MakeVideoFrame(
    std::uint64_t sequence, std::size_t byteCount = 8) {
    auto event = MakeEvent("sim-video", "camera-test", "room-test");
    event.kind = XSI::EventKind::StreamData;
    event.streamId = "stream-test";
    event.sequence = sequence;
    std::vector<std::byte> bytes(byteCount, static_cast<std::byte>(sequence & 0xff));
    event.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Video,
        XSI::MediaResourceRef::Adopt(std::move(bytes)),
        "video/x-test",
        sequence > 0 ? std::optional<std::uint64_t>((sequence - 1) * 33'000'000) :
            std::nullopt,
        33'000'000});
    return event;
}

XSI::SensoryEvent MakeMixedVideoFrame(std::uint64_t sequence) {
    auto event = MakeVideoFrame(sequence);
    event.parts.insert(event.parts.begin(), XSI::TextPart{
        "caption-" + std::to_string(sequence)});
    return event;
}

XSI::SensoryEvent MakeStreamDataWithText(
    const char* adapter, const char* instance, const char* channel,
    const std::string& streamId, std::uint64_t sequence) {
    auto event = MakeEvent(adapter, instance, channel);
    event.kind = XSI::EventKind::StreamData;
    event.streamId = streamId;
    event.sequence = sequence;
    event.parts.emplace_back(XSI::TextPart{"payload"});
    return event;
}

class SignalFramePublisher final : public XSI::ISensoryEventPublisher {
public:
    explicit SignalFramePublisher(XSI::SensoryEventChannel& channel)
        : m_Channel(channel), m_FrameAcceptedFuture(m_FrameAccepted.get_future()) {}

    XSI::PublishResult TryPublish(XSI::SensoryEvent event) override {
        return m_Channel.TryPublish(std::move(event));
    }

    XSI::PublishResult PublishWait(
        XSI::SensoryEvent event, std::stop_token stopToken = {}) override {
        const bool isFrame = event.kind == XSI::EventKind::StreamData;
        auto result = m_Channel.PublishWait(std::move(event), stopToken);
        if (isFrame && result.Accepted() &&
            !m_Signaled.exchange(true, std::memory_order_relaxed)) {
            m_FrameAccepted.set_value();
        }
        return result;
    }

    XSI::PublishResult PublishFor(
        XSI::SensoryEvent event, std::chrono::milliseconds timeout,
        std::stop_token stopToken = {}) override {
        return m_Channel.PublishFor(std::move(event), timeout, stopToken);
    }

    XSI::StreamCleanupResult AbandonStream(
        std::string_view adapter, std::string_view instance,
        std::string_view channel, std::string_view streamId) noexcept override {
        return m_Channel.AbandonStream(adapter, instance, channel, streamId);
    }

    [[nodiscard]] bool WaitForFirstFrame(std::chrono::milliseconds timeout) {
        return m_FrameAcceptedFuture.wait_for(timeout) == std::future_status::ready;
    }

private:
    XSI::SensoryEventChannel& m_Channel;
    std::promise<void> m_FrameAccepted;
    std::future<void> m_FrameAcceptedFuture;
    std::atomic<bool> m_Signaled{false};
};

class ThrowingFramePublisher final : public XSI::ISensoryEventPublisher {
public:
    ThrowingFramePublisher()
        : m_FrameAttemptedFuture(m_FrameAttempted.get_future()),
          m_EndAttemptedFuture(m_EndAttempted.get_future()) {}

    XSI::PublishResult TryPublish(XSI::SensoryEvent) override {
        return {XSI::PublishStatus::Accepted, std::nullopt, std::nullopt};
    }

    XSI::PublishResult PublishWait(
        XSI::SensoryEvent, std::stop_token = {}) override {
        if (!m_Signaled.exchange(true, std::memory_order_relaxed)) {
            m_FrameAttempted.set_value();
        }
        throw std::runtime_error("injected frame publisher failure");
    }

    XSI::PublishResult PublishFor(
        XSI::SensoryEvent event, std::chrono::milliseconds,
        std::stop_token = {}) override {
        if (event.kind == XSI::EventKind::StreamEnded) {
            m_EndCount.fetch_add(1, std::memory_order_relaxed);
            if (!m_EndSignaled.exchange(true, std::memory_order_relaxed)) {
                m_EndAttempted.set_value();
            }
        }
        return {XSI::PublishStatus::Accepted, std::nullopt, std::nullopt};
    }

    XSI::StreamCleanupResult AbandonStream(
        std::string_view, std::string_view, std::string_view,
        std::string_view) noexcept override {
        return {true, 0};
    }

    [[nodiscard]] bool WaitForFrameAttempt(std::chrono::milliseconds timeout) {
        return m_FrameAttemptedFuture.wait_for(timeout) ==
            std::future_status::ready;
    }

    [[nodiscard]] std::size_t EndCount() const noexcept {
        return m_EndCount.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool WaitForEndAttempt(std::chrono::milliseconds timeout) {
        return m_EndAttemptedFuture.wait_for(timeout) ==
            std::future_status::ready;
    }

private:
    std::promise<void> m_FrameAttempted;
    std::future<void> m_FrameAttemptedFuture;
    std::promise<void> m_EndAttempted;
    std::future<void> m_EndAttemptedFuture;
    std::atomic<bool> m_Signaled{false};
    std::atomic<bool> m_EndSignaled{false};
    std::atomic<std::size_t> m_EndCount{0};
};

XSI::SensoryEvent MakeStreamMarker(
    XSI::EventKind kind, std::uint64_t sequence) {
    auto event = MakeEvent("sim-av", "source-a", "room-a");
    event.kind = kind;
    event.streamId = "av-stream-1";
    event.sequence = sequence;
    return event;
}

void TestSingleTextAndImage() {
    auto text = MakeEvent();
    text.parts.emplace_back(XSI::TextPart{
        "\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x8C"
        "XiE sensory interface"});
    Check(!XSI::ValidateEvent(text), "accepts a single UTF-8 text event");

    auto image = MakeEvent();
    std::vector<std::byte> pixels(16, std::byte{0x5a});
    image.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Image,
        XSI::MediaResourceRef::Adopt(std::move(pixels)),
        "image/png",
        std::nullopt,
        std::nullopt});
    Check(!XSI::ValidateEvent(image), "accepts a single image event");
}

void TestMixedPartsAndOptionalIdentity() {
    auto event = MakeEvent();
    event.parts.reserve(4);
    event.parts.emplace_back(XSI::TextPart{"before"});
    event.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Image,
        XSI::MediaResourceRef::Adopt(std::vector<std::byte>(4, std::byte{1})),
        "image/jpeg"});
    event.parts.emplace_back(XSI::TextPart{"between"});
    event.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Image,
        XSI::MediaResourceRef::Adopt(std::vector<std::byte>(5, std::byte{2})),
        "image/png"});
    Check(!XSI::ValidateEvent(event), "accepts text-image-text-image ordering");
    Check(std::holds_alternative<XSI::TextPart>(event.parts[0]) &&
        std::holds_alternative<XSI::MediaPart>(event.parts[1]) &&
        std::holds_alternative<XSI::TextPart>(event.parts[2]) &&
        std::holds_alternative<XSI::MediaPart>(event.parts[3]),
        "preserves mixed part order");

    auto anonymous = MakeEvent("sensor", "sensor-1", "device-session");
    anonymous.actor.reset();
    anonymous.externalId.reset();
    anonymous.parts.emplace_back(XSI::TextPart{"anonymous event"});
    Check(!XSI::ValidateEvent(anonymous),
        "actor and external ID may be omitted");
}

void TestMultiSourceAndEventIdentity() {
    auto first = MakeEvent("qq", "account-1", "session-a");
    first.externalId = XSI::ExternalId{"qq:group", "42"};
    first.parts.emplace_back(XSI::TextPart{"first"});
    auto second = MakeEvent("camera", "device-1", "session-a");
    second.externalId = XSI::ExternalId{"camera:event", "42"};
    second.parts.emplace_back(XSI::TextPart{"second"});
    auto third = MakeEvent("qq", "account-1", "session-b");
    third.externalId = XSI::ExternalId{"qq:group", "42"};
    third.parts.emplace_back(XSI::TextPart{"third"});

    const std::set<std::string> ids{
        XSI::ToString(first.id), XSI::ToString(second.id), XSI::ToString(third.id)};
    Check(ids.size() == 3, "event IDs are unique across sources and sessions");
    Check(!XSI::ValidateEvent(first) && !XSI::ValidateEvent(second) &&
        !XSI::ValidateEvent(third),
        "external IDs with different namespaces remain valid");
    Check(XSI::ToString(first.id).size() == 32,
        "event IDs have a fixed 128-bit hexadecimal representation");
}

void TestContinuousAudioVideoAndSequence() {
    XSI::StreamSequenceValidator guard;
    auto start = MakeStreamMarker(XSI::EventKind::StreamStarted, 0);
    auto audio = MakeStreamMarker(XSI::EventKind::StreamData, 1);
    audio.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Audio,
        XSI::MediaResourceRef::Adopt(std::vector<std::byte>(128, std::byte{3})),
        "audio/pcm-s16le",
        0,
        10'000'000});
    auto video = MakeStreamMarker(XSI::EventKind::StreamData, 2);
    video.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Video,
        XSI::MediaResourceRef::Adopt(std::vector<std::byte>(64, std::byte{4})),
        "video/x-rgb",
        10'000'000,
        33'000'000});
    auto end = MakeStreamMarker(XSI::EventKind::StreamEnded, 3);

    Check(!XSI::ValidateEvent(start) && !XSI::ValidateEvent(audio) &&
        !XSI::ValidateEvent(video) && !XSI::ValidateEvent(end),
        "accepts audio/video stream markers and timestamped data");
    Check(guard.Accept(start).accepted &&
        guard.Accept(audio).accepted &&
        guard.Accept(video).accepted &&
        guard.Accept(end).accepted &&
        guard.ActiveStreamCount() == 0,
        "associates ordered audio/video segments and releases stream state");

    auto badStart = MakeStreamMarker(XSI::EventKind::StreamStarted, 1);
    Check(XSI::ValidateEvent(badStart).has_value(),
        "rejects a stream start with a nonzero sequence");
    auto badFrame = MakeVideoFrame(0);
    Check(XSI::ValidateEvent(badFrame).has_value(),
        "rejects stream data with sequence zero");
}

void TestSequenceGapsAndEnd() {
    XSI::StreamSequenceValidator guard;
    auto start = MakeStreamMarker(XSI::EventKind::StreamStarted, 0);
    auto gap = MakeStreamMarker(XSI::EventKind::StreamData, 2);
    gap.parts.emplace_back(XSI::TextPart{"simulated frame"});
    auto frame = MakeStreamMarker(XSI::EventKind::StreamData, 1);
    frame.parts.emplace_back(XSI::TextPart{"simulated frame"});
    auto badEnd = MakeStreamMarker(XSI::EventKind::StreamEnded, 4);
    auto end = MakeStreamMarker(XSI::EventKind::StreamEnded, 2);

    Check(guard.Accept(start).accepted, "stream sequence guard accepts start");
    const auto gapResult = guard.Accept(gap);
    Check(!gapResult.accepted &&
        gapResult.error == XSI::StreamSequenceError::UnexpectedSequence &&
        gapResult.expected == 1,
        "reports anomalous data sequence numbers");
    Check(guard.Accept(frame).accepted,
        "a valid frame can follow a rejected out-of-order event");
    Check(!guard.Accept(badEnd).accepted,
        "rejects a stream end with the wrong next sequence");
    Check(guard.Accept(end).accepted && guard.ActiveStreamCount() == 0,
        "accepts stream end at next expected sequence and releases state");

    auto abandonedStart = MakeStreamMarker(XSI::EventKind::StreamStarted, 0);
    abandonedStart.streamId = "abandoned-stream";
    auto abandonedData = MakeStreamMarker(XSI::EventKind::StreamData, 1);
    abandonedData.streamId = "abandoned-stream";
    abandonedData.parts.emplace_back(XSI::TextPart{"payload"});
    Check(guard.Accept(abandonedStart).accepted &&
        guard.ActiveStreamCount() == 1 &&
        guard.ForgetStream("sim-av", "source-a", "room-a", "abandoned-stream") &&
        guard.ActiveStreamCount() == 0 &&
        guard.Accept(abandonedData).error ==
            XSI::StreamSequenceError::StreamNotStarted,
        "explicit stream-abort cleanup removes active sequence state");

    XSI::StreamSequenceValidator gapTolerant;
    auto videoStart = MakeEvent("sim-video", "camera-test", "room-test");
    videoStart.kind = XSI::EventKind::StreamStarted;
    videoStart.streamId = "stream-test";
    videoStart.sequence = 0;
    const auto droppedFrame = MakeVideoFrame(1);
    const auto afterDrop = MakeVideoFrame(2);
    Check(gapTolerant.Accept(videoStart).accepted &&
        gapTolerant.NoteDroppedVideoFrame(droppedFrame) &&
        gapTolerant.Accept(afterDrop, true).accepted,
        "LatestOnly accepts a forward gap only after recording a dropped video frame");
}

void TestResourceCopyMoveAndThreadLifetime() {
    static_assert(std::is_same_v<
        decltype(std::declval<XSI::MediaResourceRef>().Bytes()),
        std::span<const std::byte>>);
    const auto before = XSI::CaptureMediaResourceMetrics();
    std::vector<std::byte> source(1024, std::byte{0x6d});
    const auto* originalData = source.data();
    auto resource = XSI::MediaResourceRef::Adopt(std::move(source));
    Check(resource.IsValid() && resource.Data() == originalData,
        "adopting media bytes does not deep-copy the vector allocation");
    auto shared = resource;
    Check(shared.Data() == originalData && resource.UseCount() == 2,
        "copying a resource reference shares immutable storage");

    auto event = MakeEvent();
    event.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Image, std::move(shared), "image/test"});
    XSI::SensoryEventChannel channel({4, XSI::QueuePolicy::Reliable, {}});
    const auto publish = channel.TryPublish(std::move(event));
    resource = {};
    Check(publish.Accepted() &&
        XSI::CaptureMediaResourceMetrics().liveBytes >= 1024,
        "published resource survives release of the producer reference");

    auto received = channel.Receive();
    Check(received.status == XSI::ReceiveStatus::Event && received.event,
        "resource event is received on another execution context");
    if (received.event) {
        Check(received.event->receivedTime.utcNanoseconds != 0 &&
            received.event->receivedTime.monotonicNanoseconds != 0,
            "publication stamps separate UTC and process-monotonic receive times");
        const auto& media = std::get<XSI::MediaPart>(received.event->parts[0]);
        std::atomic<bool> valid{false};
        std::thread worker([copy = media.resource, originalData, &valid]() {
            const auto bytes = copy.Bytes();
            valid.store(copy.IsValid() && copy.Data() == originalData &&
                bytes.size() == 1024 && bytes.front() == std::byte{0x6d},
                std::memory_order_relaxed);
        });
        worker.join();
        Check(valid.load(std::memory_order_relaxed),
            "consumer thread safely reads the shared resource after producer release");
    }
    channel.Close();
    received.event.reset();
    Check(XSI::CaptureMediaResourceMetrics().liveBytes == before.liveBytes,
        "resource storage is released after the final reference is dropped");
}

void TestQueueCapacityAndLatestOnly() {
    XSI::SensoryEventChannel reliable({1, XSI::QueuePolicy::Reliable, {}});
    auto first = MakeEvent();
    first.parts.emplace_back(XSI::TextPart{"first"});
    auto second = MakeEvent();
    second.parts.emplace_back(XSI::TextPart{"second"});
    Check(reliable.TryPublish(std::move(first)).Accepted(),
        "reliable queue accepts a message while capacity is available");
    auto backpressure = reliable.TryPublish(std::move(second));
    Check(backpressure.status == XSI::PublishStatus::Full &&
        backpressure.rejectedEvent &&
        std::get<XSI::TextPart>(backpressure.rejectedEvent->parts[0]).utf8 ==
            "second",
        "reliable backpressure returns the rejected event so it can be retried");
    auto item = reliable.Receive();
    Check(item.status == XSI::ReceiveStatus::Event &&
        reliable.TryPublish(MakeEvent()).Accepted(),
        "reliable queue resumes after a consumer frees capacity");
    Check(reliable.Stats().rejectedFull == 1,
        "reliable queue reports full attempts in queue statistics");
    reliable.Close();
    auto drained = reliable.Receive();
    auto closed = reliable.Receive();
    Check(drained.status == XSI::ReceiveStatus::Event &&
        closed.status == XSI::ReceiveStatus::Closed,
        "closed queue drains accepted events before reporting closed");
    auto publishAfterClose = reliable.TryPublish(MakeEvent());
    Check(publishAfterClose.status == XSI::PublishStatus::Closed &&
        publishAfterClose.rejectedEvent.has_value(),
        "closed queue returns the rejected event instead of consuming it");

    XSI::SensoryEventChannel latest({1, XSI::QueuePolicy::LatestOnly, {}});
    Check(latest.TryPublish(MakeVideoFrame(1)).Accepted(),
        "latest-only queue accepts first frame");
    Check(latest.TryPublish(MakeVideoFrame(2)).status ==
        XSI::PublishStatus::AcceptedReplacingStaleFrame,
        "latest-only queue explicitly reports frame replacement");
    auto current = latest.Receive();
    Check(current.event && current.event->sequence == 2 &&
        latest.Stats().droppedStaleFrames == 1,
        "latest-only queue returns current frame and counts the dropped frame");
    latest.Close();

    XSI::SensoryEventChannel noStaleFrame({1, XSI::QueuePolicy::LatestOnly, {}});
    auto control = MakeEvent();
    control.parts.emplace_back(XSI::TextPart{"keep me"});
    Check(noStaleFrame.TryPublish(std::move(control)).Accepted(),
        "latest-only control event occupies capacity");
    auto refusedFrame = noStaleFrame.TryPublish(MakeVideoFrame(1));
    Check(refusedFrame.status == XSI::PublishStatus::Full &&
        refusedFrame.rejectedEvent &&
        refusedFrame.rejectedEvent->sequence == 1 &&
        noStaleFrame.Stats().droppedStaleFrames == 0,
        "latest-only preserves input and control event when no stale frame can be evicted");
    noStaleFrame.Close();

    XSI::SensoryEventChannel invalid({0, XSI::QueuePolicy::Reliable, {}});
    Check(invalid.TryPublish(MakeEvent()).status ==
        XSI::PublishStatus::InvalidConfiguration,
        "zero-capacity queue reports invalid configuration without crashing");
}

void TestMixedLatestOnlyCapacityBounds() {
    constexpr std::array<std::size_t, 3> capacities{1, 2, 4};
    for (const auto capacity : capacities) {
        XSI::SensoryEventChannel channel({
            capacity, XSI::QueuePolicy::LatestOnly, {}});
        bool allAccepted = true;
        for (std::size_t sequence = 1; sequence <= capacity; ++sequence) {
            allAccepted = allAccepted &&
                channel.TryPublish(MakeMixedVideoFrame(sequence)).Accepted();
        }
        auto rejected = channel.TryPublish(MakeMixedVideoFrame(capacity + 1));
        Check(allAccepted && rejected.status == XSI::PublishStatus::Full &&
            rejected.rejectedEvent &&
            channel.Stats().droppedStaleFrames == 0,
            "LatestOnly refuses to evict a mixed text/video event at capacity 1, 2, or 4");

        bool preserved = true;
        for (std::size_t sequence = 1; sequence <= capacity; ++sequence) {
            const auto received = channel.Receive();
            preserved = preserved && received.event &&
                received.event->sequence == sequence &&
                received.event->parts.size() == 2 &&
                std::get<XSI::TextPart>(received.event->parts[0]).utf8 ==
                    "caption-" + std::to_string(sequence);
        }
        Check(preserved,
            "LatestOnly preserves all text and frame parts after mixed-event backpressure");
        channel.Close();
    }
}

void TestOrderedStreamReceiveAndProducerOrder() {
    {
        XSI::SensoryEventChannel channel({8, XSI::QueuePolicy::Reliable, {}});
        auto start = MakeStreamMarker(XSI::EventKind::StreamStarted, 0);
        auto data = MakeStreamDataWithText(
            "sim-av", "source-a", "room-a", "av-stream-1", 1);
        auto end = MakeStreamMarker(XSI::EventKind::StreamEnded, 2);
        Check(channel.TryPublish(std::move(start)).Accepted() &&
            channel.TryPublish(std::move(data)).Accepted() &&
            channel.TryPublish(std::move(end)).Accepted(),
            "ordered receive race fixture is published");

        std::promise<void> startPopped;
        std::promise<void> dataPopped;
        auto startPoppedFuture = startPopped.get_future();
        auto dataPoppedFuture = dataPopped.get_future();
        std::atomic<bool> startAccepted{false};
        std::atomic<bool> dataAccepted{false};
        std::atomic<bool> endAccepted{false};
        std::jthread consumerA([&] {
            const auto received = channel.Receive();
            startAccepted.store(received.streamSequence &&
                received.streamSequence->accepted, std::memory_order_relaxed);
            startPopped.set_value();
            dataPoppedFuture.wait();
        });
        std::jthread consumerB([&] {
            startPoppedFuture.wait();
            const auto received = channel.Receive();
            dataAccepted.store(received.streamSequence &&
                received.streamSequence->accepted, std::memory_order_relaxed);
            dataPopped.set_value();
            const auto ended = channel.Receive();
            endAccepted.store(ended.streamSequence &&
                ended.streamSequence->accepted, std::memory_order_relaxed);
        });
        consumerA.join();
        consumerB.join();
        Check(startAccepted.load(std::memory_order_relaxed) &&
            dataAccepted.load(std::memory_order_relaxed) &&
            endAccepted.load(std::memory_order_relaxed),
            "two consumers cannot process Data before its dequeued Start state");
        channel.Close();
    }

    {
        XSI::SensoryEventChannel channel({8, XSI::QueuePolicy::Reliable, {}});
        auto startA = MakeStreamMarker(XSI::EventKind::StreamStarted, 0);
        auto startB = startA;
        startA.streamId = "stream-a";
        startB.streamId = "stream-b";
        auto dataA = MakeStreamDataWithText(
            "sim-av", "source-a", "room-a", "stream-a", 1);
        auto dataB = MakeStreamDataWithText(
            "sim-av", "source-a", "room-a", "stream-b", 1);
        (void)channel.TryPublish(std::move(startA));
        (void)channel.TryPublish(std::move(startB));
        (void)channel.TryPublish(std::move(dataA));
        (void)channel.TryPublish(std::move(dataB));
        const auto acceptedStartA = channel.Receive();
        const auto acceptedStartB = channel.Receive();
        std::latch callbacksEntered(2);
        std::atomic<std::size_t> acceptedCallbacks{0};
        auto processOneStream = [&] {
            const auto received = channel.Receive();
            if (received.streamSequence && received.streamSequence->accepted) {
                acceptedCallbacks.fetch_add(1, std::memory_order_relaxed);
            }
            callbacksEntered.count_down();
            callbacksEntered.wait();
        };
        std::jthread consumerA(processOneStream);
        std::jthread consumerB(processOneStream);
        consumerA.join();
        consumerB.join();
        Check(acceptedStartA.streamSequence && acceptedStartA.streamSequence->accepted &&
            acceptedStartB.streamSequence && acceptedStartB.streamSequence->accepted &&
            acceptedCallbacks.load(std::memory_order_relaxed) == 2,
            "different streams can process validated payloads in parallel");
        channel.Close();
    }

    {
        constexpr std::size_t dataCount = 100;
        XSI::SensoryEventChannel channel({128, XSI::QueuePolicy::Reliable, {}});
        auto start = MakeStreamMarker(XSI::EventKind::StreamStarted, 0);
        Check(channel.TryPublish(std::move(start)).Accepted(),
            "multi-producer stream publishes its start marker");

        std::mutex turnMutex;
        std::condition_variable turnChanged;
        unsigned turn = 0;
        std::atomic<std::size_t> publishFailures{0};
        std::array<std::thread, 2> producers;
        for (unsigned producer = 0; producer < producers.size(); ++producer) {
            producers[producer] = std::thread([&, producer] {
                for (std::size_t index = 0; index < dataCount / 2; ++index) {
                    std::unique_lock lock(turnMutex);
                    turnChanged.wait(lock, [&] { return turn == producer; });
                    const auto sequence = index * 2 + producer + 1;
                    auto event = MakeStreamDataWithText("sim-av", "source-a",
                        "room-a", "av-stream-1", sequence);
                    if (!channel.TryPublish(std::move(event)).Accepted()) {
                        publishFailures.fetch_add(1, std::memory_order_relaxed);
                    }
                    turn = 1 - producer;
                    lock.unlock();
                    turnChanged.notify_all();
                }
            });
        }
        for (auto& producer : producers) producer.join();
        auto end = MakeStreamMarker(
            XSI::EventKind::StreamEnded, dataCount + 1);
        const auto endPublish = channel.TryPublish(std::move(end));

        auto received = channel.Receive();
        bool ordered = received.streamSequence && received.streamSequence->accepted;
        for (std::size_t i = 0; i < dataCount; ++i) {
            received = channel.Receive();
            ordered = ordered && received.streamSequence &&
                received.streamSequence->accepted && received.event &&
                received.event->sequence == i + 1;
        }
        const auto ended = channel.Receive();
        ordered = ordered && ended.streamSequence && ended.streamSequence->accepted;
        Check(publishFailures.load(std::memory_order_relaxed) == 0 &&
            endPublish.Accepted() && ordered,
            "two producers sharing a stream must serialize sequence assignment with publish order");
        channel.Close();
    }

    {
        XSI::SensoryEventChannel channel({4, XSI::QueuePolicy::Reliable, {}});
        auto start = MakeStreamMarker(XSI::EventKind::StreamStarted, 0);
        (void)channel.TryPublish(std::move(start));
        std::promise<void> laterSequencePublished;
        auto laterPublished = laterSequencePublished.get_future();
        std::thread producerB([&] {
            auto later = MakeStreamDataWithText(
                "sim-av", "source-a", "room-a", "av-stream-1", 2);
            (void)channel.TryPublish(std::move(later));
            laterSequencePublished.set_value();
        });
        laterPublished.wait();
        std::thread producerA([&] {
            auto earlier = MakeStreamDataWithText(
                "sim-av", "source-a", "room-a", "av-stream-1", 1);
            (void)channel.TryPublish(std::move(earlier));
        });
        producerA.join();
        producerB.join();
        const auto startResult = channel.Receive();
        const auto laterResult = channel.Receive();
        const auto earlierResult = channel.Receive();
        Check(startResult.streamSequence && startResult.streamSequence->accepted &&
            laterResult.streamSequence && !laterResult.streamSequence->accepted &&
            laterResult.streamSequence->error ==
                XSI::StreamSequenceError::UnexpectedSequence &&
            earlierResult.streamSequence && earlierResult.streamSequence->accepted,
            "same-stream publish order is queue linearization order and invalid producer order is reported");
        channel.Close();
    }
}

void TestLatestOnlyStreamGapsAndEndDelivery() {
    {
        XSI::SensoryEventChannel channel({2, XSI::QueuePolicy::LatestOnly, {}});
        auto start = MakeEvent("sim-video", "camera-test", "room-test");
        start.kind = XSI::EventKind::StreamStarted;
        start.streamId = "stream-test";
        start.sequence = 0;
        Check(channel.TryPublish(std::move(start)).Accepted() &&
            channel.TryPublish(MakeVideoFrame(1)).Accepted() &&
            channel.TryPublish(MakeVideoFrame(2)).status ==
                XSI::PublishStatus::AcceptedReplacingStaleFrame,
            "LatestOnly records an evicted video sequence");
        const auto startReceived = channel.Receive();
        const auto afterDrop = channel.Receive();
        Check(startReceived.streamSequence && startReceived.streamSequence->accepted &&
            afterDrop.streamSequence && afterDrop.streamSequence->accepted &&
            afterDrop.streamSequence->skippedSequences == 1,
            "a recorded frame gap is accepted and reported without breaking the stream");

        Check(channel.TryPublish(MakeVideoFrame(3)).Accepted(),
            "LatestOnly stream accepts the next in-order frame");
        auto frame3 = channel.Receive();
        Check(frame3.streamSequence && frame3.streamSequence->accepted,
            "LatestOnly stream continues after a permitted gap");
        Check(channel.TryPublish(MakeVideoFrame(3)).Accepted(),
            "duplicate-sequence fixture is structurally publishable");
        auto duplicate = channel.Receive();
        Check(duplicate.streamSequence && !duplicate.streamSequence->accepted &&
            duplicate.streamSequence->error ==
                XSI::StreamSequenceError::UnexpectedSequence,
            "LatestOnly never accepts duplicate or backward sequences");
        Check(channel.TryPublish(MakeVideoFrame(5)).Accepted(),
            "unrecorded-gap fixture is structurally publishable");
        auto unrecordedGap = channel.Receive();
        Check(unrecordedGap.streamSequence &&
            !unrecordedGap.streamSequence->accepted &&
            unrecordedGap.streamSequence->error ==
                XSI::StreamSequenceError::UnexpectedSequence,
            "LatestOnly rejects forward gaps that were not caused by a dropped frame");
        Check(channel.TryPublish(MakeVideoFrame(4)).Accepted(),
            "stream can recover after a rejected out-of-order event");
        auto frame4 = channel.Receive();
        auto end = MakeEvent("sim-video", "camera-test", "room-test");
        end.kind = XSI::EventKind::StreamEnded;
        end.streamId = "stream-test";
        end.sequence = 5;
        Check(frame4.streamSequence && frame4.streamSequence->accepted &&
            channel.TryPublish(std::move(end)).Accepted(),
            "ordered frame remains valid before StreamEnded");
        const auto ended = channel.Receive();
        Check(ended.streamSequence && ended.streamSequence->accepted,
            "StreamEnded is never selected as a stale video frame");
        channel.Close();
    }

    {
        XSI::SensoryEventChannel channel({2, XSI::QueuePolicy::LatestOnly, {}});
        auto start = MakeEvent("sim-video", "camera-test", "room-test");
        start.kind = XSI::EventKind::StreamStarted;
        start.streamId = "stream-test";
        start.sequence = 0;
        auto end = MakeEvent("sim-video", "camera-test", "room-test");
        end.kind = XSI::EventKind::StreamEnded;
        end.streamId = "stream-test";
        end.sequence = 3;
        Check(channel.TryPublish(std::move(start)).Accepted() &&
            channel.TryPublish(MakeVideoFrame(1)).Accepted() &&
            channel.TryPublish(MakeVideoFrame(2)).Accepted() &&
            channel.TryPublish(std::move(end)).status ==
                XSI::PublishStatus::AcceptedReplacingStaleFrame,
            "LatestOnly may discard stale frames but preserves stream boundaries");
        const auto startResult = channel.Receive();
        const auto endResult = channel.Receive();
        Check(startResult.event &&
            startResult.event->kind == XSI::EventKind::StreamStarted &&
            endResult.event && endResult.event->kind == XSI::EventKind::StreamEnded &&
            endResult.streamSequence && endResult.streamSequence->accepted &&
            endResult.streamSequence->skippedSequences == 2,
            "StreamEnded closes the stream after documented frame gaps");
        channel.Close();
    }
}

void TestDisjointLatestOnlyDropsAndTrackingSaturation() {
    {
        XSI::SensoryEventChannel channel({3, XSI::QueuePolicy::LatestOnly, {}});
        auto start = MakeEvent("sim-video", "camera-test", "room-test");
        start.kind = XSI::EventKind::StreamStarted;
        start.streamId = "stream-test";
        start.sequence = 0;
        auto middle = MakeEvent("sim-video", "camera-test", "room-test");
        middle.kind = XSI::EventKind::StreamData;
        middle.streamId = "stream-test";
        middle.sequence = 2;
        middle.parts.emplace_back(XSI::TextPart{"non-evictable middle data"});

        Check(channel.TryPublish(std::move(start)).Accepted() &&
            channel.TryPublish(MakeVideoFrame(1)).Accepted() &&
            channel.TryPublish(std::move(middle)).Accepted() &&
            channel.TryPublish(MakeVideoFrame(3)).status ==
                XSI::PublishStatus::AcceptedReplacingStaleFrame &&
            channel.TryPublish(MakeVideoFrame(4)).status ==
                XSI::PublishStatus::AcceptedReplacingStaleFrame,
            "LatestOnly records two separated frame drops around retained stream data");

        const auto receivedStart = channel.Receive();
        const auto receivedMiddle = channel.Receive();
        const auto receivedLastFrame = channel.Receive();
        Check(receivedStart.streamSequence && receivedStart.streamSequence->accepted &&
            receivedMiddle.streamSequence && receivedMiddle.streamSequence->accepted &&
            receivedMiddle.streamSequence->skippedSequences == 1 &&
            receivedLastFrame.streamSequence &&
            receivedLastFrame.streamSequence->accepted &&
            receivedLastFrame.streamSequence->skippedSequences == 1,
            "separated dropped-frame intervals do not reject valid intervening data");
        channel.Close();
    }

    {
        XSI::SensoryEventChannel channel({2, XSI::QueuePolicy::LatestOnly, {}, 1});
        auto makeFrame = [](const char* adapter, const char* instance,
                            const char* channelName, const char* stream,
                            std::uint64_t sequence) {
            auto event = MakeEvent(adapter, instance, channelName);
            event.kind = XSI::EventKind::StreamData;
            event.streamId = stream;
            event.sequence = sequence;
            event.parts.emplace_back(XSI::MediaPart{
                XSI::MediaKind::Video,
                XSI::MediaResourceRef::Adopt(
                    std::vector<std::byte>(4, std::byte{0x5a})),
                "video/x-test",
                (sequence - 1) * 33'000'000,
                33'000'000});
            return event;
        };
        Check(channel.TryPublish(makeFrame("camera", "a", "room", "A", 1)).Accepted() &&
            channel.TryPublish(makeFrame("camera", "b", "room", "B", 1)).Accepted() &&
            channel.TryPublish(makeFrame("camera", "a", "room", "A", 2)).status ==
                XSI::PublishStatus::AcceptedReplacingStaleFrame &&
            channel.TryPublish(makeFrame("camera", "a", "room", "A", 3)).status ==
                XSI::PublishStatus::AcceptedReplacingStaleFrame,
            "LatestOnly skips an untrackable oldest stream frame when a later frame is trackable");
        Check(channel.Stats().droppedStaleFrames == 2,
            "tracking saturation fallback records both legal evictions");
        channel.Close();
    }

    {
        constexpr std::size_t searchLimit = 1'024;
        constexpr std::size_t capacity = searchLimit + 3;
        XSI::SensoryEventChannel channel({
            capacity, XSI::QueuePolicy::LatestOnly, {}, 1});
        auto makeFrame = [](const char* instance, const char* stream,
                            std::uint64_t sequence) {
            auto event = MakeEvent("camera", instance, "room");
            event.kind = XSI::EventKind::StreamData;
            event.streamId = stream;
            event.sequence = sequence;
            event.parts.emplace_back(XSI::MediaPart{
                XSI::MediaKind::Video,
                XSI::MediaResourceRef::Adopt(
                    std::vector<std::byte>(1, std::byte{0x1})),
                "video/x-test", (sequence - 1) * 33'000'000, 33'000'000});
            return event;
        };
        auto firstControl = MakeEvent("camera", "control", "room");
        firstControl.parts.emplace_back(XSI::TextPart{"keep"});
        bool seeded = channel.TryPublish(makeFrame("a", "A", 1)).Accepted();
        for (std::size_t sequence = 1; sequence <= searchLimit; ++sequence) {
            seeded = seeded && channel.TryPublish(
                makeFrame("b", "B", sequence)).Accepted();
        }
        seeded = seeded && channel.TryPublish(makeFrame("a", "A", 2)).Accepted() &&
            channel.TryPublish(std::move(firstControl)).Accepted() &&
            channel.TryPublish(makeFrame("a", "A", 3)).status ==
                XSI::PublishStatus::AcceptedReplacingStaleFrame;
        const auto before = channel.Stats();
        auto rejected = channel.TryPublish(makeFrame("a", "A", 4));
        const auto after = channel.Stats();
        Check(seeded && rejected.status == XSI::PublishStatus::Full &&
            rejected.rejectedEvent.has_value() && before.depth == capacity &&
            after.depth == capacity &&
            after.droppedStaleFrames == before.droppedStaleFrames,
            "tracking saturation search is bounded and returns the unmodified rejected event at its limit");
        channel.Close();
    }
}

void TestSharedValidationLimits() {
    XSI::ValidationLimits limits;
    limits.maxStringBytes = 600;
    XSI::SensoryEventChannel channel({
        4, XSI::QueuePolicy::Reliable, limits, 4});
    const std::string streamId(513, 's');
    auto start = MakeEvent("custom", "instance", "channel");
    start.kind = XSI::EventKind::StreamStarted;
    start.streamId = streamId;
    start.sequence = 0;
    auto data = MakeStreamDataWithText(
        "custom", "instance", "channel", streamId, 1);
    Check(channel.TryPublish(std::move(start)).Accepted() &&
        channel.TryPublish(std::move(data)).Accepted(),
        "channel uses its configured 600-byte string limit");
    const auto receivedStart = channel.Receive();
    const auto receivedData = channel.Receive();
    Check(receivedStart.streamSequence && receivedStart.streamSequence->accepted &&
        receivedData.streamSequence && receivedData.streamSequence->accepted,
        "channel and sequence validator share the same custom limits");

    XSI::StreamSequenceValidator validator(4, limits);
    auto externalStart = MakeEvent("custom", "instance", "channel");
    externalStart.kind = XSI::EventKind::StreamStarted;
    externalStart.streamId = streamId;
    externalStart.sequence = 0;
    auto externalData = MakeStreamDataWithText(
        "custom", "instance", "channel", streamId, 1);
    Check(validator.Accept(externalStart).accepted &&
        validator.Accept(externalData).accepted,
        "standalone sequence validator accepts the same configured 513-byte stream ID");
    channel.Close();
}

void TestVideoAdapterConfigurationAndWorkerFailures() {
    struct InvalidConfig {
        std::chrono::milliseconds interval;
        std::size_t bytes;
        XSI::AdapterError expected;
    };
    const std::array<InvalidConfig, 4> invalidConfigs{{
        {std::chrono::milliseconds(1), 0, XSI::AdapterError::InvalidFrameSize},
        {std::chrono::milliseconds(1),
            std::numeric_limits<std::size_t>::max(),
            XSI::AdapterError::InvalidFrameSize},
        {std::chrono::milliseconds(1), 64 * 1024 * 1024 + 1,
            XSI::AdapterError::InvalidFrameSize},
        {std::chrono::milliseconds::max(), 8,
            XSI::AdapterError::InvalidFrameInterval}}};
    for (const auto& invalid : invalidConfigs) {
        XSI::SensoryEventChannel channel({2, XSI::QueuePolicy::Reliable, {}});
        XSI::SimulatedVideoAdapter adapter(invalid.interval, invalid.bytes);
        const auto started = adapter.Start(channel);
        const auto status = adapter.RuntimeStatus();
        Check(started.status == XSI::AdapterStatus::InvalidConfiguration &&
            started.error == invalid.expected && status.error == invalid.expected &&
            channel.Stats().accepted == 0,
            "video adapter rejects invalid frame sizes and overflowing intervals before Start");
        (void)adapter.Stop();
        channel.Close();
    }

    {
        XSI::SensoryEventChannel channel({16, XSI::QueuePolicy::Reliable, {}});
        XSI::SimulatedVideoAdapter adapter(std::chrono::milliseconds(2), 128);
        std::promise<void> startSeen;
        auto startSeenFuture = startSeen.get_future();
        std::atomic<bool> startSequenceAccepted{false};
        std::atomic<bool> endSeen{false};
        std::atomic<bool> endSequenceAccepted{false};
        std::jthread consumer([&] {
            for (;;) {
                const auto received = channel.Receive();
                if (received.status == XSI::ReceiveStatus::Closed) break;
                if (!received.event) continue;
                if (received.event->kind == XSI::EventKind::StreamStarted) {
                    startSequenceAccepted.store(received.streamSequence &&
                        received.streamSequence->accepted,
                        std::memory_order_relaxed);
                    startSeen.set_value();
                } else if (received.event->kind == XSI::EventKind::StreamEnded) {
                    endSeen.store(true, std::memory_order_relaxed);
                    endSequenceAccepted.store(received.streamSequence &&
                        received.streamSequence->accepted,
                        std::memory_order_relaxed);
                }
            }
        });
        const auto started = adapter.Start(channel);
        const bool sawStart = startSeenFuture.wait_for(
            std::chrono::seconds(1)) == std::future_status::ready;
        const auto stopped = adapter.StopAndReport();
        channel.Close();
        consumer.join();
        Check(started.Started() && sawStart && startSequenceAccepted.load() &&
            stopped.status == XSI::AdapterStopStatus::Stopped &&
            stopped.streamEndPublished && !stopped.streamAbnormallyTerminated &&
            endSeen.load() && endSequenceAccepted.load(),
            "normal adapter stop reliably delivers and validates StreamEnded");
    }

    {
        XSI::SensoryEventChannel channel({1, XSI::QueuePolicy::Reliable, {}});
        XSI::SimulatedVideoAdapter adapter(std::chrono::milliseconds(1), 32);
        const auto started = adapter.Start(channel);
        const auto began = std::chrono::steady_clock::now();
        const auto stopped = adapter.StopAndReport();
        const auto elapsed = std::chrono::steady_clock::now() - began;
        const auto stats = channel.Stats();
        Check(started.Started() &&
            stopped.status == XSI::AdapterStopStatus::AbnormalTermination &&
            !stopped.streamEndPublished && stopped.streamStateCleared &&
            stopped.streamEndStatus == XSI::PublishStatus::TimedOut &&
            elapsed < std::chrono::seconds(1) && stats.depth == 0 &&
            stats.abandonedStreamEvents == 1,
            "full queue without a consumer causes bounded stop and explicit stream cleanup");
        Check(adapter.RuntimeStatus().error ==
            XSI::AdapterError::StreamEndPublishFailed,
            "failed StreamEnded status is readable after stop");
        channel.Close();
    }

    {
        XSI::SensoryEventChannel channel({1, XSI::QueuePolicy::Reliable, {}});
        SignalFramePublisher publisher(channel);
        XSI::SimulatedVideoAdapter adapter(std::chrono::milliseconds(1), 32);
        const auto started = adapter.Start(publisher);
        const auto receivedStart = channel.Receive();
        const bool frameQueued = publisher.WaitForFirstFrame(
            std::chrono::seconds(1));
        const auto stopped = adapter.StopAndReport();
        Check(started.Started() && receivedStart.streamSequence &&
            receivedStart.streamSequence->accepted && frameQueued &&
            stopped.status == XSI::AdapterStopStatus::AbnormalTermination &&
            stopped.streamEndStatus == XSI::PublishStatus::TimedOut &&
            stopped.streamStateCleared && channel.Stats().depth == 0,
            "stopped consumer cannot block adapter shutdown and its active stream state is cleared");
        channel.Close();
    }

    {
        XSI::SensoryEventChannel channel({2, XSI::QueuePolicy::Reliable, {}});
        XSI::SimulatedVideoAdapter adapter(std::chrono::milliseconds(1), 16);
        const auto started = adapter.Start(channel);
        channel.Close();
        const auto stopped = adapter.StopAndReport();
        Check(started.Started() &&
            stopped.status == XSI::AdapterStopStatus::AbnormalTermination &&
            stopped.streamEndStatus == XSI::PublishStatus::Closed &&
            stopped.streamStateCleared && channel.Stats().depth == 0,
            "closed channel reports failed StreamEnded and removes queued stream state");
    }

    {
        XSI::SensoryEventChannel channel({1, XSI::QueuePolicy::Reliable, {}});
        XSI::SimulatedVideoAdapter adapter(std::chrono::milliseconds(1), 16);
        const auto started = adapter.Start(channel);
        std::stop_source cancelled;
        cancelled.request_stop();
        const auto stopped = adapter.StopAndReport(cancelled.get_token());
        Check(started.Started() &&
            stopped.status == XSI::AdapterStopStatus::AbnormalTermination &&
            stopped.streamEndStatus == XSI::PublishStatus::Cancelled &&
            stopped.streamStateCleared && channel.Stats().depth == 0,
            "cancellation of bounded stream-end wait is reported and cleaned up");
        channel.Close();
    }

    {
        ThrowingFramePublisher publisher;
        XSI::SimulatedVideoAdapter adapter(std::chrono::milliseconds(1), 32);
        const auto started = adapter.Start(publisher);
        const bool attempted = publisher.WaitForFrameAttempt(
            std::chrono::seconds(1));
        const bool endAttempted = publisher.WaitForEndAttempt(
            std::chrono::seconds(1));
        const auto runtimeBeforeStop = adapter.RuntimeStatus();
        const auto stopped = adapter.StopAndReport();
        const auto runtime = adapter.RuntimeStatus();
        Check(started.Started() && attempted && endAttempted &&
            runtimeBeforeStop.state == XSI::AdapterRunState::Failed &&
            runtimeBeforeStop.error == XSI::AdapterError::WorkerException &&
            stopped.status == XSI::AdapterStopStatus::AbnormalTermination &&
            stopped.streamEndPublished && runtime.state == XSI::AdapterRunState::Failed &&
            runtime.error == XSI::AdapterError::WorkerException &&
            publisher.EndCount() == 1,
            "post-start worker exception self-finalizes and StopAndReport returns its stored failure");
    }
}

void TestConcurrentCloseDuringAdapterStop() {
    const auto before = XSI::CaptureMediaResourceMetrics();
    XSI::SensoryEventChannel channel({2, XSI::QueuePolicy::LatestOnly, {}});
    SignalFramePublisher publisher(channel);
    XSI::SimulatedVideoAdapter adapter(std::chrono::milliseconds(1), 1024);
    const auto started = adapter.Start(publisher);
    const auto receivedStart = channel.Receive();
    const bool frameQueued = publisher.WaitForFirstFrame(std::chrono::seconds(1));
    std::latch gate(3);
    XSI::AdapterStopResult stopResult;
    std::jthread closer([&] {
        gate.count_down();
        gate.wait();
        channel.Close();
    });
    std::jthread stopper([&] {
        gate.count_down();
        gate.wait();
        stopResult = adapter.StopAndReport();
    });
    gate.count_down();
    gate.wait();
    closer.join();
    stopper.join();
    for (;;) {
        const auto received = channel.Receive();
        if (received.status != XSI::ReceiveStatus::Event) break;
    }
    const auto after = XSI::CaptureMediaResourceMetrics();
    Check(started.Started() && receivedStart.streamSequence &&
        receivedStart.streamSequence->accepted && frameQueued && channel.IsClosed() &&
        stopResult.status != XSI::AdapterStopStatus::NotRunning &&
        channel.Stats().depth == 0 && after.liveBytes == before.liveBytes,
        "concurrent channel close and adapter stop release queued media without hanging");
}

void TestInvalidInputs() {
    auto invalidUtf8 = MakeEvent();
    invalidUtf8.parts.emplace_back(XSI::TextPart{std::string("\xc0\xaf", 2)});
    Check(XSI::ValidateEvent(invalidUtf8).has_value(),
        "rejects invalid UTF-8 without corrupting the event");

    auto missingMedia = MakeEvent();
    missingMedia.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Image, {}, "image/png"});
    Check(XSI::ValidateEvent(missingMedia).has_value(),
        "rejects missing or released media resources");

    XSI::ValidationLimits limits;
    limits.maxTextBytes = 2;
    auto tooLong = MakeEvent();
    tooLong.parts.emplace_back(XSI::TextPart{"long"});
    Check(XSI::ValidateEvent(tooLong, limits).has_value(),
        "enforces configurable text length limits");
    limits.maxTextBytes = 16;
    limits.maxMediaBytes = 4;
    auto oversizedMedia = MakeEvent();
    oversizedMedia.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Image,
        XSI::MediaResourceRef::Adopt(std::vector<std::byte>(5, std::byte{0})),
        "image/test"});
    Check(XSI::ValidateEvent(oversizedMedia, limits).has_value(),
        "enforces configurable media resource size limits");
    limits.maxMediaBytes = 16;
    limits.maxParts = 1;
    auto tooManyParts = MakeEvent();
    tooManyParts.parts.emplace_back(XSI::TextPart{"a"});
    tooManyParts.parts.emplace_back(XSI::TextPart{"b"});
    Check(XSI::ValidateEvent(tooManyParts, limits).has_value(),
        "enforces configurable event part count limits");

    auto emptyMedia = MakeEvent();
    emptyMedia.parts.emplace_back(XSI::MediaPart{
        XSI::MediaKind::Image,
        XSI::MediaResourceRef::Adopt(std::vector<std::byte>{}),
        "image/test"});
    Check(XSI::ValidateEvent(emptyMedia).has_value(),
        "rejects an empty media resource");

    auto invalidStream = MakeEvent();
    invalidStream.kind = XSI::EventKind::StreamEnded;
    invalidStream.streamId = "stream";
    invalidStream.sequence = 0;
    Check(XSI::ValidateEvent(invalidStream).has_value(),
        "rejects malformed stream end sequence");

    XSI::SensoryEventChannel channel({4, XSI::QueuePolicy::Reliable, {}});
    const auto rejected = channel.TryPublish(std::move(invalidUtf8));
    Check(rejected.status == XSI::PublishStatus::InvalidEvent &&
        rejected.validationError.has_value() && rejected.rejectedEvent.has_value(),
        "malformed publication returns a typed error and preserves the input event");
    channel.Close();
}

void TestConcurrentProducersAndConsumers() {
    constexpr std::size_t producerCount = 4;
    constexpr std::size_t consumerCount = 4;
    constexpr std::size_t messagesPerProducer = 1500;
    constexpr std::size_t expected = producerCount * messagesPerProducer;

    XSI::SensoryEventChannel channel({128, XSI::QueuePolicy::Reliable, {}});
    std::atomic<std::size_t> consumed{0};
    std::atomic<std::size_t> invalid{0};
    std::mutex idsMutex;
    std::unordered_set<std::string> ids;
    ids.reserve(expected);
    std::vector<std::thread> consumers;
    consumers.reserve(consumerCount);
    for (std::size_t i = 0; i < consumerCount; ++i) {
        consumers.emplace_back([&] {
            for (;;) {
                auto received = channel.Receive();
                if (received.status == XSI::ReceiveStatus::Closed) break;
                if (received.status != XSI::ReceiveStatus::Event ||
                    !received.event) {
                    invalid.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                {
                    std::scoped_lock lock(idsMutex);
                    if (!ids.emplace(XSI::ToString(received.event->id)).second) {
                        invalid.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    const auto began = std::chrono::steady_clock::now();
    std::vector<std::thread> producers;
    producers.reserve(producerCount);
    for (std::size_t producer = 0; producer < producerCount; ++producer) {
        producers.emplace_back([&, producer] {
            for (std::size_t sequence = 0;
                 sequence < messagesPerProducer; ++sequence) {
                const auto instance = "producer-" + std::to_string(producer);
                auto event = MakeEvent(
                    "stress", instance.c_str(), "stress-session");
                event.parts.emplace_back(XSI::TextPart{
                    std::to_string(producer) + ":" + std::to_string(sequence)});
                const auto result = channel.PublishWait(std::move(event));
                if (!result.Accepted()) {
                    invalid.fetch_add(1, std::memory_order_relaxed);
                    return;
                }
            }
        });
    }
    for (auto& producer : producers) producer.join();
    channel.Close();
    for (auto& consumer : consumers) consumer.join();
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - began).count();
    const auto stats = channel.Stats();
    const double throughput = elapsed > 0 ? expected / elapsed : 0;

    Check(consumed.load() == expected,
        "multi-producer/multi-consumer stress delivers every reliable event");
    Check(invalid.load() == 0 && ids.size() == expected,
        "concurrent delivery preserves unique event IDs and valid state");
    Check(stats.accepted == expected && stats.received == expected &&
        stats.peakDepth <= channel.Capacity(),
        "MPMC queue remains within its fixed capacity");
    std::cout << "metric mpmcEvents=" << expected
              << " durationMs=" << elapsed * 1000.0
              << " throughputEventsPerSecond=" << throughput
              << " queuePeakDepth=" << stats.peakDepth << '\n';
}

void TestShutdownAndBoundedLongRun() {
    {
        XSI::SensoryEventChannel channel({2, XSI::QueuePolicy::Reliable, {}});
        std::atomic<XSI::ReceiveStatus> status{XSI::ReceiveStatus::Event};
        std::jthread consumer([&](std::stop_token stopToken) {
            status.store(channel.Receive(stopToken).status,
                std::memory_order_relaxed);
        });
        consumer.request_stop();
        consumer.join();
        Check(status.load(std::memory_order_relaxed) ==
            XSI::ReceiveStatus::Cancelled,
            "consumer stop token wakes a blocked receive");
    }
    {
        XSI::SensoryEventChannel channel({1, XSI::QueuePolicy::Reliable, {}});
        Check(channel.TryPublish(MakeEvent()).Accepted(),
            "shutdown test fills bounded queue");
        std::atomic<XSI::PublishStatus> status{XSI::PublishStatus::Accepted};
        std::jthread producer([&] {
            status.store(channel.PublishWait(MakeEvent()).status,
                std::memory_order_relaxed);
        });
        channel.Close();
        producer.join();
        Check(status.load(std::memory_order_relaxed) == XSI::PublishStatus::Closed,
            "queue close wakes a publisher waiting for capacity");
    }
    {
        XSI::SensoryEventChannel channel({2, XSI::QueuePolicy::LatestOnly, {}});
        XSI::SimulatedVideoAdapter adapter(
            std::chrono::milliseconds(1), 1024);
        std::atomic<std::size_t> receivedCount{0};
        std::jthread consumer([&](std::stop_token stopToken) {
            while (true) {
                const auto received = channel.Receive(stopToken);
                if (received.status != XSI::ReceiveStatus::Event) break;
                receivedCount.fetch_add(1, std::memory_order_relaxed);
            }
        });
        Check(adapter.Start(channel).Started(),
            "simulated adapter starts in shutdown lifecycle test");
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        (void)adapter.Stop();
        consumer.request_stop();
        consumer.join();
        channel.Close();
        Check(channel.IsClosed() && receivedCount.load() > 0 &&
            channel.Stats().peakDepth <= channel.Capacity(),
            "adapter, consumer and channel stop without hanging");
    }

    constexpr std::size_t capacity = 4;
    constexpr std::size_t frameBytes = 4096;
    constexpr std::size_t frameCount = 10'000;
    std::uint64_t sampledPeakBytes = 0;
    std::uint64_t dropped = 0;
    std::size_t peakDepth = 0;
    {
        XSI::SensoryEventChannel channel({
            capacity, XSI::QueuePolicy::LatestOnly, {}});
        for (std::size_t sequence = 1; sequence <= frameCount; ++sequence) {
            auto event = MakeVideoFrame(sequence, frameBytes);
            sampledPeakBytes = std::max(
                sampledPeakBytes, XSI::CaptureMediaResourceMetrics().liveBytes);
            const auto result = channel.TryPublish(std::move(event));
            Check(result.Accepted(),
                "long-run latest-only video input remains publishable");
        }
        const auto stats = channel.Stats();
        dropped = stats.droppedStaleFrames;
        peakDepth = stats.peakDepth;
        Check(stats.depth <= capacity && peakDepth <= capacity,
            "long-run queue depth is bounded by configured capacity");
        channel.Close();
    }
    const auto after = XSI::CaptureMediaResourceMetrics();
    Check(dropped == frameCount - capacity,
        "long-run latest-only policy counts every stale frame dropped");
    Check(sampledPeakBytes <= (capacity + 1) * frameBytes,
        "long-run media storage stays bounded by queue plus one producer frame");
    Check(after.liveBytes == 0 && after.liveResources == 0,
        "long-run queue shutdown releases all shared media bytes");
    std::cout << "metric boundedFrames=" << frameCount
              << " frameBytes=" << frameBytes
              << " queueCapacity=" << capacity
              << " queuePeakDepth=" << peakDepth
              << " droppedFrames=" << dropped
              << " sampledPeakMediaBytes=" << sampledPeakBytes
              << " liveMediaBytesAfterClose=" << after.liveBytes << '\n';
}

} // namespace

int main() {
    TestSingleTextAndImage();
    TestMixedPartsAndOptionalIdentity();
    TestMultiSourceAndEventIdentity();
    TestContinuousAudioVideoAndSequence();
    TestSequenceGapsAndEnd();
    TestResourceCopyMoveAndThreadLifetime();
    TestQueueCapacityAndLatestOnly();
    TestMixedLatestOnlyCapacityBounds();
    TestOrderedStreamReceiveAndProducerOrder();
    TestLatestOnlyStreamGapsAndEndDelivery();
    TestDisjointLatestOnlyDropsAndTrackingSaturation();
    TestSharedValidationLimits();
    TestVideoAdapterConfigurationAndWorkerFailures();
    TestConcurrentCloseDuringAdapterStop();
    TestInvalidInputs();
    TestConcurrentProducersAndConsumers();
    TestShutdownAndBoundedLongRun();
    std::cout << "XSI checks=" << gChecks << " failures=" << gFailures << '\n';
    return gFailures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
