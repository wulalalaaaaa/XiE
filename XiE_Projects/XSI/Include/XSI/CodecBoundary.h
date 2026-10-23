#pragma once

#include "XSI/SensoryEvent.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace XSI {

inline constexpr std::uint16_t SensoryWireSchemaV1 = 1;

struct PersistentMediaResourceId {
    std::string nameSpace;
    std::string value;
};

struct CodecError {
    std::uint16_t schemaVersion = SensoryWireSchemaV1;
    std::string message;
};

template <typename T>
struct CodecResult {
    std::optional<T> value;
    std::optional<CodecError> error;

    [[nodiscard]] static CodecResult Success(T result) {
        return {std::move(result), std::nullopt};
    }
    [[nodiscard]] static CodecResult Failure(CodecError failure) {
        return {std::nullopt, std::move(failure)};
    }
    [[nodiscard]] explicit operator bool() const noexcept {
        return value.has_value() && !error.has_value();
    }
};

// Maps local handles to durable resource IDs. It does not transfer media bytes.
class IPersistentMediaResourceResolver {
public:
    virtual ~IPersistentMediaResourceResolver() = default;
    [[nodiscard]] virtual std::optional<PersistentMediaResourceId> Locate(
        const MediaResourceRef& resource) const = 0;
    [[nodiscard]] virtual std::optional<MediaResourceRef> Resolve(
        const PersistentMediaResourceId& id) const = 0;
};

// Boundary only: a concrete V1 codec is intentionally not shipped yet.
// Implementations encode the versioned event envelope and resource IDs;
// media byte transport/storage remains a separate concern.
class ISensoryEventCodecV1 {
public:
    virtual ~ISensoryEventCodecV1() = default;
    [[nodiscard]] virtual CodecResult<std::vector<std::byte>> Encode(
        const SensoryEvent& event,
        const IPersistentMediaResourceResolver& resources) const = 0;
    [[nodiscard]] virtual CodecResult<SensoryEvent> Decode(
        std::span<const std::byte> bytes,
        const IPersistentMediaResourceResolver& resources) const = 0;
};

} // namespace XSI
