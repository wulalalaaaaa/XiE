#include "XSI/SensoryEvent.h"

#include <array>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <utility>

namespace XSI {
namespace {

struct ProcessNonce {
    std::uint64_t high = 0;
    std::uint64_t low = 0;
};

ProcessNonce MakeProcessNonce() noexcept {
    ProcessNonce nonce{};
    try {
        std::random_device random;
        nonce.high =
            (static_cast<std::uint64_t>(random()) << 32) |
            static_cast<std::uint64_t>(random());
        nonce.low =
            (static_cast<std::uint64_t>(random()) << 32) |
            static_cast<std::uint64_t>(random());
    } catch (...) {
        const auto wall = std::chrono::system_clock::now().time_since_epoch().count();
        const auto monotonic = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto address = reinterpret_cast<std::uintptr_t>(&nonce);
        nonce.high = static_cast<std::uint64_t>(wall) ^
            (static_cast<std::uint64_t>(address) << 17);
        nonce.low = static_cast<std::uint64_t>(monotonic) ^
            (static_cast<std::uint64_t>(address) >> 3);
    }
    if (nonce.high == 0 && nonce.low == 0) {
        nonce.low = 0x9e3779b97f4a7c15ull;
    }
    return nonce;
}

std::int64_t CurrentNanoseconds(
    std::chrono::system_clock::time_point now) noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
}

std::int64_t CurrentNanoseconds(
    std::chrono::steady_clock::time_point now) noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
}

struct ResourceCounters {
    std::atomic<std::uint64_t> liveResources{0};
    std::atomic<std::uint64_t> liveBytes{0};
    std::atomic<std::uint64_t> peakLiveBytes{0};
    std::atomic<std::uint64_t> createdResources{0};
};

ResourceCounters& Counters() noexcept {
    static ResourceCounters counters;
    return counters;
}

void RecordResourceCreation(std::size_t bytes) noexcept {
    auto& counters = Counters();
    counters.liveResources.fetch_add(1, std::memory_order_relaxed);
    counters.createdResources.fetch_add(1, std::memory_order_relaxed);
    const auto live = counters.liveBytes.fetch_add(
        static_cast<std::uint64_t>(bytes), std::memory_order_relaxed) +
        static_cast<std::uint64_t>(bytes);
    auto peak = counters.peakLiveBytes.load(std::memory_order_relaxed);
    while (peak < live && !counters.peakLiveBytes.compare_exchange_weak(
        peak, live, std::memory_order_relaxed)) {
    }
}

void RecordResourceDestruction(std::size_t bytes) noexcept {
    auto& counters = Counters();
    counters.liveResources.fetch_sub(1, std::memory_order_relaxed);
    counters.liveBytes.fetch_sub(
        static_cast<std::uint64_t>(bytes), std::memory_order_relaxed);
}

std::optional<ValidationError> Error(
    ValidationErrorCode code, std::string message) {
    return ValidationError{code, std::move(message)};
}

bool IsValidUtf8(const std::string& text) noexcept {
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char lead = bytes[i];
        if (lead <= 0x7f) {
            ++i;
            continue;
        }
        std::size_t count = 0;
        unsigned char secondMin = 0x80;
        unsigned char secondMax = 0xbf;
        if (lead >= 0xc2 && lead <= 0xdf) {
            count = 2;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            count = 3;
            if (lead == 0xe0) secondMin = 0xa0;
            if (lead == 0xed) secondMax = 0x9f;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            count = 4;
            if (lead == 0xf0) secondMin = 0x90;
            if (lead == 0xf4) secondMax = 0x8f;
        } else {
            return false;
        }
        if (i + count > text.size()) return false;
        const unsigned char second = bytes[i + 1];
        if (second < secondMin || second > secondMax) return false;
        for (std::size_t continuation = 2; continuation < count; ++continuation) {
            if (bytes[i + continuation] < 0x80 ||
                bytes[i + continuation] > 0xbf) {
                return false;
            }
        }
        i += count;
    }
    return true;
}

bool ValidTimeDomain(TimeDomain domain) noexcept {
    switch (domain) {
    case TimeDomain::UnixEpochNanoseconds:
    case TimeDomain::SourceMonotonicNanoseconds:
        return true;
    }
    return false;
}

} // namespace

struct MediaResourceRef::Storage {
    explicit Storage(std::vector<std::byte>&& payload)
        : bytes(std::move(payload)) {
        RecordResourceCreation(bytes.size());
    }
    ~Storage() { RecordResourceDestruction(bytes.size()); }

    std::vector<std::byte> bytes;
};

EventId GenerateEventId() noexcept {
    static const ProcessNonce nonce = MakeProcessNonce();
    static std::atomic<std::uint64_t> sequence{1};
    const auto value = sequence.fetch_add(1, std::memory_order_relaxed);
    return {nonce.high, nonce.low + value};
}

std::string ToString(EventId id) {
    constexpr char digits[] = "0123456789abcdef";
    std::array<char, 32> buffer{};
    const auto write = [&](std::uint64_t value, std::size_t offset) {
        for (int i = 15; i >= 0; --i) {
            buffer[offset + static_cast<std::size_t>(i)] =
                digits[value & 0x0f];
            value >>= 4;
        }
    };
    write(id.high, 0);
    write(id.low, 16);
    return std::string(buffer.data(), buffer.size());
}

OccurredTime CurrentUtcTime() noexcept {
    const auto now = std::chrono::system_clock::now();
    return {
        static_cast<std::int64_t>(CurrentNanoseconds(now)),
        TimeDomain::UnixEpochNanoseconds};
}

MediaResourceRef::MediaResourceRef(std::shared_ptr<const Storage> storage) noexcept
    : m_Storage(std::move(storage)) {}

MediaResourceRef MediaResourceRef::Copy(std::span<const std::byte> bytes) {
    return Adopt(std::vector<std::byte>(bytes.begin(), bytes.end()));
}

MediaResourceRef MediaResourceRef::Adopt(std::vector<std::byte>&& bytes) {
    return MediaResourceRef(std::make_shared<const Storage>(std::move(bytes)));
}

bool MediaResourceRef::IsValid() const noexcept {
    return m_Storage != nullptr && !m_Storage->bytes.empty();
}

std::span<const std::byte> MediaResourceRef::Bytes() const noexcept {
    if (!m_Storage) return {};
    return m_Storage->bytes;
}

const std::byte* MediaResourceRef::Data() const noexcept {
    return m_Storage && !m_Storage->bytes.empty()
        ? m_Storage->bytes.data()
        : nullptr;
}

std::size_t MediaResourceRef::ByteSize() const noexcept {
    return m_Storage ? m_Storage->bytes.size() : 0;
}

long MediaResourceRef::UseCount() const noexcept {
    return m_Storage.use_count();
}

MediaResourceMetrics CaptureMediaResourceMetrics() noexcept {
    auto& counters = Counters();
    return {
        counters.liveResources.load(std::memory_order_relaxed),
        counters.liveBytes.load(std::memory_order_relaxed),
        counters.peakLiveBytes.load(std::memory_order_relaxed),
        counters.createdResources.load(std::memory_order_relaxed)};
}

std::optional<ValidationError> ValidateEvent(
    const SensoryEvent& event, const ValidationLimits& limits) {
    if (!event.id.IsValid()) {
        return Error(ValidationErrorCode::InvalidEventId,
            "event ID must not be all zero");
    }
    if (event.source.adapter.empty() || event.source.instance.empty()) {
        return Error(ValidationErrorCode::MissingSource,
            "source adapter and source instance are required");
    }
    if (event.source.adapter.size() > limits.maxStringBytes ||
        event.source.instance.size() > limits.maxStringBytes ||
        event.channel.size() > limits.maxStringBytes) {
        return Error(ValidationErrorCode::InvalidString,
            "source and channel fields exceed the configured byte limit");
    }
    if (event.channel.empty()) {
        return Error(ValidationErrorCode::MissingChannel,
            "channel is required");
    }
    if (!IsValidUtf8(event.source.adapter) ||
        !IsValidUtf8(event.source.instance) ||
        !IsValidUtf8(event.channel)) {
        return Error(ValidationErrorCode::InvalidUtf8,
            "source and channel fields must be UTF-8");
    }
    if (event.actor && (event.actor->empty() ||
        event.actor->size() > limits.maxStringBytes ||
        !IsValidUtf8(*event.actor))) {
        return Error(ValidationErrorCode::InvalidString,
            "actor must be non-empty UTF-8 within the configured limit");
    }
    if (event.externalId &&
        (event.externalId->nameSpace.empty() || event.externalId->value.empty() ||
         event.externalId->nameSpace.size() > limits.maxStringBytes ||
         event.externalId->value.size() > limits.maxStringBytes ||
         !IsValidUtf8(event.externalId->nameSpace) ||
         !IsValidUtf8(event.externalId->value))) {
        return Error(ValidationErrorCode::InvalidString,
            "external ID requires a valid namespace and value");
    }
    if (!ValidTimeDomain(event.occurredTime.domain)) {
        return Error(ValidationErrorCode::InvalidTimeDomain,
            "occurred time has an unknown clock domain");
    }
    if (event.parts.size() > limits.maxParts) {
        return Error(ValidationErrorCode::TooManyParts,
            "event has more parts than the configured limit");
    }

    switch (event.kind) {
    case EventKind::Discrete:
        if (event.streamId || event.sequence) {
            return Error(ValidationErrorCode::InvalidStreamFields,
                "discrete events cannot carry stream ID or sequence");
        }
        break;
    case EventKind::StreamStarted:
        if (!event.streamId || event.streamId->empty() ||
            event.streamId->size() > limits.maxStringBytes ||
            !event.sequence || *event.sequence != 0 || !event.parts.empty()) {
            return Error(ValidationErrorCode::InvalidStreamFields,
                "stream start requires an ID, sequence zero, and no parts");
        }
        break;
    case EventKind::StreamData:
        if (!event.streamId || event.streamId->empty() ||
            event.streamId->size() > limits.maxStringBytes ||
            !event.sequence || *event.sequence == 0 || event.parts.empty()) {
            return Error(ValidationErrorCode::InvalidStreamFields,
                "stream data requires an ID, positive sequence, and parts");
        }
        break;
    case EventKind::StreamEnded:
        if (!event.streamId || event.streamId->empty() ||
            event.streamId->size() > limits.maxStringBytes ||
            !event.sequence || *event.sequence == 0 || !event.parts.empty()) {
            return Error(ValidationErrorCode::InvalidStreamFields,
                "stream end requires an ID, positive next sequence, and no parts");
        }
        break;
    default:
        return Error(ValidationErrorCode::InvalidEventKind,
            "event kind is not supported");
    }
    if (event.streamId && !IsValidUtf8(*event.streamId)) {
        return Error(ValidationErrorCode::InvalidUtf8,
            "stream ID must be UTF-8");
    }

    for (const auto& part : event.parts) {
        if (const auto* text = std::get_if<TextPart>(&part)) {
            if (text->utf8.size() > limits.maxTextBytes) {
                return Error(ValidationErrorCode::InvalidString,
                    "text part exceeds the configured byte limit");
            }
            if (!IsValidUtf8(text->utf8)) {
                return Error(ValidationErrorCode::InvalidUtf8,
                    "text part is not valid UTF-8");
            }
            continue;
        }
        const auto* media = std::get_if<MediaPart>(&part);
        if (media == nullptr) {
            return Error(ValidationErrorCode::InvalidMediaKind,
                "content part variant is not supported");
        }
        switch (media->kind) {
        case MediaKind::Image:
        case MediaKind::Audio:
        case MediaKind::Video:
        case MediaKind::Haptic:
        case MediaKind::Extension:
            break;
        default:
            return Error(ValidationErrorCode::InvalidMediaKind,
                "media kind is not supported");
        }
        if (!media->resource.IsValid()) {
            return Error(ValidationErrorCode::InvalidMediaResource,
                "media part must reference a live resource");
        }
        if (media->resource.ByteSize() > limits.maxMediaBytes) {
            return Error(ValidationErrorCode::MediaTooLarge,
                "media resource exceeds the configured byte limit");
        }
        if (media->formatId.empty() ||
            media->formatId.size() > limits.maxFormatIdBytes ||
            !IsValidUtf8(media->formatId)) {
            return Error(ValidationErrorCode::MissingFormatId,
                "media part requires a valid format ID");
        }
        if (event.kind == EventKind::StreamData &&
            (media->kind == MediaKind::Audio || media->kind == MediaKind::Video) &&
            !media->positionNanoseconds) {
            return Error(ValidationErrorCode::InvalidStreamFields,
                "audio and video stream data require a stream-relative position");
        }
    }
    return std::nullopt;
}

} // namespace XSI
