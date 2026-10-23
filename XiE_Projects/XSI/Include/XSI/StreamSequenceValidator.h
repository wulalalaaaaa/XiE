#pragma once

#include "XSI/SensoryEvent.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace XSI {

enum class StreamSequenceError : std::uint8_t {
    InvalidEvent,
    TooManyActiveStreams,
    DuplicateStreamStart,
    StreamNotStarted,
    UnexpectedSequence,
    SequenceOverflow,
    DroppedFrameTrackingFull,
};

struct StreamSequenceResult {
    bool accepted = false;
    std::optional<StreamSequenceError> error;
    std::uint64_t expected = 0;
    std::uint64_t skippedSequences = 0;
};

// Stateful guard for an ordered consumer. The channel calls Accept while
// holding its dequeue lock and records LatestOnly frame evictions here.
class StreamSequenceValidator final {
public:
    explicit StreamSequenceValidator(std::size_t maxActiveStreams = 1024);
    StreamSequenceValidator(
        std::size_t maxActiveStreams, ValidationLimits validationLimits);
    [[nodiscard]] StreamSequenceResult Accept(
        const SensoryEvent& event, bool allowDroppedFrameGaps = false);
    [[nodiscard]] bool NoteDroppedVideoFrame(const SensoryEvent& event);
    [[nodiscard]] bool ForgetStream(
        std::string_view adapter, std::string_view instance,
        std::string_view channel, std::string_view streamId);
    [[nodiscard]] std::size_t ActiveStreamCount() const noexcept;

private:
    struct Key {
        std::string adapter;
        std::string instance;
        std::string channel;
        std::string streamId;
        friend bool operator==(const Key&, const Key&) = default;
    };
    struct KeyHash {
        [[nodiscard]] std::size_t operator()(const Key& key) const noexcept;
    };
    struct DroppedRange {
        std::uint64_t first = 0;
        std::uint64_t last = 0;
    };

    static constexpr std::size_t MaximumDroppedIntervalsPerStream = 64;

    [[nodiscard]] static bool IsPureVideoFrame(
        const SensoryEvent& event) noexcept;

    const std::size_t m_MaxActiveStreams;
    const ValidationLimits m_ValidationLimits;
    mutable std::mutex m_Mutex;
    std::unordered_map<Key, std::uint64_t, KeyHash> m_NextSequence;
    std::unordered_map<Key, std::vector<DroppedRange>, KeyHash> m_DroppedRanges;
};

} // namespace XSI
