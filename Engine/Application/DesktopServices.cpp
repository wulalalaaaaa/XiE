#include "Application/DesktopServices.h"

#include <algorithm>
#include <cctype>
#include <deque>
#include <mutex>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Engine::Detail {

struct TrayRecord {
    std::vector<TrayMenuItem> menu;
    std::string tooltip;
    std::function<void(DesktopCommandId)> onCommand;
    std::function<void()> onDoubleClick;
};

struct ShortcutRecord {
    std::function<void()> onPressed;
};

struct InstanceRecord {
    std::function<void()> onActivate;
};

struct DesktopServicesState {
    std::thread::id ownerThread = std::this_thread::get_id();
    std::unique_ptr<IDesktopBackend> backend;
    DesktopBackendCapabilities capabilities;
    DesktopHealthSnapshot health;
    std::function<void()> wakeOwnerThread;
    std::string lastError;
    bool started = false;
    bool shuttingDown = false;

    mutable std::mutex mutex;
    std::deque<DesktopBackendEvent> events;
    std::unordered_map<DesktopRegistrationId, TrayRecord> trays;
    std::unordered_map<DesktopRegistrationId, ShortcutRecord> shortcuts;
    std::unordered_map<DesktopRegistrationId, InstanceRecord> instances;
};

} // namespace Engine::Detail

namespace Engine {
namespace {

using Detail::DesktopServicesState;

constexpr std::size_t kMaxPendingDesktopEvents = 256;

ApplicationError MakeError(ApplicationErrorCode code,
    std::string operation, std::string capability, std::string message,
    std::string backend = {}, std::optional<std::int64_t> nativeCode = {}) {
    return {code, std::move(operation), std::move(capability),
        std::move(message), nativeCode, std::move(backend)};
}

template <typename T>
ApplicationResult<T> Failure(ApplicationError error) {
    return ApplicationResult<T>::Failure(std::move(error));
}

CapabilityHealth* SelectHealth(DesktopHealthSnapshot& snapshot,
    ApplicationCapability capability) noexcept {
    switch (capability) {
    case ApplicationCapability::DesktopIntegration:
        return &snapshot.desktopIntegration;
    case ApplicationCapability::SystemTray:
        return &snapshot.systemTray;
    case ApplicationCapability::GlobalShortcut:
        return &snapshot.globalShortcut;
    case ApplicationCapability::SingleInstance:
        return &snapshot.singleInstance;
    case ApplicationCapability::PrimaryInstanceActivation:
        return &snapshot.primaryInstanceActivation;
    default:
        return nullptr;
    }
}

bool Provides(const DesktopBackendCapabilities& capabilities,
    ApplicationCapability capability) noexcept {
    switch (capability) {
    case ApplicationCapability::DesktopIntegration:
        return capabilities.systemTray || capabilities.globalShortcut ||
            capabilities.singleInstance || capabilities.primaryInstanceActivation;
    case ApplicationCapability::SystemTray: return capabilities.systemTray;
    case ApplicationCapability::GlobalShortcut:
        return capabilities.globalShortcut;
    case ApplicationCapability::SingleInstance:
        return capabilities.singleInstance;
    case ApplicationCapability::PrimaryInstanceActivation:
        return capabilities.primaryInstanceActivation;
    default:
        return false;
    }
}

std::string InitialHealthReason(
    const DesktopBackendCapabilities& capabilities,
    ApplicationCapability capability) {
    if (!Provides(capabilities, capability)) {
        if (capabilities.backend.find("not_linked") != std::string::npos) {
            return "module_not_linked:optional_desktop_services";
        }
        return std::string("backend_unavailable:") + ToString(capability);
    }
    if (capability == ApplicationCapability::SystemTray &&
        capabilities.environment != DesktopEnvironmentStatus::Unknown &&
        capabilities.environment != DesktopEnvironmentStatus::Ready) {
        return capabilities.environmentReason.empty()
            ? std::string("environment:") + ToString(capabilities.environment)
            : capabilities.environmentReason;
    }
    return std::string("service_not_initialized:") + ToString(capability);
}

void InitializeHealth(DesktopHealthSnapshot& health,
    const DesktopBackendCapabilities& capabilities) {
    for (const auto capability : {
             ApplicationCapability::DesktopIntegration,
             ApplicationCapability::SystemTray,
             ApplicationCapability::GlobalShortcut,
             ApplicationCapability::SingleInstance,
             ApplicationCapability::PrimaryInstanceActivation}) {
        if (auto* value = SelectHealth(health, capability)) {
            value->status = CapabilityStatus::Unavailable;
            value->reason = InitialHealthReason(capabilities, capability);
            value->revision = 1;
        }
    }
}

void MarkHealthSuccessLocked(DesktopServicesState& state,
    ApplicationCapability capability, std::string operation) {
    auto* health = SelectHealth(state.health, capability);
    if (health == nullptr) return;
    health->status = CapabilityStatus::Supported;
    health->reason.clear();
    health->lastSuccessfulOperation = std::move(operation);
    health->consecutiveFailures = 0;
    health->recovering = false;
    if (health->revision != std::numeric_limits<std::uint64_t>::max()) {
        ++health->revision;
    }
}

void MarkHealthFailureLocked(DesktopServicesState& state,
    ApplicationCapability capability, CapabilityStatus status,
    std::string operation, std::string reason,
    std::optional<std::int64_t> nativeError, bool recovering) {
    auto* health = SelectHealth(state.health, capability);
    if (health == nullptr) return;
    health->status = status;
    health->reason = reason.empty() ? "backend_operation_failed" : std::move(reason);
    health->lastFailedOperation = std::move(operation);
    health->lastNativeError = nativeError;
    if (health->consecutiveFailures != std::numeric_limits<std::uint32_t>::max()) {
        ++health->consecutiveFailures;
    }
    health->recovering = recovering;
    if (health->revision != std::numeric_limits<std::uint64_t>::max()) {
        ++health->revision;
    }
}

void MarkServiceUnavailableLocked(DesktopServicesState& state,
    ApplicationCapability capability, std::string reason) {
    auto* health = SelectHealth(state.health, capability);
    if (health == nullptr) return;
    health->status = CapabilityStatus::Unavailable;
    health->reason = std::move(reason);
    health->recovering = false;
    if (health->revision != std::numeric_limits<std::uint64_t>::max()) {
        ++health->revision;
    }
}

void RecordBackendFailure(const std::shared_ptr<DesktopServicesState>& state,
    ApplicationCapability capability, const ApplicationError& error,
    CapabilityStatus status = CapabilityStatus::Degraded,
    std::string reason = {}) {
    if (!state) return;
    std::lock_guard lock(state->mutex);
    state->lastError = error.message;
    if (error.code == ApplicationErrorCode::BackendFailure) {
        MarkHealthFailureLocked(*state, capability, status,
            error.operation.empty() ? "DesktopOperation" : error.operation,
            std::move(reason), error.nativeCode, false);
    }
}

void RefreshBackendProbe(const std::shared_ptr<DesktopServicesState>& state) {
    if (!state) return;
    IDesktopBackend* backend = nullptr;
    {
        std::lock_guard lock(state->mutex);
        backend = state->backend.get();
    }
    if (backend == nullptr) return;
    const auto probed = backend->Probe();
    std::lock_guard lock(state->mutex);
    const auto previous = state->capabilities;
    state->capabilities = probed;
    if (!state->started) return;
    const bool environmentChanged = previous.environment != probed.environment ||
        previous.systemTray != probed.systemTray;
    if (!environmentChanged) return;
    if (!probed.systemTray) {
        MarkHealthFailureLocked(*state, ApplicationCapability::SystemTray,
            CapabilityStatus::Unavailable, "ProbeDesktopEnvironment",
            probed.environmentReason.empty()
                ? "backend_unavailable:system_tray"
                : probed.environmentReason,
            probed.environmentNativeError, true);
    } else if (probed.environment == DesktopEnvironmentStatus::Ready ||
        probed.environment == DesktopEnvironmentStatus::Unknown) {
        MarkHealthSuccessLocked(*state, ApplicationCapability::SystemTray,
            "ProbeDesktopEnvironment");
    } else {
        MarkHealthFailureLocked(*state, ApplicationCapability::SystemTray,
            CapabilityStatus::Unavailable, "ProbeDesktopEnvironment",
            probed.environmentReason.empty()
                ? std::string("environment:") + ToString(probed.environment)
                : probed.environmentReason,
            probed.environmentNativeError, true);
    }
}

void ApplyHealthEventLocked(DesktopServicesState& state,
    const DesktopBackendEvent& event) {
    if (event.healthStatus == CapabilityStatus::Supported) {
        MarkHealthSuccessLocked(state, event.capability,
            event.operation.empty() ? "DesktopBackendRecovery" : event.operation);
        auto* health = SelectHealth(state.health, event.capability);
        if (health != nullptr) health->recovering = event.recovering;
        return;
    }
    MarkHealthFailureLocked(state, event.capability, event.healthStatus,
        event.operation.empty() ? "DesktopBackendHealth" : event.operation,
        event.reason, event.nativeError, event.recovering);
}

std::optional<ApplicationError> CheckOwner(
    const std::shared_ptr<DesktopServicesState>& state,
    std::string operation, std::string capability = "desktop") {
    if (!state) {
        return MakeError(ApplicationErrorCode::LifecycleViolation,
            std::move(operation), std::move(capability),
            "desktop services are no longer alive");
    }
    if (std::this_thread::get_id() != state->ownerThread) {
        return MakeError(ApplicationErrorCode::WrongThread,
            std::move(operation), std::move(capability),
            "desktop service operations must run on the application thread",
            state->capabilities.backend);
    }
    return std::nullopt;
}

ApplicationResult<void> CheckUsable(
    const std::shared_ptr<DesktopServicesState>& state,
    std::string operation, std::string capability) {
    if (const auto error = CheckOwner(state, operation, capability)) {
        return ApplicationResult<void>::Failure(*error);
    }
    if (state->shuttingDown) {
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::ShuttingDown, std::move(operation),
            std::move(capability), "desktop services are shutting down",
            state->capabilities.backend));
    }
    if (!state->started || !state->backend) {
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::Unsupported, std::move(operation),
            std::move(capability),
            "the optional desktop backend is not started",
            state->capabilities.backend));
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ValidateTray(const TrayOptions& options) {
    if (options.tooltip.size() > 256) {
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::InvalidOptions, "CreateTray", "system_tray",
            "tray tooltip must be at most 256 bytes"));
    }
    std::unordered_set<DesktopCommandId> ids;
    for (const auto& item : options.menu) {
        if (item.kind == TrayMenuItemKind::Separator) continue;
        if (item.id == 0 || !ids.insert(item.id).second) {
            return ApplicationResult<void>::Failure(MakeError(
                ApplicationErrorCode::InvalidOptions, "CreateTray", "system_tray",
                "tray command IDs must be non-zero and unique"));
        }
        if (item.label.size() > 256) {
            return ApplicationResult<void>::Failure(MakeError(
                ApplicationErrorCode::InvalidOptions, "CreateTray", "system_tray",
                "tray menu labels must be at most 256 bytes"));
        }
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ValidateShortcut(const ShortcutOptions& options) {
    if (options.key == InputKeyCode::Unknown) {
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::InvalidOptions, "RegisterGlobalShortcut",
            "global_shortcut", "shortcut key must be specified"));
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ValidateInstance(const SingleInstanceOptions& options) {
    if (options.applicationId.empty() || options.applicationId.size() > 96) {
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::InvalidOptions, "AcquireSingleInstance",
            "single_instance", "applicationId must be 1..96 bytes"));
    }
    for (const unsigned char value : options.applicationId) {
        if (!(std::isalnum(value) || value == '.' || value == '-' ||
                value == '_')) {
            return ApplicationResult<void>::Failure(MakeError(
                ApplicationErrorCode::InvalidOptions, "AcquireSingleInstance",
                "single_instance",
                "applicationId may contain only letters, digits, '.', '-' and '_'"));
        }
    }
    if (options.maxActivationBytes == 0 ||
        options.maxActivationBytes > 4096 ||
        options.timeoutMilliseconds == 0 ||
        options.timeoutMilliseconds > 60000) {
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::InvalidOptions, "AcquireSingleInstance",
            "single_instance", "activation limits are outside the supported range"));
    }
    return ApplicationResult<void>::Success();
}

void EnqueueEvent(const std::weak_ptr<DesktopServicesState>& weak,
    DesktopBackendEvent event) {
    const auto state = weak.lock();
    if (!state) return;
    std::function<void()> wake;
    {
        std::lock_guard lock(state->mutex);
        if (!state->started || state->shuttingDown) return;
        if (event.type == DesktopBackendEventType::PrimaryInstanceActivated) {
            const auto duplicate = std::find_if(state->events.begin(),
                state->events.end(), [&event](const auto& pending) {
                    return pending.type == event.type &&
                        pending.registration == event.registration;
            });
            if (duplicate != state->events.end()) return;
        }
        if (event.type == DesktopBackendEventType::HealthChanged) {
            const auto duplicate = std::find_if(state->events.begin(),
                state->events.end(), [&event](const auto& pending) {
                    return pending.type == event.type &&
                        pending.capability == event.capability &&
                        pending.healthStatus == event.healthStatus &&
                        pending.reason == event.reason;
                });
            if (duplicate != state->events.end()) return;
        }
        if (state->events.size() >= kMaxPendingDesktopEvents) {
            // Desktop callbacks are notifications, not a durable command
            // log.  Keep the queue bounded and let the next owner-thread
            // Pump observe the newest notification.
            state->events.pop_front();
        }
        state->events.push_back(event);
        wake = state->wakeOwnerThread;
    }
    if (wake) wake();
}

ApplicationResult<void> RemoveTray(
    const std::shared_ptr<DesktopServicesState>& state,
    DesktopRegistrationId registration) {
    const auto usable = CheckUsable(state, "RemoveTray", "system_tray");
    if (!usable) {
        // A token may be destroyed after Shutdown.  Shutdown already removed
        // the native object, so that path is intentionally idempotent.
        if (state && state->shuttingDown) return ApplicationResult<void>::Success();
        return usable;
    }
    {
        std::lock_guard lock(state->mutex);
        if (state->trays.find(registration) == state->trays.end()) {
            return ApplicationResult<void>::Success();
        }
    }
    const auto result = state->backend->RemoveTray(registration);
    if (!result) {
        RecordBackendFailure(state, ApplicationCapability::SystemTray,
            result.Error());
        return result;
    }
    std::lock_guard lock(state->mutex);
    state->trays.erase(registration);
    MarkHealthSuccessLocked(*state, ApplicationCapability::SystemTray,
        "RemoveTray");
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> UnregisterShortcut(
    const std::shared_ptr<DesktopServicesState>& state,
    DesktopRegistrationId registration) {
    const auto usable = CheckUsable(state, "UnregisterGlobalShortcut",
        "global_shortcut");
    if (!usable) {
        if (state && state->shuttingDown) return ApplicationResult<void>::Success();
        return usable;
    }
    {
        std::lock_guard lock(state->mutex);
        if (state->shortcuts.find(registration) == state->shortcuts.end()) {
            return ApplicationResult<void>::Success();
        }
    }
    const auto result = state->backend->UnregisterGlobalShortcut(registration);
    if (!result) {
        RecordBackendFailure(state, ApplicationCapability::GlobalShortcut,
            result.Error());
        return result;
    }
    std::lock_guard lock(state->mutex);
    state->shortcuts.erase(registration);
    MarkHealthSuccessLocked(*state, ApplicationCapability::GlobalShortcut,
        "UnregisterGlobalShortcut");
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> ReleaseInstance(
    const std::shared_ptr<DesktopServicesState>& state,
    DesktopRegistrationId registration) {
    const auto usable = CheckUsable(state, "ReleaseSingleInstance",
        "single_instance");
    if (!usable) {
        if (state && state->shuttingDown) return ApplicationResult<void>::Success();
        return usable;
    }
    {
        std::lock_guard lock(state->mutex);
        if (state->instances.find(registration) == state->instances.end()) {
            return ApplicationResult<void>::Success();
        }
    }
    const auto result = state->backend->ReleaseSingleInstance(registration);
    if (!result) {
        RecordBackendFailure(state, ApplicationCapability::SingleInstance,
            result.Error());
        return result;
    }
    std::lock_guard lock(state->mutex);
    state->instances.erase(registration);
    MarkHealthSuccessLocked(*state, ApplicationCapability::SingleInstance,
        "ReleaseSingleInstance");
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> UpdateTray(
    const std::shared_ptr<DesktopServicesState>& state,
    DesktopRegistrationId registration,
    const std::vector<TrayMenuItem>& menu, std::string tooltip) {
    const auto valid = ValidateTray(TrayOptions{tooltip, {}, menu, {}, {}});
    if (!valid) return valid;
    const auto usable = CheckUsable(state, "UpdateTray", "system_tray");
    if (!usable) {
        if (state && state->shuttingDown) {
            return ApplicationResult<void>::Success();
        }
        return usable;
    }
    Detail::TrayRecord previous;
    {
        std::lock_guard lock(state->mutex);
        const auto found = state->trays.find(registration);
        if (found == state->trays.end()) {
            return ApplicationResult<void>::Failure(MakeError(
                ApplicationErrorCode::LifecycleViolation, "UpdateTray",
                "system_tray", "tray session is no longer active",
                state->capabilities.backend));
        }
        previous = found->second;
        if (tooltip.empty()) tooltip = previous.tooltip;
    }
    const auto updated = state->backend->UpdateTray(registration, menu, tooltip);
    if (!updated) {
        RecordBackendFailure(state, ApplicationCapability::SystemTray,
            updated.Error());
        return updated;
    }
    std::lock_guard lock(state->mutex);
    if (const auto found = state->trays.find(registration);
        found != state->trays.end()) {
        found->second.menu = menu;
        found->second.tooltip = std::move(tooltip);
        MarkHealthSuccessLocked(*state, ApplicationCapability::SystemTray,
            "UpdateTray");
    }
    return ApplicationResult<void>::Success();
}

} // namespace

TraySession::TraySession(std::weak_ptr<Detail::DesktopServicesState> state,
    DesktopRegistrationId registration)
    : m_State(std::move(state)), m_Registration(registration) {}

TraySession::~TraySession() noexcept { (void)Remove(); }

TraySession::TraySession(TraySession&& other) noexcept
    : m_State(std::move(other.m_State)),
      m_Registration(other.m_Registration),
      m_LastError(std::move(other.m_LastError)) {
    other.m_Registration = 0;
}

TraySession& TraySession::operator=(TraySession&& other) noexcept {
    if (this == &other) return *this;
    (void)Remove();
    m_State = std::move(other.m_State);
    m_Registration = other.m_Registration;
    m_LastError = std::move(other.m_LastError);
    other.m_Registration = 0;
    return *this;
}

ApplicationResult<void> TraySession::Update(
    const std::vector<TrayMenuItem>& menu, std::string tooltip) {
    const auto result = ::Engine::UpdateTray(m_State.lock(), m_Registration,
        menu, std::move(tooltip));
    if (!result) m_LastError = result.Error().message;
    else m_LastError.clear();
    return result;
}

ApplicationResult<void> TraySession::Remove() {
    if (m_Registration == 0) return ApplicationResult<void>::Success();
    const auto result = ::Engine::RemoveTray(m_State.lock(), m_Registration);
    if (!result) {
        m_LastError = result.Error().message;
        return result;
    }
    m_LastError.clear();
    m_Registration = 0;
    m_State.reset();
    return ApplicationResult<void>::Success();
}

bool TraySession::IsActive() const noexcept {
    const auto state = m_State.lock();
    if (!state || m_Registration == 0) return false;
    std::lock_guard lock(state->mutex);
    return state->trays.find(m_Registration) != state->trays.end();
}

const std::string& TraySession::LastError() const noexcept { return m_LastError; }

ShortcutRegistration::ShortcutRegistration(
    std::weak_ptr<Detail::DesktopServicesState> state,
    DesktopRegistrationId registration)
    : m_State(std::move(state)), m_Registration(registration) {}

ShortcutRegistration::~ShortcutRegistration() noexcept { (void)Unregister(); }

ShortcutRegistration::ShortcutRegistration(ShortcutRegistration&& other) noexcept
    : m_State(std::move(other.m_State)),
      m_Registration(other.m_Registration),
      m_LastError(std::move(other.m_LastError)) {
    other.m_Registration = 0;
}

ShortcutRegistration& ShortcutRegistration::operator=(
    ShortcutRegistration&& other) noexcept {
    if (this == &other) return *this;
    (void)Unregister();
    m_State = std::move(other.m_State);
    m_Registration = other.m_Registration;
    m_LastError = std::move(other.m_LastError);
    other.m_Registration = 0;
    return *this;
}

ApplicationResult<void> ShortcutRegistration::Unregister() {
    if (m_Registration == 0) return ApplicationResult<void>::Success();
    const auto result = ::Engine::UnregisterShortcut(m_State.lock(),
        m_Registration);
    if (!result) {
        m_LastError = result.Error().message;
        return result;
    }
    m_LastError.clear();
    m_Registration = 0;
    m_State.reset();
    return ApplicationResult<void>::Success();
}

bool ShortcutRegistration::IsRegistered() const noexcept {
    const auto state = m_State.lock();
    if (!state || m_Registration == 0) return false;
    std::lock_guard lock(state->mutex);
    return state->shortcuts.find(m_Registration) != state->shortcuts.end();
}

const std::string& ShortcutRegistration::LastError() const noexcept {
    return m_LastError;
}

SingleInstanceSession::SingleInstanceSession(
    std::weak_ptr<Detail::DesktopServicesState> state,
    DesktopRegistrationId registration)
    : m_State(std::move(state)), m_Registration(registration) {}

SingleInstanceSession::~SingleInstanceSession() noexcept { (void)Release(); }

SingleInstanceSession::SingleInstanceSession(
    SingleInstanceSession&& other) noexcept
    : m_State(std::move(other.m_State)),
      m_Registration(other.m_Registration),
      m_LastError(std::move(other.m_LastError)) {
    other.m_Registration = 0;
}

SingleInstanceSession& SingleInstanceSession::operator=(
    SingleInstanceSession&& other) noexcept {
    if (this == &other) return *this;
    (void)Release();
    m_State = std::move(other.m_State);
    m_Registration = other.m_Registration;
    m_LastError = std::move(other.m_LastError);
    other.m_Registration = 0;
    return *this;
}

ApplicationResult<void> SingleInstanceSession::Release() {
    if (m_Registration == 0) return ApplicationResult<void>::Success();
    const auto result = ::Engine::ReleaseInstance(m_State.lock(),
        m_Registration);
    if (!result) {
        m_LastError = result.Error().message;
        return result;
    }
    m_LastError.clear();
    m_Registration = 0;
    m_State.reset();
    return ApplicationResult<void>::Success();
}

bool SingleInstanceSession::IsPrimary() const noexcept {
    return IsActive();
}

bool SingleInstanceSession::IsActive() const noexcept {
    const auto state = m_State.lock();
    if (!state || m_Registration == 0) return false;
    std::lock_guard lock(state->mutex);
    return state->instances.find(m_Registration) != state->instances.end();
}

const std::string& SingleInstanceSession::LastError() const noexcept {
    return m_LastError;
}

DesktopServices::DesktopServices(std::unique_ptr<IDesktopBackend> backend)
    : m_State(std::make_shared<Detail::DesktopServicesState>()) {
    m_State->backend = std::move(backend);
    if (m_State->backend) {
        m_State->capabilities = m_State->backend->Probe();
    } else {
        m_State->capabilities.backend = "not_linked:optional_desktop_services";
    }
    InitializeHealth(m_State->health, m_State->capabilities);
}

DesktopServices::~DesktopServices() noexcept { (void)Shutdown(); }

DesktopBackendCapabilities DesktopServices::Capabilities() const noexcept {
    if (!m_State) return {};
    RefreshBackendProbe(m_State);
    std::lock_guard lock(m_State->mutex);
    return m_State->capabilities;
}

DesktopHealthSnapshot DesktopServices::Health() const noexcept {
    if (!m_State) return {};
    RefreshBackendProbe(m_State);
    std::lock_guard lock(m_State->mutex);
    return m_State->health;
}

bool DesktopServices::IsStarted() const noexcept {
    if (!m_State) return false;
    std::lock_guard lock(m_State->mutex);
    return m_State->started && !m_State->shuttingDown;
}

const std::string& DesktopServices::LastError() const noexcept {
    static const std::string empty;
    return m_State ? m_State->lastError : empty;
}

ApplicationResult<void> DesktopServices::Start() {
    if (const auto error = CheckOwner(m_State, "StartDesktopServices")) {
        return ApplicationResult<void>::Failure(*error);
    }
    if (m_State->shuttingDown) {
        return ApplicationResult<void>::Failure(MakeError(
            ApplicationErrorCode::ShuttingDown, "StartDesktopServices",
            "desktop", "desktop services are shutting down",
            m_State->capabilities.backend));
    }
    if (m_State->started) return ApplicationResult<void>::Success();
    if (!m_State->backend) {
        m_State->started = true;
        return ApplicationResult<void>::Success();
    }

    const std::weak_ptr<Detail::DesktopServicesState> weak = m_State;
    DesktopBackendStartOptions options;
    options.enqueueEvent = [weak](DesktopBackendEvent event) {
        EnqueueEvent(weak, event);
    };
    options.wakeOwnerThread = [weak] {
        if (const auto state = weak.lock()) {
            std::function<void()> wake;
            {
                std::lock_guard lock(state->mutex);
                wake = state->wakeOwnerThread;
            }
            if (wake) wake();
        }
    };
    const auto result = m_State->backend->Start(options);
    if (!result) {
        std::lock_guard lock(m_State->mutex);
        m_State->lastError = result.Error().message;
        for (const auto capability : {
                 ApplicationCapability::DesktopIntegration,
                 ApplicationCapability::SystemTray,
                 ApplicationCapability::GlobalShortcut,
                 ApplicationCapability::SingleInstance,
                 ApplicationCapability::PrimaryInstanceActivation}) {
            if (Provides(m_State->capabilities, capability) &&
                result.Error().code == ApplicationErrorCode::BackendFailure) {
                MarkHealthFailureLocked(*m_State, capability,
                    CapabilityStatus::Unavailable, "StartDesktopServices",
                    "backend_start_failed", result.Error().nativeCode, false);
            }
        }
        return result;
    }
    {
        std::lock_guard lock(m_State->mutex);
        m_State->started = true;
        m_State->lastError.clear();
        for (const auto capability : {
                 ApplicationCapability::DesktopIntegration,
                 ApplicationCapability::SystemTray,
                 ApplicationCapability::GlobalShortcut,
                 ApplicationCapability::SingleInstance,
                 ApplicationCapability::PrimaryInstanceActivation}) {
            if (!Provides(m_State->capabilities, capability)) continue;
            if (capability == ApplicationCapability::SystemTray &&
                m_State->capabilities.environment !=
                    DesktopEnvironmentStatus::Unknown &&
                m_State->capabilities.environment !=
                    DesktopEnvironmentStatus::Ready) {
                MarkHealthFailureLocked(*m_State, capability,
                    CapabilityStatus::Unavailable, "StartDesktopServices",
                    m_State->capabilities.environmentReason.empty()
                        ? std::string("environment:") +
                            ToString(m_State->capabilities.environment)
                        : m_State->capabilities.environmentReason,
                    m_State->capabilities.environmentNativeError, false);
            } else {
                MarkHealthSuccessLocked(*m_State, capability,
                    "StartDesktopServices");
            }
        }
    }
    RefreshBackendProbe(m_State);
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> DesktopServices::Pump() {
    if (const auto error = CheckOwner(m_State, "PumpDesktopServices")) {
        return ApplicationResult<void>::Failure(*error);
    }
    if (!m_State->started || m_State->shuttingDown) {
        return ApplicationResult<void>::Success();
    }
    if (m_State->backend) m_State->backend->Pump();

    for (;;) {
        DesktopBackendEvent event;
        {
            std::lock_guard lock(m_State->mutex);
            if (m_State->events.empty()) break;
            event = m_State->events.front();
            m_State->events.pop_front();
        }

        std::function<void(DesktopCommandId)> command;
        std::function<void()> callback;
        {
            std::lock_guard lock(m_State->mutex);
            switch (event.type) {
            case DesktopBackendEventType::TrayCommand: {
                const auto found = m_State->trays.find(event.registration);
                if (found != m_State->trays.end()) command = found->second.onCommand;
                if (found != m_State->trays.end()) {
                    MarkHealthSuccessLocked(*m_State,
                        ApplicationCapability::SystemTray, "TrayCommand");
                }
                break;
            }
            case DesktopBackendEventType::TrayDoubleClick: {
                const auto found = m_State->trays.find(event.registration);
                if (found != m_State->trays.end()) callback = found->second.onDoubleClick;
                if (found != m_State->trays.end()) {
                    MarkHealthSuccessLocked(*m_State,
                        ApplicationCapability::SystemTray, "TrayDoubleClick");
                }
                break;
            }
            case DesktopBackendEventType::GlobalShortcutPressed: {
                const auto found = m_State->shortcuts.find(event.registration);
                if (found != m_State->shortcuts.end()) callback = found->second.onPressed;
                if (found != m_State->shortcuts.end()) {
                    MarkHealthSuccessLocked(*m_State,
                        ApplicationCapability::GlobalShortcut,
                        "GlobalShortcutPressed");
                }
                break;
            }
            case DesktopBackendEventType::PrimaryInstanceActivated: {
                const auto found = m_State->instances.find(event.registration);
                if (found != m_State->instances.end()) callback = found->second.onActivate;
                if (found != m_State->instances.end()) {
                    MarkHealthSuccessLocked(*m_State,
                        ApplicationCapability::PrimaryInstanceActivation,
                        "PrimaryInstanceActivated");
                }
                break;
            }
            case DesktopBackendEventType::HealthChanged: {
                ApplyHealthEventLocked(*m_State, event);
                break;
            }
            }
        }
        if (command) command(event.command);
        if (callback) callback();
    }
    return ApplicationResult<void>::Success();
}

ApplicationResult<void> DesktopServices::Shutdown() {
    if (!m_State) return ApplicationResult<void>::Success();
    if (m_State->shuttingDown) return ApplicationResult<void>::Success();
    if (const auto error = CheckOwner(m_State, "ShutdownDesktopServices")) {
        return ApplicationResult<void>::Failure(*error);
    }
    {
        std::lock_guard lock(m_State->mutex);
        m_State->shuttingDown = true;
        m_State->started = false;
        for (const auto capability : {
                 ApplicationCapability::DesktopIntegration,
                 ApplicationCapability::SystemTray,
                 ApplicationCapability::GlobalShortcut,
                 ApplicationCapability::SingleInstance,
                 ApplicationCapability::PrimaryInstanceActivation}) {
            MarkServiceUnavailableLocked(*m_State, capability,
                "service_shutdown");
        }
    }
    if (m_State->backend) m_State->backend->Stop();
    std::lock_guard lock(m_State->mutex);
    m_State->events.clear();
    m_State->trays.clear();
    m_State->shortcuts.clear();
    m_State->instances.clear();
    return ApplicationResult<void>::Success();
}

void DesktopServices::SetOwnerWakeCallback(std::function<void()> callback) {
    if (!m_State) return;
    std::lock_guard lock(m_State->mutex);
    m_State->wakeOwnerThread = std::move(callback);
}

DesktopServices::TrayApi DesktopServices::Tray() noexcept { return TrayApi(this); }
DesktopServices::ShortcutApi DesktopServices::Shortcuts() noexcept {
    return ShortcutApi(this);
}
DesktopServices::InstanceApi DesktopServices::Instances() noexcept {
    return InstanceApi(this);
}

ApplicationResult<TraySession> DesktopServices::TrayApi::Create(
    const TrayOptions& options) {
    if (!m_Owner) {
        return Failure<TraySession>(MakeError(
            ApplicationErrorCode::LifecycleViolation, "CreateTray",
            "system_tray", "desktop service view is empty"));
    }
    const auto valid = ValidateTray(options);
    if (!valid) return Failure<TraySession>(valid.Error());
    const auto state = m_Owner->m_State;
    const auto usable = CheckUsable(state, "CreateTray", "system_tray");
    if (!usable) return Failure<TraySession>(usable.Error());
    if (!state->capabilities.systemTray) {
        return Failure<TraySession>(MakeError(
            ApplicationErrorCode::Unsupported, "CreateTray", "system_tray",
            "desktop backend does not provide a system tray",
            state->capabilities.backend));
    }
    const auto created = state->backend->CreateTray(options);
    if (!created) {
        RecordBackendFailure(state, ApplicationCapability::SystemTray,
            created.Error());
        return Failure<TraySession>(created.Error());
    }
    {
        std::lock_guard lock(state->mutex);
        state->trays.emplace(created.Value(), Detail::TrayRecord{
            options.menu, options.tooltip, options.onCommand,
            options.onDoubleClick});
        MarkHealthSuccessLocked(*state, ApplicationCapability::SystemTray,
            "CreateTray");
    }
    return ApplicationResult<TraySession>::Success(
        TraySession(state, created.Value()));
}

ApplicationResult<ShortcutRegistration> DesktopServices::ShortcutApi::Register(
    const ShortcutOptions& options) {
    if (!m_Owner) {
        return Failure<ShortcutRegistration>(MakeError(
            ApplicationErrorCode::LifecycleViolation, "RegisterGlobalShortcut",
            "global_shortcut", "desktop service view is empty"));
    }
    const auto valid = ValidateShortcut(options);
    if (!valid) return Failure<ShortcutRegistration>(valid.Error());
    const auto state = m_Owner->m_State;
    const auto usable = CheckUsable(state, "RegisterGlobalShortcut",
        "global_shortcut");
    if (!usable) return Failure<ShortcutRegistration>(usable.Error());
    if (!state->capabilities.globalShortcut) {
        return Failure<ShortcutRegistration>(MakeError(
            ApplicationErrorCode::Unsupported, "RegisterGlobalShortcut",
            "global_shortcut", "desktop backend does not provide global shortcuts",
            state->capabilities.backend));
    }
    const auto registered = state->backend->RegisterGlobalShortcut(options);
    if (!registered) {
        RecordBackendFailure(state, ApplicationCapability::GlobalShortcut,
            registered.Error());
        return Failure<ShortcutRegistration>(registered.Error());
    }
    {
        std::lock_guard lock(state->mutex);
        state->shortcuts.emplace(registered.Value(),
            Detail::ShortcutRecord{options.onPressed});
        MarkHealthSuccessLocked(*state, ApplicationCapability::GlobalShortcut,
            "RegisterGlobalShortcut");
    }
    return ApplicationResult<ShortcutRegistration>::Success(
        ShortcutRegistration(state, registered.Value()));
}

ApplicationResult<SingleInstanceAcquireResult>
DesktopServices::InstanceApi::Acquire(const SingleInstanceOptions& options) {
    if (!m_Owner) {
        return Failure<SingleInstanceAcquireResult>(MakeError(
            ApplicationErrorCode::LifecycleViolation, "AcquireSingleInstance",
            "single_instance", "desktop service view is empty"));
    }
    const auto valid = ValidateInstance(options);
    if (!valid) return Failure<SingleInstanceAcquireResult>(valid.Error());
    const auto state = m_Owner->m_State;
    const auto usable = CheckUsable(state, "AcquireSingleInstance",
        "single_instance");
    if (!usable) return Failure<SingleInstanceAcquireResult>(usable.Error());
    if (!state->capabilities.singleInstance) {
        return Failure<SingleInstanceAcquireResult>(MakeError(
            ApplicationErrorCode::Unsupported, "AcquireSingleInstance",
            "single_instance", "desktop backend does not provide single instance",
            state->capabilities.backend));
    }
    {
        std::lock_guard lock(state->mutex);
        if (!state->instances.empty()) {
            return Failure<SingleInstanceAcquireResult>(MakeError(
                ApplicationErrorCode::AlreadyExists, "AcquireSingleInstance",
                "single_instance", "a single-instance session is already active",
                state->capabilities.backend));
        }
    }
    auto backendResult = state->backend->AcquireSingleInstance(options);
    if (!backendResult) {
        RecordBackendFailure(state, ApplicationCapability::SingleInstance,
            backendResult.Error());
        return Failure<SingleInstanceAcquireResult>(backendResult.Error());
    }
    auto acquired = std::move(backendResult.Value());
    if (acquired.status != SingleInstanceAcquireStatus::PrimaryAcquired) {
        std::lock_guard lock(state->mutex);
        if (acquired.status == SingleInstanceAcquireStatus::ExistingInstanceActivated) {
            MarkHealthSuccessLocked(*state, ApplicationCapability::SingleInstance,
                "AcquireSingleInstance");
            MarkHealthSuccessLocked(*state,
                ApplicationCapability::PrimaryInstanceActivation,
                "ActivatePrimary");
        } else if (acquired.status ==
            SingleInstanceAcquireStatus::AlreadyRunningButUnreachable) {
            MarkHealthSuccessLocked(*state, ApplicationCapability::SingleInstance,
                "AcquireSingleInstance");
            MarkHealthFailureLocked(*state,
                ApplicationCapability::PrimaryInstanceActivation,
                CapabilityStatus::Degraded, "ActivatePrimary",
                "activation_endpoint_unreachable", std::nullopt, false);
        } else if (acquired.status == SingleInstanceAcquireStatus::BackendFailure) {
            MarkHealthFailureLocked(*state, ApplicationCapability::SingleInstance,
                CapabilityStatus::Degraded, "AcquireSingleInstance",
                "backend_reported_failure", std::nullopt, false);
            MarkHealthFailureLocked(*state,
                ApplicationCapability::PrimaryInstanceActivation,
                CapabilityStatus::Degraded, "ActivatePrimary",
                "backend_reported_failure", std::nullopt, false);
        }
        return ApplicationResult<SingleInstanceAcquireResult>::Success(
            std::move(acquired));
    }
    if (acquired.registration == 0) {
        acquired.status = SingleInstanceAcquireStatus::BackendFailure;
        acquired.message = "desktop backend returned no primary registration";
        std::lock_guard lock(state->mutex);
        MarkHealthFailureLocked(*state, ApplicationCapability::SingleInstance,
            CapabilityStatus::Degraded, "AcquireSingleInstance",
            "backend_missing_primary_registration", std::nullopt, false);
        MarkHealthFailureLocked(*state,
            ApplicationCapability::PrimaryInstanceActivation,
            CapabilityStatus::Degraded, "AcquireSingleInstance",
            "backend_missing_primary_registration", std::nullopt, false);
        return ApplicationResult<SingleInstanceAcquireResult>::Success(
            std::move(acquired));
    }
    {
        std::lock_guard lock(state->mutex);
        state->instances.emplace(acquired.registration,
            Detail::InstanceRecord{options.onPrimaryActivate});
        MarkHealthSuccessLocked(*state, ApplicationCapability::SingleInstance,
            "AcquireSingleInstance");
        MarkHealthSuccessLocked(*state,
            ApplicationCapability::PrimaryInstanceActivation,
            "AcquireSingleInstance");
    }
    acquired.primary.emplace(state, acquired.registration);
    return ApplicationResult<SingleInstanceAcquireResult>::Success(
        std::move(acquired));
}

} // namespace Engine
