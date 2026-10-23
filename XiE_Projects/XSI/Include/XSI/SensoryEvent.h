#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace XSI {

struct EventId {
    std::uint64_t high = 0;
    std::uint64_t low = 0;

    [[nodiscard]] bool IsValid() const noexcept { return high != 0 || low != 0; }
    friend constexpr bool operator==(EventId, EventId) = default;
};

// IDs combine a process-session nonce with a monotonic counter. The nonce is
// generated once from std::random_device, with clock/address entropy fallback.
[[nodiscard]] EventId GenerateEventId() noexcept;
[[nodiscard]] std::string ToString(EventId id);

enum class TimeDomain : std::uint8_t {
    UnixEpochNanoseconds,
    SourceMonotonicNanoseconds,
};

struct OccurredTime {
    std::int64_t nanoseconds = 0;
    TimeDomain domain = TimeDomain::UnixEpochNanoseconds;
};

struct ReceivedTime {
    // UTC since Unix epoch; suitable for cross-process display and persistence.
    std::int64_t utcNanoseconds = 0;
    // Steady-clock time in this process; use for latency only, never serialize
    // as UTC or compare with another process's monotonic clock.
    std::int64_t monotonicNanoseconds = 0;
};

[[nodiscard]] OccurredTime CurrentUtcTime() noexcept;

struct SourceIdentity {
    std::string adapter;
    std::string instance;
};

struct ExternalId {
    // The namespace identifies the source platform, for example "qq:group".
    std::string nameSpace;
    std::string value;
};

enum class MediaKind : std::uint8_t {
    Image,
    Audio,
    Video,
    Haptic,
    Extension,
};

struct MediaResourceMetrics {
    std::uint64_t liveResources = 0;
    std::uint64_t liveBytes = 0;
    std::uint64_t peakLiveBytes = 0;
    std::uint64_t createdResources = 0;
};

// Immutable, reference-counted process-local media bytes. Copying this handle
// shares storage; Adopt transfers a vector allocation without copying its bytes.
class MediaResourceRef final {
public:
    MediaResourceRef() = default;

    [[nodiscard]] static MediaResourceRef Copy(std::span<const std::byte> bytes);
    [[nodiscard]] static MediaResourceRef Adopt(std::vector<std::byte>&& bytes);

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] std::span<const std::byte> Bytes() const noexcept;
    [[nodiscard]] const std::byte* Data() const noexcept;
    [[nodiscard]] std::size_t ByteSize() const noexcept;
    [[nodiscard]] long UseCount() const noexcept;

private:
    struct Storage;
    explicit MediaResourceRef(std::shared_ptr<const Storage> storage) noexcept;
    std::shared_ptr<const Storage> m_Storage;
};

[[nodiscard]] MediaResourceMetrics CaptureMediaResourceMetrics() noexcept;

struct TextPart {
    std::string utf8;
};

struct MediaPart {
    MediaKind kind = MediaKind::Extension;
    MediaResourceRef resource;
    // MIME-like or application-defined format ID, such as "image/jpeg".
    std::string formatId;
    // Optional stream-relative position and duration in nanoseconds.
    std::optional<std::uint64_t> positionNanoseconds;
    std::optional<std::uint64_t> durationNanoseconds;
};

using ContentPart = std::variant<TextPart, MediaPart>;

enum class EventKind : std::uint8_t {
    Discrete,
    StreamStarted,
    StreamData,
    StreamEnded,
};

struct SensoryEvent {
    EventId id = GenerateEventId();
    SourceIdentity source;
    std::string channel;
    std::optional<std::string> actor;
    std::optional<ExternalId> externalId;
    EventKind kind = EventKind::Discrete;
    OccurredTime occurredTime{};
    ReceivedTime receivedTime{};
    std::optional<std::string> streamId;
    std::optional<std::uint64_t> sequence;
    std::vector<ContentPart> parts;
};

struct ValidationLimits {
    std::size_t maxStringBytes = 512;
    std::size_t maxTextBytes = 1 * 1024 * 1024;
    std::size_t maxFormatIdBytes = 256;
    std::size_t maxParts = 64;
    std::uint64_t maxMediaBytes = 512ull * 1024ull * 1024ull;
};

enum class ValidationErrorCode : std::uint8_t {
    InvalidEventId,
    MissingSource,
    MissingChannel,
    InvalidString,
    InvalidUtf8,
    TooManyParts,
    InvalidEventKind,
    InvalidStreamFields,
    InvalidSequence,
    InvalidMediaKind,
    InvalidMediaResource,
    MediaTooLarge,
    MissingFormatId,
    InvalidTimeDomain,
};

struct ValidationError {
    ValidationErrorCode code = ValidationErrorCode::InvalidString;
    std::string message;
};

[[nodiscard]] std::optional<ValidationError> ValidateEvent(
    const SensoryEvent& event,
    const ValidationLimits& limits = {});

} // namespace XSI
