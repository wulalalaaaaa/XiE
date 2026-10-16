#include "Image/ImageAsync.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace {

int failures = 0;
int checks = 0;

void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

Engine::ImageBuffer MakeImage() {
    Engine::ImageDescriptor descriptor;
    descriptor.width = 1;
    descriptor.height = 1;
    descriptor.rowStride = 4;
    descriptor.pixelFormat = Engine::ImagePixelFormat::RGBA8;
    const auto result = Engine::ImageBuffer::Allocate(descriptor);
    if (!result) return {};
    auto image = std::move(result.Value());
    image.MutableBytes()[0] = std::byte{1};
    image.MutableBytes()[1] = std::byte{2};
    image.MutableBytes()[2] = std::byte{3};
    image.MutableBytes()[3] = std::byte{255};
    return image;
}

class FakeFileService final : public Engine::IImageFileService {
public:
    Engine::ApplicationResult<Engine::ImageEncodedPayload> ReadEncoded(
        const std::filesystem::path&, const Engine::ImageFileReadOptions&) const override {
        return Engine::ApplicationResult<Engine::ImageEncodedPayload>::Failure({
            Engine::ApplicationErrorCode::Unsupported, "Read", "fake",
            "not used", std::nullopt, "fake"});
    }

    Engine::ApplicationResult<Engine::ImageMetadata> Inspect(
        const std::filesystem::path&, const Engine::ImageFileReadOptions&) const override {
        return Engine::ApplicationResult<Engine::ImageMetadata>::Failure({
            Engine::ApplicationErrorCode::Unsupported, "Inspect", "fake",
            "not used", std::nullopt, "fake"});
    }

    Engine::ApplicationResult<Engine::ImageDecodedFile> Decode(
        const std::filesystem::path& path,
        const Engine::ImageDecodeOptions&) const override {
        if (path.filename().string().find("slow") != std::string::npos)
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
        Engine::ImageMetadata metadata;
        metadata.encodedFormat = Engine::ImageEncodedFormat::Png;
        metadata.width = 1;
        metadata.height = 1;
        metadata.frameCount = 1;
        metadata.orientation = Engine::ImageOrientation::TopDown;
        return Engine::ApplicationResult<Engine::ImageDecodedFile>::Success({
            MakeImage(), metadata});
    }

    Engine::ApplicationResult<Engine::ImageDecodedFile> DecodeThumbnail(
        const std::filesystem::path& path,
        const Engine::ImageResizeOptions&,
        const Engine::ImageDecodeOptions& decode) const override {
        return Decode(path, decode);
    }

    const char* BackendName() const noexcept override { return "fake"; }
};

bool WaitFor(const std::function<bool()>& predicate) {
    for (int index = 0; index < 200; ++index) {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return predicate();
}

} // namespace

int main() {
    auto files = std::make_shared<FakeFileService>();
    Engine::ImageAsyncOptions options;
    options.workerCount = 1;
    options.maxQueuedTasks = 1;
    options.maxPendingResults = 8;
    Engine::ImageAsyncService service(files, options);
    std::atomic<int> wakes = 0;
    Check(service.SetWakeCallback([&wakes] { ++wakes; }).HasValue(),
        "async service accepts an owner wake callback");

    Engine::ImageAsyncRequest fast;
    fast.sourcePath = "fast.png";
    fast.resize = Engine::ImageResizeOptions{2, 2, false,
        Engine::ImageResizeFilter::Nearest, {}};
    Check(service.Snapshot().workerCount == 0,
        "async worker pool is lazy before the first image task");
    const auto fastToken = service.Submit(fast);
    Check(fastToken.HasValue(), "async service returns an owned task token");
    Check(WaitFor([&service] { return service.Snapshot().workerCount == 1; }),
        "first image task starts the bounded worker pool");
    Check(WaitFor([&wakes] { return wakes.load() > 0; }),
        "worker completion wakes the owner without touching UI");
    bool complete = false;
    const auto drained = service.DrainResults(4,
        [&complete](Engine::ImageAsyncResult result) {
            complete = result.state == Engine::ImageTaskState::Completed &&
                result.value && result.value->image.ByteSize() == 4;
        });
    Check(drained && complete, "owner thread drains an owned ImageBuffer result");

    Engine::ApplicationResult<std::size_t> wrongThread;
    std::thread wrongThreadCaller([&service, &wrongThread] {
        wrongThread = service.DrainResults(1, {});
    });
    wrongThreadCaller.join();
    Check(!wrongThread && wrongThread.Error().code ==
        Engine::ApplicationErrorCode::WrongThread,
        "async results reject a non-owner consumer thread");

    Engine::ImageAsyncRequest slow;
    slow.sourcePath = "slow.png";
    const auto slowToken = service.Submit(slow);
    Check(slowToken.HasValue(), "running cancellation task is accepted");
    Check(WaitFor([&service] { return service.Snapshot().runningTasks == 1; }),
        "task transitions to running with a bounded worker count");
    const auto full = service.Submit(slow);
    Check(!full && full.Error().code == Engine::ApplicationErrorCode::QueueFull,
        "bounded queue rejects overload structurally");
    Check(service.Cancel(slowToken.Value()).HasValue(),
        "running cancellation is accepted and idempotent");
    Check(service.Cancel(slowToken.Value()).HasValue(),
        "repeated cancellation is harmless");
    bool cancelled = false;
    Check(WaitFor([&service, &cancelled] {
        const auto result = service.DrainResults(4,
            [&cancelled](Engine::ImageAsyncResult value) {
                cancelled = value.state == Engine::ImageTaskState::Cancelled;
            });
        return result && cancelled;
    }), "running cancellation reaches the owner as Cancelled");

    Engine::ImageAsyncRequest race;
    race.sourcePath = "race.png";
    const auto raceToken = service.Submit(race);
    Check(raceToken.HasValue(), "completion race task is accepted");
    Check(WaitFor([&service] { return service.Snapshot().pendingResults > 0; }),
        "completed task is held in the bounded result queue");
    Check(service.Cancel(raceToken.Value()).HasValue(),
        "cancellation can remove an already queued completion");
    bool raceDelivered = false;
    (void)service.DrainResults(4, [&, raceToken](Engine::ImageAsyncResult result) {
        raceDelivered = result.token == raceToken.Value();
    });
    Check(!raceDelivered, "cancelled completion cannot reach a stale consumer");

    Check(service.Shutdown().HasValue(), "shutdown joins workers and clears results");
    Check(service.Shutdown().HasValue(), "repeated async shutdown is idempotent");
    Check(service.Snapshot().queuedTasks == 0 &&
        service.Snapshot().runningTasks == 0 &&
        service.Snapshot().pendingResults == 0,
        "shutdown leaves no queued, running or pending image task state");
    const auto afterShutdown = service.Submit(fast);
    Check(!afterShutdown && afterShutdown.Error().code ==
        Engine::ApplicationErrorCode::ShuttingDown,
        "submit after shutdown returns ShuttingDown");

    std::cout << "XiE image async contract tests: " << checks
              << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
