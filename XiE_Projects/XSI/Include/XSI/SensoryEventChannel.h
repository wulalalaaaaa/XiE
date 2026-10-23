#pragma once

#include "XSI/SensoryEvent.h"
#include "XSI/StreamSequenceValidator.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string_view>
#include <utility>
#include <vector>

namespace XSI {

enum class QueuePolicy : std::uint8_t {
    Reliable,
    LatestOnly,
};

struct QueueOptions {
    std::size_t capacity = 256;
    QueuePolicy policy = QueuePolicy::Reliable;
    ValidationLimits validationLimits{};
    std::size_t maxActiveStreams = 1024;
};

enum class PublishStatus : std::uint8_t {
    Accepted,
    AcceptedReplacingStaleFrame,
    Full,
    Closed,
    Cancelled,
    TimedOut,
    InvalidConfiguration,
    InvalidEvent,
};

struct PublishResult {
    PublishStatus status = PublishStatus::InvalidConfiguration;
    std::optional<ValidationError> validationError;
    std::optional<SensoryEvent> rejectedEvent;

    [[nodiscard]] bool Accepted() const noexcept {
        return status == PublishStatus::Accepted ||
            status == PublishStatus::AcceptedReplacingStaleFrame;
    }
};

enum class ReceiveStatus : std::uint8_t {
    Event,
    Closed,
    Cancelled,
    InvalidConfiguration,
};

struct ReceiveResult {
    ReceiveStatus status = ReceiveStatus::InvalidConfiguration;
    std::optional<SensoryEvent> event;
    // Stream sequence state is processed in dequeue order before return.
    std::optional<StreamSequenceResult> streamSequence;
};

struct QueueStats {
    std::uint64_t accepted = 0;
    std::uint64_t received = 0;
    std::uint64_t rejectedFull = 0;
    std::uint64_t droppedStaleFrames = 0;
    std::uint64_t abandonedStreamEvents = 0;
    std::size_t depth = 0;
    std::size_t peakDepth = 0;
};

struct StreamCleanupResult {
    bool stateCleared = false;
    std::size_t queuedEventsRemoved = 0;
};

class ISensoryEventPublisher {
public:
    virtual ~ISensoryEventPublisher() = default;
    [[nodiscard]] virtual PublishResult TryPublish(SensoryEvent event) = 0;
    [[nodiscard]] virtual PublishResult PublishWait(
        SensoryEvent event, std::stop_token stopToken = {}) = 0;
    // Publishers without a timed wait may attempt immediate publication.
    [[nodiscard]] virtual PublishResult PublishFor(
        SensoryEvent event, std::chrono::milliseconds timeout,
        std::stop_token stopToken = {}) {
        (void)timeout;
        if (stopToken.stop_requested()) {
            return {PublishStatus::Cancelled, std::nullopt, std::move(event)};
        }
        return TryPublish(std::move(event));
    }
    // Recovery hook used when a stream end cannot be delivered.
    [[nodiscard]] virtual StreamCleanupResult AbandonStream(
        std::string_view sourceAdapter, std::string_view sourceInstance,
        std::string_view channel, std::string_view streamId) noexcept {
        (void)sourceAdapter;
        (void)sourceInstance;
        (void)channel;
        (void)streamId;
        return {};
    }
};

class ISensoryEventReceiver {
public:
    virtual ~ISensoryEventReceiver() = default;
    [[nodiscard]] virtual ReceiveResult Receive(
        std::stop_token stopToken = {}) = 0;
};

// A bounded MPMC channel. Failed publication returns the original event.
// LatestOnly can replace only pure-video StreamData events. Stream sequence
// state is updated while dequeuing, before a consumer can reorder processing.
class SensoryEventChannel final : public ISensoryEventPublisher,
                                  public ISensoryEventReceiver {
public:
    explicit SensoryEventChannel(QueueOptions options = {});
    ~SensoryEventChannel() override;
    SensoryEventChannel(const SensoryEventChannel&) = delete;
    SensoryEventChannel& operator=(const SensoryEventChannel&) = delete;

    [[nodiscard]] PublishResult TryPublish(SensoryEvent event) override;
    [[nodiscard]] PublishResult PublishWait(
        SensoryEvent event, std::stop_token stopToken = {}) override;
    [[nodiscard]] PublishResult PublishFor(
        SensoryEvent event, std::chrono::milliseconds timeout,
        std::stop_token stopToken = {}) override;
    [[nodiscard]] StreamCleanupResult AbandonStream(
        std::string_view sourceAdapter, std::string_view sourceInstance,
        std::string_view channel, std::string_view streamId) noexcept override;
    [[nodiscard]] ReceiveResult Receive(
        std::stop_token stopToken = {}) override;
    void Close() noexcept;
    [[nodiscard]] bool IsClosed() const noexcept;
    [[nodiscard]] std::size_t Capacity() const noexcept;
    [[nodiscard]] QueuePolicy Policy() const noexcept;
    [[nodiscard]] QueueStats Stats() const noexcept;

private:
    static constexpr std::size_t InvalidSlot =
        std::numeric_limits<std::size_t>::max();

    [[nodiscard]] PublishResult ValidateAndPublishLocked(SensoryEvent&& event);
    [[nodiscard]] bool HasReplaceableVideoFrameLocked() const noexcept;
    [[nodiscard]] std::size_t EraseVideoFrameLocked(std::size_t slot) noexcept;
    void UnlinkQueueSlotLocked(std::size_t slot) noexcept;
    void UnlinkVideoSlotLocked(std::size_t slot) noexcept;
    void AddQueueTailLocked(std::size_t slot) noexcept;
    void AddVideoTailLocked(std::size_t slot) noexcept;
    [[nodiscard]] std::size_t AllocateSlotLocked() noexcept;
    void ReleaseSlotLocked(std::size_t slot) noexcept;
    [[nodiscard]] static bool IsReplaceableVideoFrame(
        const SensoryEvent& event) noexcept;

    QueueOptions m_Options;
    StreamSequenceValidator m_StreamSequenceValidator;
    bool m_ValidConfiguration = false;
    mutable std::mutex m_Mutex;
    std::condition_variable_any m_NotEmpty;
    std::condition_variable_any m_NotFull;
    std::vector<std::optional<SensoryEvent>> m_Slots;
    std::vector<std::size_t> m_QueueNext;
    std::vector<std::size_t> m_QueuePrevious;
    std::vector<std::size_t> m_VideoNext;
    std::vector<std::size_t> m_VideoPrevious;
    std::vector<std::size_t> m_FreeSlots;
    std::size_t m_FreeCount = 0;
    std::size_t m_QueueHead = InvalidSlot;
    std::size_t m_QueueTail = InvalidSlot;
    std::size_t m_VideoHead = InvalidSlot;
    std::size_t m_VideoTail = InvalidSlot;
    std::size_t m_Count = 0;
    bool m_Closed = false;
    QueueStats m_Stats{};
};

} // namespace XSI
