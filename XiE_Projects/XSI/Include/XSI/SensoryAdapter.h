#pragma once

#include "XSI/SensoryEventChannel.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace XSI {

enum class AdapterStatus : std::uint8_t {
    Started,
    AlreadyRunning,
    PublishRejected,
    InvalidConfiguration,
    InitializationFailed,
};

enum class AdapterError : std::uint8_t {
    None,
    InvalidFrameSize,
    InvalidFrameInterval,
    ThreadStartFailed,
    WorkerException,
    FramePublishFailed,
    StreamEndPublishFailed,
};

enum class AdapterRunState : std::uint8_t {
    Idle,
    Running,
    Stopped,
    Failed,
};

struct AdapterRuntimeStatus {
    AdapterRunState state = AdapterRunState::Idle;
    AdapterError error = AdapterError::None;
    std::optional<PublishStatus> lastPublishStatus;
};

enum class AdapterStopStatus : std::uint8_t {
    NotRunning,
    Stopped,
    AbnormalTermination,
};

struct AdapterStopResult {
    AdapterStopStatus status = AdapterStopStatus::NotRunning;
    bool streamEndPublished = false;
    bool streamAbnormallyTerminated = false;
    bool streamStateCleared = false;
    PublishStatus streamEndStatus = PublishStatus::Accepted;
    AdapterError error = AdapterError::None;
};

struct AdapterStartResult {
    AdapterStatus status = AdapterStatus::InitializationFailed;
    PublishResult publishResult{};
    AdapterError error = AdapterError::None;

    [[nodiscard]] bool Started() const noexcept {
        return status == AdapterStatus::Started;
    }
};

class ISensoryAdapter {
public:
    virtual ~ISensoryAdapter() = default;
    [[nodiscard]] virtual AdapterStartResult Start(
        ISensoryEventPublisher& publisher) = 0;
    virtual void Stop() noexcept = 0;
    [[nodiscard]] virtual AdapterStopResult StopAndReport(
        std::stop_token cancellationToken = {}) noexcept {
        (void)cancellationToken;
        Stop();
        return {AdapterStopStatus::Stopped};
    }
};

class SimulatedTextImageAdapter final : public ISensoryAdapter {
public:
    [[nodiscard]] AdapterStartResult Start(
        ISensoryEventPublisher& publisher) override;
    void Stop() noexcept override;
    [[nodiscard]] AdapterStopResult StopAndReport(
        std::stop_token cancellationToken = {}) noexcept override;

private:
    bool m_Started = false;
};

class SimulatedVideoAdapter final : public ISensoryAdapter {
public:
    explicit SimulatedVideoAdapter(
        std::chrono::milliseconds frameInterval = std::chrono::milliseconds(33),
        std::size_t frameBytes = 64 * 1024);
    ~SimulatedVideoAdapter() override;

    [[nodiscard]] AdapterStartResult Start(
        ISensoryEventPublisher& publisher) override;
    void Stop() noexcept override;
    [[nodiscard]] AdapterStopResult StopAndReport(
        std::stop_token cancellationToken = {}) noexcept override;
    [[nodiscard]] AdapterRuntimeStatus RuntimeStatus() const noexcept;

private:
    void Run(std::stop_token stopToken,
        ISensoryEventPublisher* publisher) noexcept;
    [[nodiscard]] AdapterStopResult FinalizeStream(
        std::stop_token cancellationToken) noexcept;
    void RecordFailure(AdapterError error,
        std::optional<PublishStatus> publishStatus = std::nullopt) noexcept;

    const std::chrono::milliseconds m_FrameInterval;
    const std::size_t m_FrameBytes;
    const AdapterError m_ConfigurationError;
    std::string m_StreamId;
    std::uint64_t m_NextSequence = 1;
    std::jthread m_Worker;
    ISensoryEventPublisher* m_Publisher = nullptr;
    mutable std::mutex m_LifecycleMutex;
    mutable std::mutex m_StatusMutex;
    AdapterRuntimeStatus m_RuntimeStatus{};
    AdapterStopResult m_LastStopResult{};
};

} // namespace XSI
