#include "XSI/StreamSequenceValidator.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

namespace XSI {

StreamSequenceValidator::StreamSequenceValidator(std::size_t maxActiveStreams)
    : StreamSequenceValidator(maxActiveStreams, ValidationLimits{}) {}

StreamSequenceValidator::StreamSequenceValidator(
    std::size_t maxActiveStreams, ValidationLimits validationLimits)
    : m_MaxActiveStreams(maxActiveStreams),
      m_ValidationLimits(std::move(validationLimits)) {}

StreamSequenceResult StreamSequenceValidator::Accept(
    const SensoryEvent& event, bool allowDroppedFrameGaps) {
    if (ValidateEvent(event, m_ValidationLimits)) {
        return {false, StreamSequenceError::InvalidEvent, 0, 0};
    }
    if (event.kind == EventKind::Discrete) {
        return {true, std::nullopt, 0, 0};
    }

    Key key{event.source.adapter, event.source.instance, event.channel,
        *event.streamId};
    std::scoped_lock lock(m_Mutex);
    if (event.kind == EventKind::StreamStarted) {
        if (m_NextSequence.contains(key)) {
            return {false, StreamSequenceError::DuplicateStreamStart, 0, 0};
        }
        if (m_NextSequence.size() >= m_MaxActiveStreams) {
            return {false, StreamSequenceError::TooManyActiveStreams, 0, 0};
        }
        m_NextSequence.emplace(std::move(key), 1);
        return {true, std::nullopt, 1, 0};
    }

    const auto iterator = m_NextSequence.find(key);
    if (iterator == m_NextSequence.end()) {
        return {false, StreamSequenceError::StreamNotStarted, 0, 0};
    }
    const std::uint64_t sequence = *event.sequence;
    const std::uint64_t expected = iterator->second;
    if (sequence < expected) {
        return {false, StreamSequenceError::UnexpectedSequence, expected, 0};
    }

    const std::uint64_t skipped = sequence - expected;
    auto dropped = m_DroppedRanges.find(key);
    if (skipped > 0) {
        bool fullyCovered = false;
        if (allowDroppedFrameGaps && dropped != m_DroppedRanges.end()) {
            auto nextMissing = expected;
            const auto lastMissing = sequence - 1;
            for (const auto& range : dropped->second) {
                if (range.last < nextMissing) continue;
                if (range.first > nextMissing) break;
                if (range.last >= lastMissing) {
                    fullyCovered = true;
                    break;
                }
                if (range.last == std::numeric_limits<std::uint64_t>::max()) {
                    break;
                }
                nextMissing = range.last + 1;
            }
        }
        if (!fullyCovered) {
            return {false, StreamSequenceError::UnexpectedSequence, expected, 0};
        }
    }

    if (event.kind == EventKind::StreamEnded) {
        m_NextSequence.erase(iterator);
        if (dropped != m_DroppedRanges.end()) m_DroppedRanges.erase(dropped);
        return {true, std::nullopt, expected, skipped};
    }
    if (sequence == std::numeric_limits<std::uint64_t>::max()) {
        return {false, StreamSequenceError::SequenceOverflow, expected, 0};
    }

    iterator->second = sequence + 1;
    if (dropped != m_DroppedRanges.end()) {
        const auto newExpected = sequence + 1;
        auto& ranges = dropped->second;
        ranges.erase(std::remove_if(ranges.begin(), ranges.end(),
            [newExpected](const DroppedRange& range) {
                return range.last < newExpected;
            }), ranges.end());
        if (!ranges.empty() && ranges.front().first < newExpected) {
            ranges.front().first = newExpected;
        }
        if (ranges.empty()) {
            m_DroppedRanges.erase(dropped);
        }
    }
    return {true, std::nullopt, iterator->second, skipped};
}

bool StreamSequenceValidator::NoteDroppedVideoFrame(
    const SensoryEvent& event) {
    if (!IsPureVideoFrame(event) || ValidateEvent(event, m_ValidationLimits)) {
        return false;
    }

    Key key{event.source.adapter, event.source.instance, event.channel,
        *event.streamId};
    const std::uint64_t sequence = *event.sequence;
    std::scoped_lock lock(m_Mutex);
    auto iterator = m_DroppedRanges.find(key);
    if (iterator == m_DroppedRanges.end()) {
        if (m_DroppedRanges.size() >= m_MaxActiveStreams) return false;
        m_DroppedRanges.emplace(std::move(key),
            std::vector<DroppedRange>{{sequence, sequence}});
        return true;
    }

    auto& ranges = iterator->second;
    const auto next = std::lower_bound(ranges.begin(), ranges.end(), sequence,
        [](const DroppedRange& range, std::uint64_t value) {
            return range.last < value;
        });
    if (next != ranges.end() && next->first <= sequence) return true;

    auto previous = next;
    const bool hasPrevious = previous != ranges.begin();
    if (hasPrevious) --previous;
    const bool joinsPrevious = hasPrevious &&
        previous->last != std::numeric_limits<std::uint64_t>::max() &&
        previous->last + 1 == sequence;
    const bool joinsNext = next != ranges.end() &&
        sequence != std::numeric_limits<std::uint64_t>::max() &&
        sequence + 1 == next->first;

    if (joinsPrevious && joinsNext) {
        previous->last = next->last;
        ranges.erase(next);
        return true;
    }
    if (joinsPrevious) {
        previous->last = sequence;
        return true;
    }
    if (joinsNext) {
        next->first = sequence;
        return true;
    }
    if (ranges.size() >= MaximumDroppedIntervalsPerStream) return false;
    ranges.insert(next, DroppedRange{sequence, sequence});
    return true;
}

bool StreamSequenceValidator::ForgetStream(
    std::string_view adapter, std::string_view instance,
    std::string_view channel, std::string_view streamId) {
    if (adapter.empty() || instance.empty() || channel.empty() || streamId.empty()) {
        return false;
    }
    Key key{std::string(adapter), std::string(instance), std::string(channel),
        std::string(streamId)};
    std::scoped_lock lock(m_Mutex);
    const bool active = m_NextSequence.erase(key) != 0;
    const bool dropped = m_DroppedRanges.erase(key) != 0;
    return active || dropped;
}

std::size_t StreamSequenceValidator::ActiveStreamCount() const noexcept {
    std::scoped_lock lock(m_Mutex);
    return m_NextSequence.size();
}

bool StreamSequenceValidator::IsPureVideoFrame(
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

std::size_t StreamSequenceValidator::KeyHash::operator()(
    const Key& key) const noexcept {
    std::size_t seed = 0;
    const auto mix = [&](const std::string& value) {
        const auto hash = std::hash<std::string>{}(value);
        seed ^= hash + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
    };
    mix(key.adapter);
    mix(key.instance);
    mix(key.channel);
    mix(key.streamId);
    return seed;
}

} // namespace XSI
