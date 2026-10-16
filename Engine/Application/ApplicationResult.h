#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace Engine {

// Result is intentionally scoped to the application-composition facade.  It
// does not attempt to replace established feature-specific result types
// outside the application-composition facade.
enum class ApplicationErrorCode : std::uint16_t {
    None = 0,
    InvalidOptions = 1,
    Unsupported = 2,
    WrongThread = 3,
    AlreadyExists = 4,
    AlreadyBound = 5,
    Busy = 6,
    LifecycleViolation = 7,
    BackendFailure = 8,
    ShuttingDown = 9,
    UnknownFailure = 10,
    NoData = 11,
    InvalidData = 12,
    TooLarge = 13,
    DecodeFailed = 14,
    EncodeFailed = 15,
    BackendUnavailable = 16,
    FileNotFound = 17,
    PermissionDenied = 18,
    ReadFailed = 19,
    QueueFull = 20,
    Cancelled = 21,
};

struct ApplicationError {
    ApplicationErrorCode code = ApplicationErrorCode::None;
    std::string operation;
    std::string capability;
    std::string message;
    std::optional<std::int64_t> nativeCode;
    std::string backend;
};

inline const char* ToString(ApplicationErrorCode code) noexcept {
    switch (code) {
    case ApplicationErrorCode::None: return "none";
    case ApplicationErrorCode::InvalidOptions: return "invalid_options";
    case ApplicationErrorCode::Unsupported: return "unsupported";
    case ApplicationErrorCode::WrongThread: return "wrong_thread";
    case ApplicationErrorCode::AlreadyExists: return "already_exists";
    case ApplicationErrorCode::AlreadyBound: return "already_bound";
    case ApplicationErrorCode::Busy: return "busy";
    case ApplicationErrorCode::LifecycleViolation: return "lifecycle_violation";
    case ApplicationErrorCode::BackendFailure: return "backend_failure";
    case ApplicationErrorCode::ShuttingDown: return "shutting_down";
    case ApplicationErrorCode::UnknownFailure: return "unknown_failure";
    case ApplicationErrorCode::NoData: return "no_data";
    case ApplicationErrorCode::InvalidData: return "invalid_data";
    case ApplicationErrorCode::TooLarge: return "too_large";
    case ApplicationErrorCode::DecodeFailed: return "decode_failed";
    case ApplicationErrorCode::EncodeFailed: return "encode_failed";
    case ApplicationErrorCode::BackendUnavailable:
        return "backend_unavailable";
    case ApplicationErrorCode::FileNotFound: return "file_not_found";
    case ApplicationErrorCode::PermissionDenied: return "permission_denied";
    case ApplicationErrorCode::ReadFailed: return "read_failed";
    case ApplicationErrorCode::QueueFull: return "queue_full";
    case ApplicationErrorCode::Cancelled: return "cancelled";
    }
    return "unknown_failure";
}

template <typename T>
class ApplicationResult final {
public:
    static ApplicationResult Success(T value) {
        ApplicationResult result;
        result.m_Value.emplace(std::move(value));
        return result;
    }

    static ApplicationResult Failure(ApplicationError error) {
        ApplicationResult result;
        result.m_Error.emplace(std::move(error));
        return result;
    }

    [[nodiscard]] bool HasValue() const noexcept { return m_Value.has_value(); }
    [[nodiscard]] explicit operator bool() const noexcept { return HasValue(); }

    T& Value() {
        if (!m_Value) throw std::logic_error("ApplicationResult has no value");
        return *m_Value;
    }
    const T& Value() const {
        if (!m_Value) throw std::logic_error("ApplicationResult has no value");
        return *m_Value;
    }

    const ApplicationError& Error() const noexcept {
        static const ApplicationError empty{};
        return m_Error ? *m_Error : empty;
    }

private:
    std::optional<T> m_Value;
    std::optional<ApplicationError> m_Error;
};

template <>
class ApplicationResult<void> final {
public:
    static ApplicationResult Success() {
        ApplicationResult result;
        result.m_Success = true;
        return result;
    }

    static ApplicationResult Failure(ApplicationError error) {
        ApplicationResult result;
        result.m_Error.emplace(std::move(error));
        return result;
    }

    [[nodiscard]] bool HasValue() const noexcept { return m_Success; }
    [[nodiscard]] explicit operator bool() const noexcept { return HasValue(); }

    const ApplicationError& Error() const noexcept {
        static const ApplicationError empty{};
        return m_Error ? *m_Error : empty;
    }

private:
    bool m_Success = false;
    std::optional<ApplicationError> m_Error;
};

} // namespace Engine
