#include "Application/ApplicationContext.h"

#ifdef _WIN32

#include "Application/DesktopServices.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

namespace {

using namespace Engine;

int Fail(const char* message, int code) {
    std::cerr << message << '\n';
    return code;
}

ApplicationResult<std::unique_ptr<ApplicationContext>> CreateContext() {
    ApplicationContextOptions options;
    options.applicationName = "XiE Desktop Instance Helper";
    options.initializeBackends = false;
    options.desktopBackend = CreateWin32DesktopBackend();
    return ApplicationContext::Create(std::move(options));
}

bool WriteReadyFile(const std::filesystem::path& path) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    stream << "ready\n";
    return stream.good();
}

int RunPrimary(const std::string& applicationId,
    const std::filesystem::path& readyFile,
    const std::filesystem::path& stopFile,
    const std::filesystem::path& activationFile) {
    auto contextResult = CreateContext();
    if (!contextResult) return Fail("primary: context creation failed", 10);
    auto context = std::move(contextResult.Value());

    if (const auto started = context->Desktop().Start(); !started) {
        return Fail("primary: desktop service start failed", 11);
    }
    SingleInstanceOptions options;
    options.applicationId = applicationId;
    options.timeoutMilliseconds = 5000;
    std::uint32_t activationCount = 0;
    options.onPrimaryActivate = [&activationCount, activationFile] {
        ++activationCount;
        std::ofstream stream(activationFile,
            std::ios::binary | std::ios::trunc);
        stream << activationCount << '\n';
    };
    auto acquiredResult = context->Desktop().Instances().Acquire(options);
    if (!acquiredResult) return Fail("primary: acquire call failed", 12);
    auto acquired = std::move(acquiredResult.Value());
    if (acquired.status != SingleInstanceAcquireStatus::PrimaryAcquired ||
        !acquired.primary.has_value()) {
        return Fail("primary: did not acquire primary instance", 13);
    }
    if (!WriteReadyFile(readyFile)) {
        return Fail("primary: ready file could not be written", 14);
    }

    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(30);
    while (!std::filesystem::exists(stopFile) &&
        std::chrono::steady_clock::now() < deadline) {
        const auto pumped = context->PumpDesktopEvents();
        if (!pumped) return Fail("primary: desktop event pump failed", 15);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!std::filesystem::exists(stopFile)) {
        return Fail("primary: stop timeout", 16);
    }
    return context->Shutdown() ? 0 : Fail("primary: shutdown failed", 17);
}

int RunSecondary(const std::string& applicationId,
    std::uint32_t timeoutMilliseconds = 5000) {
    auto contextResult = CreateContext();
    if (!contextResult) return Fail("secondary: context creation failed", 20);
    auto context = std::move(contextResult.Value());

    if (const auto started = context->Desktop().Start(); !started) {
        return Fail("secondary: desktop service start failed", 21);
    }
    SingleInstanceOptions options;
    options.applicationId = applicationId;
    options.timeoutMilliseconds = timeoutMilliseconds;
    auto acquiredResult = context->Desktop().Instances().Acquire(options);
    if (!acquiredResult) return Fail("secondary: acquire call failed", 22);
    const auto status = acquiredResult.Value().status;
    if (status != SingleInstanceAcquireStatus::ExistingInstanceActivated) {
        std::cerr << "secondary: status=" << static_cast<int>(status)
                  << " message=" << acquiredResult.Value().message << '\n';
    }
    if (status == SingleInstanceAcquireStatus::ExistingInstanceActivated) {
        (void)context->Shutdown();
        return 0;
    }
    if (status == SingleInstanceAcquireStatus::AlreadyRunningButUnreachable) {
        (void)context->Shutdown();
        return 2;
    }
    (void)context->Shutdown();
    return Fail("secondary: expected an existing primary instance", 23);
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--primary" && argc == 6) {
        return RunPrimary(argv[2], argv[3], argv[4], argv[5]);
    }
    if (argc == 3 && std::string(argv[1]) == "--secondary") {
        return RunSecondary(argv[2]);
    }
    if (argc == 4 && std::string(argv[1]) == "--secondary-timeout") {
        try {
            const auto timeout = static_cast<std::uint32_t>(
                std::stoul(argv[3]));
            return RunSecondary(argv[2], timeout);
        } catch (...) {
            return Fail("secondary: invalid timeout", 24);
        }
    }
    std::cerr << "usage: --primary <application-id> <ready-file> <stop-file> "
                 "<activation-file>\n"
                 "       --secondary <application-id>\n"
                 "       --secondary-timeout <application-id> <milliseconds>\n";
    return 2;
}

#else

int main() { return 0; }

#endif
