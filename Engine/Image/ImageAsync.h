#pragma once

#include "Image/ImageFile.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace Engine {

using ImageTaskToken = std::uint64_t;

struct ImageAsyncTaskTiming {
    ImageTaskToken token = 0;
    // Monotonic timestamps are optional diagnostics only.  They are populated
    // by the service for a task that reached a worker and never borrow a
    // platform or product resource.
    std::chrono::steady_clock::time_point submittedAt{};
    std::chrono::steady_clock::time_point workerStartedAt{};
    std::chrono::steady_clock::time_point fileStartedAt{};
    std::chrono::steady_clock::time_point fileFinishedAt{};
    std::uint64_t queueWaitUs = 0;
    std::uint64_t fileServiceUs = 0;
    std::uint64_t resultEnqueueUs = 0;
    std::chrono::steady_clock::time_point resultEnqueuedAt{};
    bool resultEnqueued = false;
    bool cancelled = false;
    bool success = false;
};

using ImageAsyncTimingCallback =
    std::function<void(const ImageAsyncTaskTiming&)>;

enum class ImageTaskPriority : std::uint8_t {
    Immediate,
    Visible,
    Background,
};

enum class ImageTaskState : std::uint8_t {
    Queued,
    Running,
    Completed,
    Cancelled,
    Failed,
    ShuttingDown,
};

struct ImageAsyncOptions {
    std::size_t workerCount = 0;
    std::size_t maxQueuedTasks = 32;
    std::size_t maxPendingResults = 64;
    ImageAsyncTimingCallback timingCallback;
};

struct ImageAsyncRequest {
    std::filesystem::path sourcePath;
    ImageDecodeOptions decode{};
    std::optional<ImageResizeOptions> resize;
    ImageTaskPriority priority = ImageTaskPriority::Visible;
};

struct ImageAsyncResult {
    ImageTaskToken token = 0;
    ImageTaskState state = ImageTaskState::Failed;
    std::optional<ImageDecodedFile> value;
    std::optional<ApplicationError> error;
};

struct ImageAsyncSnapshot {
    std::size_t workerCount = 0;
    std::size_t queuedTasks = 0;
    std::size_t runningTasks = 0;
    std::size_t pendingResults = 0;
    std::size_t maxQueuedTasks = 0;
    std::size_t maxPendingResults = 0;
    std::uint64_t completedTasks = 0;
    std::uint64_t cancelledTasks = 0;
    std::uint64_t failedTasks = 0;
    std::uint64_t droppedResults = 0;
    bool shuttingDown = false;
};

class ImageAsyncService final {
public:
    ImageAsyncService(
        std::shared_ptr<IImageFileService> files,
        ImageAsyncOptions options = {});
    ~ImageAsyncService() noexcept;

    ImageAsyncService(const ImageAsyncService&) = delete;
    ImageAsyncService& operator=(const ImageAsyncService&) = delete;

    ApplicationResult<ImageTaskToken> Submit(ImageAsyncRequest request);
    ApplicationResult<void> Cancel(ImageTaskToken token);
    ApplicationResult<std::size_t> DrainResults(
        std::size_t budget,
        const std::function<void(ImageAsyncResult)>& consumer);
    ApplicationResult<std::size_t> DrainResults(
        std::size_t budget,
        const std::function<bool(const ImageAsyncResult&)>& selector,
        const std::function<void(ImageAsyncResult)>& consumer);
    ApplicationResult<void> SetWakeCallback(std::function<void()> callback);
    // Optional diagnostics callback. It is copied under the service lock and
    // invoked on the worker that completes a task; it never changes task
    // scheduling or result ownership.
    ApplicationResult<void> SetTimingCallback(
        ImageAsyncTimingCallback callback);
    ApplicationResult<void> Shutdown();

    [[nodiscard]] ImageAsyncSnapshot Snapshot() const;
    [[nodiscard]] const char* BackendName() const noexcept {
        return "xie_image_async";
    }
    [[nodiscard]] bool IsShuttingDown() const noexcept;

private:
    struct Job {
        ImageTaskToken token = 0;
        ImageAsyncRequest request;
        bool running = false;
        bool cancelRequested = false;
        std::chrono::steady_clock::time_point submittedAt;
    };

    static std::size_t DefaultWorkerCount();
    void StartWorkersLocked();
    std::shared_ptr<Job> TakeJob();
    void WorkerMain();
    void Complete(
        const std::shared_ptr<Job>& job,
        ImageAsyncResult result,
        ImageAsyncTaskTiming timing);
    void EnqueueResultLocked(ImageAsyncResult result);

    std::shared_ptr<IImageFileService> m_Files;
    ImageAsyncOptions m_Options;
    std::thread::id m_OwnerThread = std::this_thread::get_id();
    mutable std::mutex m_Mutex;
    std::condition_variable m_Wake;
    std::map<ImageTaskToken, std::shared_ptr<Job>> m_Jobs;
    std::deque<ImageTaskToken> m_Immediate;
    std::deque<ImageTaskToken> m_Visible;
    std::deque<ImageTaskToken> m_Background;
    std::deque<ImageAsyncResult> m_Results;
    std::vector<std::thread> m_Workers;
    std::function<void()> m_WakeCallback;
    ImageAsyncTimingCallback m_TimingCallback;
    ImageTaskToken m_NextToken = 1;
    std::uint64_t m_Completed = 0;
    std::uint64_t m_Cancelled = 0;
    std::uint64_t m_Failed = 0;
    std::uint64_t m_DroppedResults = 0;
    bool m_Stopping = false;
};

} // namespace Engine
