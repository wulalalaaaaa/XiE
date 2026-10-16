#include "Input/InputDiagnostics.h"

#include <mutex>
#include <atomic>
#include <utility>

namespace Engine {
namespace {
std::mutex g_InputDiagnosticMutex;
InputDiagnosticSink g_InputDiagnosticSink;
std::atomic_bool g_InputDiagnosticsEnabled{false};
}

void SetInputDiagnosticSink(InputDiagnosticSink sink) {
    std::scoped_lock lock(g_InputDiagnosticMutex);
    g_InputDiagnosticSink = std::move(sink);
    g_InputDiagnosticsEnabled.store(
        static_cast<bool>(g_InputDiagnosticSink),
        std::memory_order_release);
}

bool IsInputDiagnosticsEnabled() noexcept {
    return g_InputDiagnosticsEnabled.load(std::memory_order_acquire);
}

void EmitInputDiagnostic(const InputDiagnosticRecord& record) {
    if (!IsInputDiagnosticsEnabled()) return;
    InputDiagnosticSink sink;
    {
        std::scoped_lock lock(g_InputDiagnosticMutex);
        sink = g_InputDiagnosticSink;
    }
    if (sink) sink(record);
}

} // namespace Engine
