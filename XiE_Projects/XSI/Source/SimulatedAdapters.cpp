#include "XSI/SensoryAdapter.h"

#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace XSI {
namespace {

constexpr std::size_t MaximumSimulatedFrameBytes = 64 * 1024 * 1024;
constexpr std::int64_t MaximumFrameIntervalMilliseconds = 60'000;
constexpr auto StreamEndPublishTimeout = std::chrono::milliseconds(100);
constexpr std::string_view VideoAdapterName = "simulated-video";
constexpr std::string_view VideoInstanceName = "camera-01";
constexpr std::string_view VideoChannelName = "room-01";

SensoryEvent MakeStreamEvent(EventKind kind, const std::string& streamId,
    std::uint64_t sequence) {
    SensoryEvent event;
    event.source = {std::string(VideoAdapterName), std::string(VideoInstanceName)};
    event.channel = VideoChannelName;
    event.kind = kind;
    event.occurredTime = CurrentUtcTime();
    event.streamId = streamId;
    event.sequence = sequence;
    return event;
}

AdapterError ValidateConfiguration(
    std::chrono::milliseconds frameInterval, std::size_t frameBytes) noexcept {
    if (frameBytes == 0 || frameBytes > MaximumSimulatedFrameBytes ||
        frameBytes > std::vector<std::byte>{}.max_size()) {
        return AdapterError::InvalidFrameSize;
    }
    if (frameInterval.count() <= 0 ||
        frameInterval.count() > MaximumFrameIntervalMilliseconds) {
        return AdapterError::InvalidFrameInterval;
    }
    return AdapterError::None;
}

} // namespace

AdapterStartResult SimulatedTextImageAdapter::Start(
    ISensoryEventPublisher& publisher) {
    if (m_Started) {
        return {AdapterStatus::AlreadyRunning,
            {PublishStatus::Accepted, std::nullopt, std::nullopt}};
    }

    SensoryEvent event;
    event.source = {"simulated-chat", "account-01"};
    event.channel = "conversation-42";
    event.actor = "user-7";
    event.externalId = ExternalId{"simchat:message", "message-001"};
    event.kind = EventKind::Discrete;
    event.occurredTime = CurrentUtcTime();
    event.parts.reserve(4);
    event.parts.emplace_back(TextPart{
        "\xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x8C\x58\x53\x49\xE3\x80\x82"});

    std::vector<std::byte> firstImage(2048, std::byte{0x2a});
    event.parts.emplace_back(MediaPart{
        MediaKind::Image,
        MediaResourceRef::Adopt(std::move(firstImage)),
        "image/png",
        std::nullopt,
        std::nullopt});
    event.parts.emplace_back(TextPart{
        "\xE7\xAC\xAC\xE4\xBA\x8C\xE5\xBC\xA0\xE5\x9B\xBE\xE7\x89\x87\xE5\x9C\xA8\xE8\xBF\x99\xE6\x9D\xA1\xE6\x96\x87\xE5\xAD\x97\xE4\xB9\x8B\xE5\x90\x8E\xE3\x80\x82"});

    std::vector<std::byte> secondImage(4096, std::byte{0x7f});
    event.parts.emplace_back(MediaPart{
        MediaKind::Image,
        MediaResourceRef::Adopt(std::move(secondImage)),
        "image/jpeg",
        std::nullopt,
        std::nullopt});

    const auto result = publisher.TryPublish(std::move(event));
    if (!result.Accepted()) {
        return {AdapterStatus::PublishRejected, result};
    }
    m_Started = true;
    return {AdapterStatus::Started, result};
}

void SimulatedTextImageAdapter::Stop() noexcept {
    (void)StopAndReport();
}

AdapterStopResult SimulatedTextImageAdapter::StopAndReport(
    std::stop_token cancellationToken) noexcept {
    (void)cancellationToken;
    const bool wasStarted = m_Started;
    m_Started = false;
    return {wasStarted ? AdapterStopStatus::Stopped
                       : AdapterStopStatus::NotRunning};
}

SimulatedVideoAdapter::SimulatedVideoAdapter(
    std::chrono::milliseconds frameInterval, std::size_t frameBytes)
    : m_FrameInterval(frameInterval),
      m_FrameBytes(frameBytes),
      m_ConfigurationError(ValidateConfiguration(frameInterval, frameBytes)) {}

SimulatedVideoAdapter::~SimulatedVideoAdapter() {
    Stop();
}

AdapterStartResult SimulatedVideoAdapter::Start(
    ISensoryEventPublisher& publisher) {
    std::scoped_lock lifecycleLock(m_LifecycleMutex);
    if (m_Worker.joinable() || !m_StreamId.empty()) {
        return {AdapterStatus::AlreadyRunning,
            {PublishStatus::Accepted, std::nullopt, std::nullopt}};
    }

    {
        std::scoped_lock statusLock(m_StatusMutex);
        m_RuntimeStatus = {};
        m_LastStopResult = {};
    }

    if (m_ConfigurationError != AdapterError::None) {
        {
            std::scoped_lock statusLock(m_StatusMutex);
            m_RuntimeStatus = {
                AdapterRunState::Failed, m_ConfigurationError, std::nullopt};
        }
        return {AdapterStatus::InvalidConfiguration,
            {PublishStatus::InvalidConfiguration, std::nullopt, std::nullopt},
            m_ConfigurationError};
    }

    try {
        m_StreamId = "video-" + ToString(GenerateEventId());
        m_NextSequence = 1;
        auto started = MakeStreamEvent(EventKind::StreamStarted, m_StreamId, 0);
        auto publishResult = publisher.TryPublish(std::move(started));
        if (!publishResult.Accepted()) {
            m_StreamId.clear();
            {
                std::scoped_lock statusLock(m_StatusMutex);
                m_RuntimeStatus = {
                    AdapterRunState::Failed, AdapterError::FramePublishFailed,
                    publishResult.status};
            }
            return {AdapterStatus::PublishRejected, std::move(publishResult),
                AdapterError::FramePublishFailed};
        }

        m_Publisher = &publisher;
        {
            std::scoped_lock statusLock(m_StatusMutex);
            m_RuntimeStatus = {AdapterRunState::Running, AdapterError::None,
                std::nullopt};
        }
        auto* publisherPointer = &publisher;
        try {
            m_Worker = std::jthread([this, publisherPointer](std::stop_token stopToken) {
                Run(stopToken, publisherPointer);
            });
        } catch (...) {
            RecordFailure(AdapterError::ThreadStartFailed);
            (void)FinalizeStream({});
            return {AdapterStatus::InitializationFailed,
                std::move(publishResult), AdapterError::ThreadStartFailed};
        }
        return {AdapterStatus::Started, std::move(publishResult)};
    } catch (...) {
        m_StreamId.clear();
        m_Publisher = nullptr;
        RecordFailure(AdapterError::WorkerException);
        return {AdapterStatus::InitializationFailed,
            {PublishStatus::InvalidConfiguration, std::nullopt, std::nullopt},
            AdapterError::WorkerException};
    }
}

void SimulatedVideoAdapter::Stop() noexcept {
    (void)StopAndReport();
}

AdapterStopResult SimulatedVideoAdapter::StopAndReport(
    std::stop_token cancellationToken) noexcept {
    try {
        std::scoped_lock lifecycleLock(m_LifecycleMutex);
        if (m_Worker.joinable()) {
            m_Worker.request_stop();
            try {
                m_Worker.join();
            } catch (...) {
                RecordFailure(AdapterError::WorkerException);
            }
        }
        if (m_StreamId.empty() || m_Publisher == nullptr) {
            std::scoped_lock statusLock(m_StatusMutex);
            if (m_LastStopResult.status != AdapterStopStatus::NotRunning) {
                return m_LastStopResult;
            }
            return {AdapterStopStatus::NotRunning, false, false, false,
                PublishStatus::Accepted, m_RuntimeStatus.error};
        }
        return FinalizeStream(cancellationToken);
    } catch (...) {
        RecordFailure(AdapterError::WorkerException);
        const auto runtime = RuntimeStatus();
        return {AdapterStopStatus::AbnormalTermination, false, true, false,
            PublishStatus::InvalidConfiguration, runtime.error};
    }
}

AdapterRuntimeStatus SimulatedVideoAdapter::RuntimeStatus() const noexcept {
    std::scoped_lock statusLock(m_StatusMutex);
    return m_RuntimeStatus;
}

void SimulatedVideoAdapter::Run(
    std::stop_token stopToken, ISensoryEventPublisher* publisher) noexcept {
    try {
        const auto frameDurationCount =
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                m_FrameInterval).count();
        if (frameDurationCount <= 0) {
            RecordFailure(AdapterError::InvalidFrameInterval);
            return;
        }
        const auto frameDuration = static_cast<std::uint64_t>(frameDurationCount);
        std::condition_variable_any timer;
        std::mutex timerMutex;

        while (!stopToken.stop_requested()) {
            if (m_NextSequence == std::numeric_limits<std::uint64_t>::max()) {
                RecordFailure(AdapterError::WorkerException);
                break;
            }
            const auto frameIndex = m_NextSequence - 1;
            if (frameIndex > std::numeric_limits<std::uint64_t>::max() /
                    frameDuration) {
                RecordFailure(AdapterError::WorkerException);
                break;
            }

            SensoryEvent event = MakeStreamEvent(
                EventKind::StreamData, m_StreamId, m_NextSequence);
            std::vector<std::byte> frame(m_FrameBytes);
            const auto frameMark = static_cast<std::byte>(m_NextSequence & 0xff);
            std::fill(frame.begin(), frame.end(), frameMark);
            event.parts.emplace_back(MediaPart{
                MediaKind::Video,
                MediaResourceRef::Adopt(std::move(frame)),
                "video/x-simulated-rgb-frame",
                frameIndex * frameDuration,
                frameDuration});

            const auto result = publisher->PublishWait(std::move(event), stopToken);
            if (!result.Accepted()) {
                if (!(stopToken.stop_requested() &&
                      result.status == PublishStatus::Cancelled)) {
                    RecordFailure(AdapterError::FramePublishFailed, result.status);
                }
                break;
            }
            ++m_NextSequence;

            std::unique_lock timerLock(timerMutex);
            (void)timer.wait_for(timerLock, stopToken, m_FrameInterval,
                [] { return false; });
        }
    } catch (...) {
        RecordFailure(AdapterError::WorkerException);
        (void)FinalizeStream({});
        return;
    }
    if (RuntimeStatus().state == AdapterRunState::Failed) {
        (void)FinalizeStream({});
    }
}

AdapterStopResult SimulatedVideoAdapter::FinalizeStream(
    std::stop_token cancellationToken) noexcept {
    AdapterStopResult stopResult;
    stopResult.status = AdapterStopStatus::AbnormalTermination;
    stopResult.streamEndStatus = PublishStatus::InvalidConfiguration;
    try {
        auto ended = MakeStreamEvent(
            EventKind::StreamEnded, m_StreamId, m_NextSequence);
        const auto publishResult = m_Publisher->PublishFor(
            std::move(ended), StreamEndPublishTimeout, cancellationToken);
        stopResult.streamEndStatus = publishResult.status;
        stopResult.streamEndPublished = publishResult.Accepted();
        if (!stopResult.streamEndPublished) {
            const auto cleanup = m_Publisher->AbandonStream(
                VideoAdapterName, VideoInstanceName, VideoChannelName, m_StreamId);
            stopResult.streamStateCleared = cleanup.stateCleared;
            RecordFailure(AdapterError::StreamEndPublishFailed,
                publishResult.status);
        }
    } catch (...) {
        RecordFailure(AdapterError::StreamEndPublishFailed,
            stopResult.streamEndStatus);
        try {
            const auto cleanup = m_Publisher->AbandonStream(
                VideoAdapterName, VideoInstanceName, VideoChannelName, m_StreamId);
            stopResult.streamStateCleared = cleanup.stateCleared;
        } catch (...) {
        }
    }

    const auto runtime = RuntimeStatus();
    stopResult.error = runtime.error;
    stopResult.streamAbnormallyTerminated =
        !stopResult.streamEndPublished || runtime.error != AdapterError::None;
    stopResult.status = stopResult.streamAbnormallyTerminated
        ? AdapterStopStatus::AbnormalTermination : AdapterStopStatus::Stopped;
    {
        std::scoped_lock statusLock(m_StatusMutex);
        m_RuntimeStatus.state = stopResult.streamAbnormallyTerminated
            ? AdapterRunState::Failed : AdapterRunState::Stopped;
        m_LastStopResult = stopResult;
    }
    m_StreamId.clear();
    m_Publisher = nullptr;
    return stopResult;
}

void SimulatedVideoAdapter::RecordFailure(
    AdapterError error, std::optional<PublishStatus> publishStatus) noexcept {
    try {
        std::scoped_lock statusLock(m_StatusMutex);
        if (m_RuntimeStatus.error == AdapterError::None) {
            m_RuntimeStatus.error = error;
        }
        if (publishStatus) m_RuntimeStatus.lastPublishStatus = publishStatus;
        m_RuntimeStatus.state = AdapterRunState::Failed;
    } catch (...) {
    }
}

} // namespace XSI
