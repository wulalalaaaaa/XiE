#include "Image/Windows/ImageServicesWin32.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void Print(const char* phase, const char* result, int code,
    const std::string& detail = {}) {
    std::cout << "{\"tool\":\"xie_image_native_clipboard_smoke\","
              << "\"phase\":\"" << phase << "\","
              << "\"result\":\"" << result << "\","
              << "\"code\":" << code;
    if (!detail.empty())
        std::cout << ",\"detail\":\"" << detail << "\"";
    std::cout << "}\n";
}

} // namespace

int main() {
    if (std::getenv("XIE_RUN_IMAGE_CLIPBOARD_SMOKE") == nullptr) {
        Print("preflight", "not_run", 0,
            "set XIE_RUN_IMAGE_CLIPBOARD_SMOKE=1 to read the current system clipboard");
        return 0;
    }

    const auto services = Engine::CreateWin32ImageServices();
    if (!services || !services->HasClipboard()) {
        Print("native_clipboard", "backend_unavailable", 20,
            "optional Win32 image clipboard service is unavailable");
        return 20;
    }
    const auto clipboard = services->Clipboard();
    const auto result = clipboard->ReadImage();
    if (result) {
        Print("native_clipboard", "passed", 0,
            "read-only clipboard image captured");
        (void)services->Shutdown();
        return 0;
    }

    int code = 22;
    const auto error = result.Error().code;
    if (error == Engine::ApplicationErrorCode::NoData) code = 21;
    else if (error == Engine::ApplicationErrorCode::Busy) code = 23;
    else if (error == Engine::ApplicationErrorCode::BackendUnavailable) code = 20;
    else if (error == Engine::ApplicationErrorCode::TooLarge) code = 24;
    Print("native_clipboard", code == 21 ? "not_tested" : "failed", code,
        result.Error().message);
    (void)services->Shutdown();
    return code;
}
