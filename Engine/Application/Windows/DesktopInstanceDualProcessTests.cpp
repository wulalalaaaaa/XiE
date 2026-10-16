#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t kActivationMagic = 0x31414958u;
constexpr std::uint16_t kActivationVersion = 1;
constexpr std::uint16_t kActivatePrimary = 1;
constexpr std::uint16_t kActivationAccepted = 0;

#pragma pack(push, 1)
struct ActivationRequest {
    std::uint32_t magic = kActivationMagic;
    std::uint16_t version = kActivationVersion;
    std::uint16_t command = kActivatePrimary;
    std::uint32_t payloadBytes = 0;
};

struct ActivationResponse {
    std::uint32_t magic = kActivationMagic;
    std::uint16_t version = kActivationVersion;
    std::uint16_t status = 1;
};
#pragma pack(pop)

std::wstring Quote(const std::wstring& value) {
    std::wstring result = L"\"";
    for (const wchar_t character : value) {
        if (character == L'\"') result += L'\\';
        result += character;
    }
    result += L'\"';
    return result;
}

bool StartProcess(const std::wstring& command,
    PROCESS_INFORMATION& process) {
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    std::vector<wchar_t> commandLine(command.begin(), command.end());
    commandLine.push_back(L'\0');
    return CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE;
}

std::uint64_t StableId(std::string_view value) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::wstring HexId(std::uint64_t value) {
    wchar_t buffer[17]{};
    (void)swprintf_s(buffer, ARRAYSIZE(buffer), L"%016llX",
        static_cast<unsigned long long>(value));
    return buffer;
}

std::wstring CurrentSessionSuffix() {
    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) session = 0;
    return std::to_wstring(session);
}

std::wstring InstanceMutexName(const std::string& applicationId) {
    return L"Local\\XiE.DesktopServices.Instance." +
        CurrentSessionSuffix() + L"." + HexId(StableId(applicationId));
}

std::wstring ActivationPipeName(const std::string& applicationId) {
    return L"\\\\.\\pipe\\XiE.DesktopServices.Activate." +
        CurrentSessionSuffix() + L"." + HexId(StableId(applicationId));
}

bool SendRawActivation(const std::string& applicationId,
    std::uint32_t magic, std::uint16_t version, std::uint16_t command,
    std::uint32_t payloadBytes, DWORD timeout, bool& accepted) {
    accepted = false;
    const auto pipeName = ActivationPipeName(applicationId);
    const ULONGLONG deadline = GetTickCount64() + timeout;
    for (;;) {
        if (WaitNamedPipeW(pipeName.c_str(), 50) != FALSE) break;
        const DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND &&
            error != ERROR_PIPE_BUSY) return false;
        if (GetTickCount64() >= deadline) return false;
        Sleep(1);
    }
    HANDLE pipe = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return false;
    ActivationRequest request;
    request.magic = magic;
    request.version = version;
    request.command = command;
    request.payloadBytes = payloadBytes;
    DWORD written = 0;
    const BOOL writeOk = WriteFile(pipe, &request, sizeof(request), &written,
        nullptr);
    ActivationResponse response{};
    DWORD read = 0;
    const BOOL readOk = writeOk != FALSE &&
        ReadFile(pipe, &response, sizeof(response), &read, nullptr);
    CloseHandle(pipe);
    if (readOk == FALSE || written != sizeof(request) ||
        read != sizeof(response)) return false;
    accepted = response.magic == kActivationMagic &&
        response.version == kActivationVersion &&
        response.status == kActivationAccepted;
    return true;
}

bool WaitForFile(const std::filesystem::path& path,
    std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (std::filesystem::exists(path)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return std::filesystem::exists(path);
}

bool WriteFileMarker(const std::filesystem::path& path) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << "stop\n";
    return stream.good();
}

int StopProcess(PROCESS_INFORMATION& process, DWORD timeout) {
    if (process.hProcess == nullptr) {
        if (process.hThread != nullptr) CloseHandle(process.hThread);
        process = {};
        return 1;
    }
    const DWORD result = WaitForSingleObject(process.hProcess, timeout);
    if (result == WAIT_TIMEOUT) {
        (void)TerminateProcess(process.hProcess, 1);
        (void)WaitForSingleObject(process.hProcess, 5000);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        process = {};
        return 1;
    }
    DWORD exitCode = 1;
    (void)GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    process = {};
    return static_cast<int>(exitCode);
}

bool KillProcess(PROCESS_INFORMATION& process, DWORD timeout) {
    if (process.hProcess == nullptr) return false;
    if (TerminateProcess(process.hProcess, 1) == FALSE) return false;
    const bool exited = WaitForSingleObject(process.hProcess, timeout) ==
        WAIT_OBJECT_0;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    process = {};
    return exited;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "expected the helper executable path\n";
        return 2;
    }
    const auto root = std::filesystem::temp_directory_path() /
        ("xie_desktop_instance_dual_" +
            std::to_string(GetCurrentProcessId()));
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);
    if (error) {
        std::cerr << "could not create test directory\n";
        return 3;
    }

    const std::wstring helper = std::filesystem::path(argv[1]).wstring();
    const auto makePrimaryCommand = [&](const std::string& applicationId,
        const std::filesystem::path& ready,
        const std::filesystem::path& stop,
        const std::filesystem::path& activation) {
        return Quote(helper) + L" --primary " +
            Quote(std::wstring(applicationId.begin(), applicationId.end())) + L" " +
            Quote(ready.wstring()) + L" " + Quote(stop.wstring()) + L" " +
            Quote(activation.wstring());
    };

    int result = 0;
    const std::string applicationId =
        "xie.phase4.dual." + std::to_string(GetCurrentProcessId());
    const auto ready = root / "primary.ready";
    const auto stop = root / "primary.stop";
    const auto activation = root / "primary.activation";
    PROCESS_INFORMATION primary{};
    if (!StartProcess(makePrimaryCommand(applicationId, ready, stop, activation),
            primary) || !WaitForFile(ready, std::chrono::seconds(10))) {
        std::cerr << "primary helper did not become ready\n";
        (void)StopProcess(primary, 1000);
        std::filesystem::remove_all(root, error);
        return 4;
    }

    // Bad protocol fields receive a rejection and must not reach the primary
    // callback.  The test then uses the same endpoint for a burst of valid
    // requests to exercise bounded/coalesced notification handling.
    bool accepted = false;
    if (!SendRawActivation(applicationId, 0, kActivationVersion,
            kActivatePrimary, 0, 500, accepted) || accepted) result = 5;
    if (result == 0 && (!SendRawActivation(applicationId, kActivationMagic, 99,
            kActivatePrimary, 0, 500, accepted) || accepted)) result = 6;
    if (result == 0 && (!SendRawActivation(applicationId, kActivationMagic,
            kActivationVersion, kActivatePrimary, 1, 500, accepted) || accepted)) {
        result = 7;
    }

    if (result == 0) {
        const std::wstring secondaryCommand = Quote(helper) + L" --secondary " +
            Quote(std::wstring(applicationId.begin(), applicationId.end()));
        PROCESS_INFORMATION secondary{};
        if (!StartProcess(secondaryCommand, secondary)) {
            std::cerr << "could not start secondary helper\n";
            result = 8;
        } else {
            result = StopProcess(secondary, 10000);
            if (result != 0) {
                std::cerr << "secondary helper did not report activation success "
                          << "(exit code " << result << ")\n";
            }
        }
    }
    if (result == 0 && !WaitForFile(activation, std::chrono::seconds(5))) {
        std::cerr << "primary helper did not report activation callback\n";
        result = 9;
    }
    if (result == 0) {
        std::ifstream stream(activation, std::ios::binary);
        std::string value;
        std::getline(stream, value);
        if (value != "1") {
            std::cerr << "primary helper activation count was not one\n";
            result = 10;
        }
    }

    if (result == 0) {
        int acceptedBurst = 0;
        for (int index = 0; index < 64; ++index) {
            if (SendRawActivation(applicationId, kActivationMagic,
                    kActivationVersion, kActivatePrimary, 0, 1000, accepted) &&
                accepted) {
                ++acceptedBurst;
            }
        }
        if (acceptedBurst == 0) {
            std::cerr << "valid activation burst was not accepted\n";
            result = 11;
        }
    }

    if (!WriteFileMarker(stop) && result == 0) result = 12;
    const int primaryResult = StopProcess(primary, 10000);
    if (primaryResult != 0 && result == 0) result = primaryResult;
    if (result == 0 && SendRawActivation(applicationId, kActivationMagic,
            kActivationVersion, kActivatePrimary, 0, 200, accepted)) {
        std::cerr << "activation endpoint survived primary shutdown\n";
        result = 13;
    }

    // Reacquisition after a clean primary shutdown proves that the mutex and
    // activation endpoint do not survive as a stale ownership claim.
    if (result == 0) {
        const auto readyAgain = root / "primary-again.ready";
        const auto stopAgain = root / "primary-again.stop";
        const auto activationAgain = root / "primary-again.activation";
        PROCESS_INFORMATION primaryAgain{};
        if (!StartProcess(makePrimaryCommand(applicationId, readyAgain, stopAgain,
                activationAgain), primaryAgain) ||
            !WaitForFile(readyAgain, std::chrono::seconds(10))) {
            std::cerr << "primary helper could not reacquire instance\n";
            (void)StopProcess(primaryAgain, 1000);
            result = 14;
        } else {
            if (!WriteFileMarker(stopAgain)) result = 15;
            const int primaryAgainResult = StopProcess(primaryAgain, 10000);
            if (primaryAgainResult != 0 && result == 0) result = primaryAgainResult;
        }
    }

    // Hold only the session-local mutex without an endpoint.  A secondary
    // must return AlreadyRunningButUnreachable within the requested timeout.
    if (result == 0) {
        const std::string unreachableId =
            "xie.phase4.unreachable." + std::to_string(GetCurrentProcessId());
        SetLastError(ERROR_SUCCESS);
        HANDLE holder = CreateMutexW(nullptr, TRUE,
            InstanceMutexName(unreachableId).c_str());
        if (holder == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
            if (holder != nullptr) CloseHandle(holder);
            result = 16;
        } else {
            const auto started = std::chrono::steady_clock::now();
            const std::wstring secondaryCommand = Quote(helper) +
                L" --secondary-timeout " +
                Quote(std::wstring(unreachableId.begin(), unreachableId.end())) +
                L" 150";
            PROCESS_INFORMATION secondary{};
            if (!StartProcess(secondaryCommand, secondary)) {
                result = 17;
            } else {
                const int secondaryResult = StopProcess(secondary, 3000);
                const auto elapsed = std::chrono::steady_clock::now() - started;
                if (secondaryResult != 2 || elapsed > std::chrono::seconds(2)) {
                    std::cerr << "unreachable endpoint status or timeout was wrong\n";
                    result = 18;
                }
            }
            CloseHandle(holder);
        }
    }

    // Terminate a primary without its normal shutdown and verify the mutex and
    // pipe are recoverable by a new primary in the same session.
    if (result == 0) {
        const std::string crashId =
            "xie.phase4.crash." + std::to_string(GetCurrentProcessId());
        const auto crashReady = root / "crash.ready";
        const auto crashStop = root / "crash.stop";
        const auto crashActivation = root / "crash.activation";
        PROCESS_INFORMATION crashed{};
        if (!StartProcess(makePrimaryCommand(crashId, crashReady, crashStop,
                crashActivation), crashed) ||
            !WaitForFile(crashReady, std::chrono::seconds(10)) ||
            !KillProcess(crashed, 10000)) {
            std::cerr << "primary crash setup did not terminate cleanly\n";
            (void)StopProcess(crashed, 1000);
            result = 19;
        } else {
            const auto recoveryReady = root / "crash-recovery.ready";
            const auto recoveryStop = root / "crash-recovery.stop";
            const auto recoveryActivation = root / "crash-recovery.activation";
            PROCESS_INFORMATION recovered{};
            if (!StartProcess(makePrimaryCommand(crashId, recoveryReady,
                    recoveryStop, recoveryActivation), recovered) ||
                !WaitForFile(recoveryReady, std::chrono::seconds(10))) {
                std::cerr << "primary could not recover after crash\n";
                (void)StopProcess(recovered, 1000);
                result = 20;
            } else {
                if (!WriteFileMarker(recoveryStop)) result = 21;
                const int recoveredResult = StopProcess(recovered, 10000);
                if (recoveredResult != 0 && result == 0) result = recoveredResult;
            }
        }
    }

    if (result != 0) std::cerr << "desktop instance matrix result=" << result
        << '\n';
    std::filesystem::remove_all(root, error);
    return result;
}

#else

int main() { return 0; }

#endif
