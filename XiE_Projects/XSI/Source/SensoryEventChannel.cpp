#include "XSI/SensoryEventChannel.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace XSI {
namespace {

constexpr std::size_t MaximumQueueCapacity = 65'536;
constexpr std::size_t MaximumVideoCandidatesToCheck = 1'024;

ReceivedTime CaptureReceivedTime() noexcept {
    const auto wall = std::chrono::system_clock::now().time_since_epoch();
    const auto monotonic = std::chrono::steady_clock::now().time_since_epoch();
    return {
        std::chrono::duration_cast<std::chrono::nanoseconds>(wall).count(),
        std::chrono::duration_cast<std::chrono::nanoseconds>(monotonic).count()};
}

bool IsKnownPolicy(QueuePolicy policy) noexcept {
    switch (policy) {
    case QueuePolicy::Reliable:
    case QueuePolicy::LatestOnly:
        return true;
    }
    return false;
}

} // namespace

SensoryEventChannel::SensoryEventChannel(QueueOptions options)
    : m_Options(std::move(options)),
      m_StreamSequenceValidator(m_Options.maxActiveStreams,
          m_Options.validationLimits) {
    m_ValidConfiguration = m_Options.capacity > 0 &&
        m_Options.capacity <= MaximumQueueCapacity &&
        IsKnownPolicy(m_Options.policy);
    if (!m_ValidConfiguration) return;

    const auto capacity = m_Options.capacity;
    m_Slots.resize(capacity);
    m_QueueNext.assign(capacity, InvalidSlot);
    m_QueuePrevious.assign(capacity, InvalidSlot);
    m_VideoNext.assign(capacity, InvalidSlot);
    m_VideoPrevious.assign(capacity, InvalidSlot);
    m_FreeSlots.resize(capacity);
    for (std::size_t index = 0; index < capacity; ++index) {
        m_FreeSlots[index] = capacity - index - 1;
    }
    m_FreeCount = capacity;
}

SensoryEventChannel::~SensoryEventChannel() {
    Close();
}

PublishResult SensoryEventChannel::TryPublish(SensoryEvent event) {
    if (!m_ValidConfiguration) {
        return {PublishStatus::InvalidConfiguration, std::nullopt,
            std::move(event)};
    }
    if (auto error = ValidateEvent(event, m_Options.validationLimits)) {
        return {PublishStatus::InvalidEvent, std::move(error),
            std::move(event)};
    }

    std::unique_lock lock(m_Mutex);
    if (m_Closed) {
        return {PublishStatus::Closed, std::nullopt, std::move(event)};
    }
    event.receivedTime = CaptureReceivedTime();
    auto result = ValidateAndPublishLocked(std::move(event));
    lock.unlock();
    if (result.Accepted()) {
        m_NotEmpty.notify_one();
        m_NotFull.notify_all();
    }
    return result;
}

PublishResult SensoryEventChannel::PublishWait(
    SensoryEvent event, std::stop_token stopToken) {
    if (!m_ValidConfiguration) {
        return {PublishStatus::InvalidConfiguration, std::nullopt,
            std::move(event)};
    }
    if (auto error = ValidateEvent(event, m_Options.validationLimits)) {
        return {PublishStatus::InvalidEvent, std::move(error),
            std::move(event)};
    }

    std::unique_lock lock(m_Mutex);
    const auto canPublish = [&] {
        return m_Closed || m_Count < m_Options.capacity ||
            (m_Options.policy == QueuePolicy::LatestOnly &&
             HasReplaceableVideoFrameLocked());
    };
    if (!m_NotFull.wait(lock, stopToken, canPublish)) {
        return {PublishStatus::Cancelled, std::nullopt, std::move(event)};
    }
    if (m_Closed) {
        return {PublishStatus::Closed, std::nullopt, std::move(event)};
    }
    event.receivedTime = CaptureReceivedTime();
    auto result = ValidateAndPublishLocked(std::move(event));
    lock.unlock();
    if (result.Accepted()) {
        m_NotEmpty.notify_one();
        m_NotFull.notify_all();
    }
    return result;
}

PublishResult SensoryEventChannel::PublishFor(
    SensoryEvent event, std::chrono::milliseconds timeout,
    std::stop_token stopToken) {
    if (!m_ValidConfiguration) {
        return {PublishStatus::InvalidConfiguration, std::nullopt,
            std::move(event)};
    }
    if (auto error = ValidateEvent(event, m_Options.validationLimits)) {
        return {PublishStatus::InvalidEvent, std::move(error),
            std::move(event)};
    }

    std::unique_lock lock(m_Mutex);
    const auto canPublish = [&] {
        return m_Closed || m_Count < m_Options.capacity ||
            (m_Options.policy == QueuePolicy::LatestOnly &&
             HasReplaceableVideoFrameLocked());
    };
    if (!m_NotFull.wait_for(lock, stopToken, timeout, canPublish)) {
        const auto status = stopToken.stop_requested()
            ? PublishStatus::Cancelled : PublishStatus::TimedOut;
        return {status, std::nullopt, std::move(event)};
    }
    if (m_Closed) {
        return {PublishStatus::Closed, std::nullopt, std::move(event)};
    }
    event.receivedTime = CaptureReceivedTime();
    auto result = ValidateAndPublishLocked(std::move(event));
    lock.unlock();
    if (result.Accepted()) {
        m_NotEmpty.notify_one();
        m_NotFull.notify_all();
    }
    return result;
}

StreamCleanupResult SensoryEventChannel::AbandonStream(
    std::string_view sourceAdapter, std::string_view sourceInstance,
    std::string_view channel, std::string_view streamId) noexcept {
    try {
        std::unique_lock lock(m_Mutex);
        std::size_t removed = 0;
        auto slot = m_QueueHead;
        while (slot != InvalidSlot) {
            const auto next = m_QueueNext[slot];
            const auto& event = *m_Slots[slot];
            if (event.source.adapter == sourceAdapter &&
                event.source.instance == sourceInstance &&
                event.channel == channel && event.streamId &&
                *event.streamId == streamId) {
                if (IsReplaceableVideoFrame(event)) UnlinkVideoSlotLocked(slot);
                UnlinkQueueSlotLocked(slot);
                m_Slots[slot].reset();
                ReleaseSlotLocked(slot);
                --m_Count;
                ++removed;
            }
            slot = next;
        }
        const bool stateRemoved = m_StreamSequenceValidator.ForgetStream(
            sourceAdapter, sourceInstance, channel, streamId);
        m_Stats.abandonedStreamEvents += removed;
        m_Stats.depth = m_Count;
        lock.unlock();
        if (removed > 0) m_NotFull.notify_all();
        return {stateRemoved || removed > 0, removed};
    } catch (...) {
        return {};
    }
}

ReceiveResult SensoryEventChannel::Receive(std::stop_token stopToken) {
    if (!m_ValidConfiguration) {
        return {ReceiveStatus::InvalidConfiguration, std::nullopt};
    }

    std::unique_lock lock(m_Mutex);
    const auto canReceive = [&] { return m_Count > 0 || m_Closed; };
    if (!m_NotEmpty.wait(lock, stopToken, canReceive)) {
        return {ReceiveStatus::Cancelled, std::nullopt};
    }
    if (m_Count == 0) return {ReceiveStatus::Closed, std::nullopt};

    const auto slot = m_QueueHead;
    std::optional<SensoryEvent> event = std::move(m_Slots[slot]);
    if (IsReplaceableVideoFrame(*event)) UnlinkVideoSlotLocked(slot);
    UnlinkQueueSlotLocked(slot);
    m_Slots[slot].reset();
    ReleaseSlotLocked(slot);
    --m_Count;
    ++m_Stats.received;
    m_Stats.depth = m_Count;

    std::optional<StreamSequenceResult> sequenceResult;
    if (event->kind != EventKind::Discrete) {
        sequenceResult = m_StreamSequenceValidator.Accept(
            *event, m_Options.policy == QueuePolicy::LatestOnly);
    }
    lock.unlock();
    m_NotFull.notify_all();
    return {ReceiveStatus::Event, std::move(event), std::move(sequenceResult)};
}

void SensoryEventChannel::Close() noexcept {
    {
        std::scoped_lock lock(m_Mutex);
        m_Closed = true;
    }
    m_NotEmpty.notify_all();
    m_NotFull.notify_all();
}

bool SensoryEventChannel::IsClosed() const noexcept {
    std::scoped_lock lock(m_Mutex);
    return m_Closed;
}

std::size_t SensoryEventChannel::Capacity() const noexcept {
    return m_ValidConfiguration ? m_Options.capacity : 0;
}

QueuePolicy SensoryEventChannel::Policy() const noexcept {
    return m_Options.policy;
}

QueueStats SensoryEventChannel::Stats() const noexcept {
    std::scoped_lock lock(m_Mutex);
    QueueStats result = m_Stats;
    result.depth = m_Count;
    return result;
}

PublishResult SensoryEventChannel::ValidateAndPublishLocked(
    SensoryEvent&& event) {
    bool replaced = false;
    std::size_t slot = InvalidSlot;
    if (m_Count == m_Options.capacity) {
        if (m_Options.policy != QueuePolicy::LatestOnly ||
            !HasReplaceableVideoFrameLocked()) {
            ++m_Stats.rejectedFull;
            return {PublishStatus::Full, std::nullopt, std::move(event)};
        }

        auto staleSlot = m_VideoHead;
        std::size_t checkedCandidates = 0;
        bool recordedDrop = false;
        while (staleSlot != InvalidSlot &&
            checkedCandidates < MaximumVideoCandidatesToCheck) {
            ++checkedCandidates;
            if (m_StreamSequenceValidator.NoteDroppedVideoFrame(
                    *m_Slots[staleSlot])) {
                recordedDrop = true;
                break;
            }
            staleSlot = m_VideoNext[staleSlot];
        }
        if (!recordedDrop) {
            ++m_Stats.rejectedFull;
            return {PublishStatus::Full, std::nullopt, std::move(event)};
        }
        slot = EraseVideoFrameLocked(staleSlot);
        ++m_Stats.droppedStaleFrames;
        replaced = true;
    } else {
        slot = AllocateSlotLocked();
    }

    if (slot == InvalidSlot) {
        ++m_Stats.rejectedFull;
        return {PublishStatus::Full, std::nullopt, std::move(event)};
    }
    const bool replaceable = IsReplaceableVideoFrame(event);
    m_Slots[slot].emplace(std::move(event));
    AddQueueTailLocked(slot);
    if (replaceable) AddVideoTailLocked(slot);
    ++m_Count;
    ++m_Stats.accepted;
    m_Stats.depth = m_Count;
    m_Stats.peakDepth = std::max(m_Stats.peakDepth, m_Count);
    return {
        replaced ? PublishStatus::AcceptedReplacingStaleFrame
                 : PublishStatus::Accepted,
        std::nullopt};
}

bool SensoryEventChannel::HasReplaceableVideoFrameLocked() const noexcept {
    return m_VideoHead != InvalidSlot;
}

std::size_t SensoryEventChannel::EraseVideoFrameLocked(
    std::size_t slot) noexcept {
    if (slot == InvalidSlot) return InvalidSlot;
    UnlinkVideoSlotLocked(slot);
    UnlinkQueueSlotLocked(slot);
    m_Slots[slot].reset();
    --m_Count;
    m_Stats.depth = m_Count;
    return slot;
}

void SensoryEventChannel::UnlinkQueueSlotLocked(std::size_t slot) noexcept {
    const auto previous = m_QueuePrevious[slot];
    const auto next = m_QueueNext[slot];
    if (previous == InvalidSlot) m_QueueHead = next;
    else m_QueueNext[previous] = next;
    if (next == InvalidSlot) m_QueueTail = previous;
    else m_QueuePrevious[next] = previous;
    m_QueueNext[slot] = InvalidSlot;
    m_QueuePrevious[slot] = InvalidSlot;
}

void SensoryEventChannel::UnlinkVideoSlotLocked(std::size_t slot) noexcept {
    const auto previous = m_VideoPrevious[slot];
    const auto next = m_VideoNext[slot];
    if (previous == InvalidSlot) m_VideoHead = next;
    else m_VideoNext[previous] = next;
    if (next == InvalidSlot) m_VideoTail = previous;
    else m_VideoPrevious[next] = previous;
    m_VideoNext[slot] = InvalidSlot;
    m_VideoPrevious[slot] = InvalidSlot;
}

void SensoryEventChannel::AddQueueTailLocked(std::size_t slot) noexcept {
    m_QueuePrevious[slot] = m_QueueTail;
    m_QueueNext[slot] = InvalidSlot;
    if (m_QueueTail == InvalidSlot) m_QueueHead = slot;
    else m_QueueNext[m_QueueTail] = slot;
    m_QueueTail = slot;
}

void SensoryEventChannel::AddVideoTailLocked(std::size_t slot) noexcept {
    m_VideoPrevious[slot] = m_VideoTail;
    m_VideoNext[slot] = InvalidSlot;
    if (m_VideoTail == InvalidSlot) m_VideoHead = slot;
    else m_VideoNext[m_VideoTail] = slot;
    m_VideoTail = slot;
}

std::size_t SensoryEventChannel::AllocateSlotLocked() noexcept {
    if (m_FreeCount == 0) return InvalidSlot;
    return m_FreeSlots[--m_FreeCount];
}

void SensoryEventChannel::ReleaseSlotLocked(std::size_t slot) noexcept {
    if (m_FreeCount < m_FreeSlots.size()) {
        m_FreeSlots[m_FreeCount++] = slot;
    }
}

bool SensoryEventChannel::IsReplaceableVideoFrame(
    const SensoryEvent& event) noexcept {
    if (event.kind != EventKind::StreamData || !event.streamId ||
        !event.sequence || event.parts.empty()) {
        return false;
    }
    for (const auto& part : event.parts) {
        const auto* media = std::get_if<MediaPart>(&part);
        if (media == nullptr || media->kind != MediaKind::Video) return false;
    }
    return true;
}

} // namespace XSI
