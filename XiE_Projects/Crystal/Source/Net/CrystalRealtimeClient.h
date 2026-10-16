#pragma once

#include "Core/MainThreadEventQueue.h"
#include "Net/CrystalRealtimeEvent.h"

#include <string>
#include <functional>
#include <vector>

namespace Crystal {

class CrystalRealtimeClient {
public:
    CrystalRealtimeClient() = default;
    ~CrystalRealtimeClient() { Stop(); }

    void Configure(std::string serverBaseUrl, std::string projectId);
    void SetWakeCallback(std::function<void()> callback) {
        m_Events.SetWakeCallback(std::move(callback));
    }
    bool Start();
    void Stop();
    void ForceDisconnectForDebug() noexcept { m_Client.ForceDisconnectForDebug(); }
    void ClearEvents() { m_Events.Clear(); }
    [[nodiscard]] std::vector<CrystalRealtimeEvent> DrainEvents() { return m_Events.Drain(); }
    [[nodiscard]] std::size_t PendingEventCount() const { return m_Events.Size(); }
    [[nodiscard]] Engine::Network::WebSocketState State() const noexcept { return m_Client.State(); }

private:
    void HandleMessage(std::string message);
    void EnqueueState(Engine::Network::WebSocketState state);

    Engine::Network::WebSocketClient m_Client;
    Engine::MainThreadEventQueue<CrystalRealtimeEvent> m_Events;
};

} // namespace Crystal
