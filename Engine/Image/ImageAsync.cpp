#include "Image/ImageAsync.h"

#include <algorithm>
#include <utility>

namespace Engine {
namespace {

ApplicationError Error(ApplicationErrorCode code, std::string operation,
    std::string message) {
    return {code, std::move(operation), "image_async", std::move(message),
        std::nullopt, "xie_image_async"};
}

template <typename T>
ApplicationResult<T> Failure(ApplicationErrorCode code,
    std::string operation, std::string message) {
    return ApplicationResult<T>::Failure(
        Error(code, std::move(operation), std::move(message)));
}

ImageAsyncOptions Normalize(ImageAsyncOptions options) {
    const auto hardware = std::thread::hardware_concurrency();
    const auto defaultWorkers = std::min<std::size_t>(
        4, std::max<std::size_t>(1, hardware > 1 ? hardware - 1 : 1));
    options.workerCount = std::clamp<std::size_t>(
        options.workerCount == 0 ? defaultWorkers : options.workerCount, 1, 4);
    options.maxQueuedTasks = std::clamp<std::size_t>(
        options.maxQueuedTasks, 1, 4096);
    options.maxPendingResults = std::clamp<std::size_t>(
        options.maxPendingResults, 1, 4096);
    return options;
}

void PushToken(ImageTaskPriority priority, ImageTaskToken token,
    std::deque<ImageTaskToken>& immediate,
    std::deque<ImageTaskToken>& visible,
    std::deque<ImageTaskToken>& background) {
    switch (priority) {
    case ImageTaskPriority::Immediate: immediate.push_back(token); break;
    case ImageTaskPriority::Visible: visible.push_back(token); break;
    case ImageTaskPriority::Background: background.push_back(token); break;
    }
}

} // namespace

ImageAsyncService::ImageAsyncService(
    std::shared_ptr<IImageFileService> files, ImageAsyncOptions options)
    : m_Files(std::move(files)), m_Options(Normalize(options)),
      m_TimingCallback(m_Options.timingCallback) {
}

ImageAsyncService::~ImageAsyncService() noexcept {
    (void)Shutdown();
}

ApplicationResult<ImageTaskToken> ImageAsyncService::Submit(
    ImageAsyncRequest request) {
    if (request.sourcePath.empty()) return Failure<ImageTaskToken>(
        ApplicationErrorCode::InvalidOptions, "SubmitImageTask",
        "image task source path is empty");
    if (!m_Files) return Failure<ImageTaskToken>(
        ApplicationErrorCode::BackendUnavailable, "SubmitImageTask",
        "image file service is unavailable");
    std::lock_guard lock(m_Mutex);
    if (m_Stopping) return Failure<ImageTaskToken>(
        ApplicationErrorCode::ShuttingDown, "SubmitImageTask",
        "image task service is shutting down");
    if (m_Jobs.size() >= m_Options.maxQueuedTasks) {
        return Failure<ImageTaskToken>(ApplicationErrorCode::QueueFull,
            "SubmitImageTask", "image task queue is full");
    }
    if (m_NextToken == 0) ++m_NextToken;
    const auto token = m_NextToken++;
    auto job = std::make_shared<Job>();
    job->token = token;
    job->request = std::move(request);
    job->submittedAt = std::chrono::steady_clock::now();
    m_Jobs.emplace(token, job);
    PushToken(job->request.priority, token, m_Immediate, m_Visible,
        m_Background);
    StartWorkersLocked();
    m_Wake.notify_one();
    return ApplicationResult<ImageTaskToken>::Success(token);
}

void ImageAsyncService::StartWorkersLocked() {
    if (!m_Workers.empty()) return;
    m_Workers.reserve(m_Options.workerCount);
    for (std::size_t index = 0; index < m_Options.workerCount; ++index)
        m_Workers.emplace_back([this] { WorkerMain(); });
}

ApplicationResult<void> ImageAsyncService::Cancel(ImageTaskToken token) {
    if (token == 0) return ApplicationResult<void>::Success();
    std::function<void()> wake;
    {
        std::lock_guard lock(m_Mutex);
        const auto found = m_Jobs.find(token);
        if (found != m_Jobs.end()) {
            const auto& job = found->second;
            if (job->running) {
                job->cancelRequested = true;
            } else {
                ImageAsyncResult result;
                result.token = token;
                result.state = ImageTaskState::Cancelled;
                result.error = Error(ApplicationErrorCode::Cancelled,
                    "CancelImageTask", "queued image task was cancelled");
                m_Jobs.erase(found);
                ++m_Cancelled;
                const auto before = m_Results.size();
                EnqueueResultLocked(std::move(result));
                wake = before == m_Results.size() ? std::function<void()>{}
                                                   : m_WakeCallback;
            }
        } else {
            // Cancellation is deliberately idempotent.  If a completed result
            // is still queued, remove it so it cannot reach a stale consumer.
            for (auto it = m_Results.begin(); it != m_Results.end(); ++it) {
                if (it->token == token) {
                    m_Results.erase(it);
                    ++m_Cancelled;
                    break;
                }
            }
        }
    }
    if (wake) wake();
    return ApplicationResult<void>::Success();
}

std::shared_ptr<ImageAsyncService::Job> ImageAsyncService::TakeJob() {
    std::unique_lock lock(m_Mutex);
    m_Wake.wait(lock, [this] {
        return m_Stopping || !m_Immediate.empty() || !m_Visible.empty() ||
            !m_Background.empty();
    });
    while (!m_Stopping) {
        ImageTaskToken token = 0;
        if (!m_Immediate.empty()) {
            token = m_Immediate.front();
            m_Immediate.pop_front();
        } else if (!m_Visible.empty()) {
            token = m_Visible.front();
            m_Visible.pop_front();
        } else if (!m_Background.empty()) {
            token = m_Background.front();
            m_Background.pop_front();
        } else {
            m_Wake.wait(lock);
            continue;
        }
        const auto found = m_Jobs.find(token);
        if (found == m_Jobs.end() || found->second->running ||
            found->second->cancelRequested) continue;
        found->second->running = true;
        return found->second;
    }
    return {};
}

void ImageAsyncService::WorkerMain() {
    while (const auto job = TakeJob()) {
        ImageAsyncTaskTiming timing;
        timing.token = job->token;
        timing.submittedAt = job->submittedAt;
        const auto workerStart = std::chrono::steady_clock::now();
        timing.workerStartedAt = workerStart;
        timing.queueWaitUs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                workerStart - job->submittedAt).count());
        ImageAsyncResult result;
        result.token = job->token;
        ApplicationResult<ImageDecodedFile> loaded =
            ApplicationResult<ImageDecodedFile>::Failure(Error(
                ApplicationErrorCode::BackendUnavailable, "DecodeImageTask",
                "image file service is unavailable"));
        const auto fileStart = std::chrono::steady_clock::now();
        timing.fileStartedAt = fileStart;
        if (m_Files) {
            loaded = job->request.resize
                ? m_Files->DecodeThumbnail(job->request.sourcePath,
                    *job->request.resize, job->request.decode)
                : m_Files->Decode(job->request.sourcePath,
                    job->request.decode);
        }
        const auto fileFinished = std::chrono::steady_clock::now();
        timing.fileFinishedAt = fileFinished;
        timing.fileServiceUs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                fileFinished - fileStart).count());
        timing.success = static_cast<bool>(loaded);
        if (loaded) {
            result.state = ImageTaskState::Completed;
            result.value.emplace(std::move(loaded.Value()));
        } else {
            result.state = ImageTaskState::Failed;
            result.error = loaded.Error();
        }
        Complete(job, std::move(result), std::move(timing));
    }
}

void ImageAsyncService::Complete(
    const std::shared_ptr<Job>& job,
    ImageAsyncResult result,
    ImageAsyncTaskTiming timing) {
    std::function<void()> wake;
    ImageAsyncTimingCallback timingCallback;
    {
        std::lock_guard lock(m_Mutex);
        const auto found = m_Jobs.find(job->token);
        if (found == m_Jobs.end() || found->second != job) return;
        const bool cancelled = job->cancelRequested;
        timing.cancelled = cancelled;
        if (cancelled) timing.success = false;
        if (m_Stopping) {
            m_Jobs.erase(found);
        } else if (cancelled) {
            result.state = ImageTaskState::Cancelled;
            result.value.reset();
            result.error = Error(ApplicationErrorCode::Cancelled,
                "DecodeImageTask", "running image task was cancelled");
            ++m_Cancelled;
        } else if (result.state == ImageTaskState::Completed) {
            ++m_Completed;
            timing.success = true;
        } else {
            ++m_Failed;
            timing.success = false;
        }
        if (!m_Stopping) {
            m_Jobs.erase(found);
            const auto before = m_Results.size();
            const auto enqueueStart = std::chrono::steady_clock::now();
            EnqueueResultLocked(std::move(result));
            const auto enqueueEnd = std::chrono::steady_clock::now();
            timing.resultEnqueueUs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    enqueueEnd - enqueueStart).count());
            if (before != m_Results.size()) {
                timing.resultEnqueued = true;
                timing.resultEnqueuedAt = enqueueEnd;
                wake = m_WakeCallback;
            }
        }
        timingCallback = m_TimingCallback;
    }
    if (timingCallback) timingCallback(timing);
    if (wake) wake();
}

void ImageAsyncService::EnqueueResultLocked(ImageAsyncResult result) {
    if (m_Results.size() >= m_Options.maxPendingResults) {
        ++m_DroppedResults;
        return;
    }
    m_Results.push_back(std::move(result));
}

ApplicationResult<std::size_t> ImageAsyncService::DrainResults(
    std::size_t budget,
    const std::function<void(ImageAsyncResult)>& consumer) {
    return DrainResults(budget, {}, consumer);
}

ApplicationResult<std::size_t> ImageAsyncService::DrainResults(
    std::size_t budget,
    const std::function<bool(const ImageAsyncResult&)>& selector,
    const std::function<void(ImageAsyncResult)>& consumer) {
    if (std::this_thread::get_id() != m_OwnerThread) {
        return Failure<std::size_t>(ApplicationErrorCode::WrongThread,
            "DrainImageTaskResults", "results must be consumed on the owner thread");
    }
    std::size_t drained = 0;
    while (drained < budget) {
        ImageAsyncResult result;
        {
            std::lock_guard lock(m_Mutex);
            auto found = m_Results.end();
            for (auto it = m_Results.begin(); it != m_Results.end(); ++it) {
                if (!selector || selector(*it)) {
                    found = it;
                    break;
                }
            }
            if (found == m_Results.end()) break;
            result = std::move(*found);
            m_Results.erase(found);
        }
        if (consumer) consumer(std::move(result));
        ++drained;
    }
    return ApplicationResult<std::size_t>::Success(drained);
}

ApplicationResult<void> ImageAsyncService::SetWakeCallback(
    std::function<void()> callback) {
    std::lock_guard lock(m_Mutex);
    if (m_Stopping) return Failure<void>(ApplicationErrorCode::ShuttingDown,
        "SetImageTaskWakeCallback", "image task service is shutting down");
    m_WakeCallback = std::move(callback);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ImageAsyncService::SetTimingCallback(
    ImageAsyncTimingCallback callback) {
    std::lock_guard lock(m_Mutex);
    if (m_Stopping) return Failure<void>(ApplicationErrorCode::ShuttingDown,
        "SetImageTaskTimingCallback", "image task service is shutting down");
    m_TimingCallback = std::move(callback);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ImageAsyncService::Shutdown() {
    {
        std::lock_guard lock(m_Mutex);
        if (m_Stopping) return ApplicationResult<void>::Success();
        m_Stopping = true;
        m_Immediate.clear();
        m_Visible.clear();
        m_Background.clear();
        m_Results.clear();
        // Running workers hold their Job shared_ptr locally and will observe
        // the stopping state in Complete().  Clearing the registry here also
        // makes the post-Shutdown snapshot truthful and prevents queued work
        // from surviving the service lifecycle.
        m_Jobs.clear();
        m_WakeCallback = {};
    }
    m_Wake.notify_all();
    for (auto& worker : m_Workers)
        if (worker.joinable()) worker.join();
    {
        std::lock_guard lock(m_Mutex);
        m_Workers.clear();
    }
    return ApplicationResult<void>::Success();
}

ImageAsyncSnapshot ImageAsyncService::Snapshot() const {
    std::lock_guard lock(m_Mutex);
    ImageAsyncSnapshot result;
    result.workerCount = m_Workers.size();
    result.queuedTasks = m_Immediate.size() + m_Visible.size() +
        m_Background.size();
    for (const auto& [token, job] : m_Jobs)
        if (job->running) ++result.runningTasks;
    result.pendingResults = m_Results.size();
    result.maxQueuedTasks = m_Options.maxQueuedTasks;
    result.maxPendingResults = m_Options.maxPendingResults;
    result.completedTasks = m_Completed;
    result.cancelledTasks = m_Cancelled;
    result.failedTasks = m_Failed;
    result.droppedResults = m_DroppedResults;
    result.shuttingDown = m_Stopping;
    return result;
}

bool ImageAsyncService::IsShuttingDown() const noexcept {
    std::lock_guard lock(m_Mutex);
    return m_Stopping;
}

} // namespace Engine
