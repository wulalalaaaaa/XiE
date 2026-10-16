#include "Net/CrystalRealtimeClient.h"

#include <utility>

namespace Crystal {

void CrystalRealtimeClient::Configure(std::string serverBaseUrl, std::string projectId) {
    // WinHTTP's WebSocket upgrade API expects an HTTP(S) URL.  Keep the
    // transport scheme in the form accepted by WinHttpCrackUrl instead of
    // converting it to ws/wss before the native client parses it.
    if (serverBaseUrl.rfind("wss://", 0) == 0) serverBaseUrl.replace(0, 6, "https://");
    else if (serverBaseUrl.rfind("ws://", 0) == 0) serverBaseUrl.replace(0, 5, "http://");
    m_Client.SetUrl(std::move(serverBaseUrl) + "/ws/projects/" + projectId);
    m_Client.SetMessageHandler([this](std::string message) { HandleMessage(std::move(message)); });
    m_Client.SetStateHandler([this](Engine::Network::WebSocketState state) {
        EnqueueState(state);
    });
}

bool CrystalRealtimeClient::Start() { return m_Client.Start(); }

void CrystalRealtimeClient::Stop() { m_Client.Stop(); }

void CrystalRealtimeClient::EnqueueState(Engine::Network::WebSocketState state) {
    CrystalRealtimeEvent event;
    event.type = CrystalRealtimeEventType::ConnectionState;
    event.wireType = "connection.state";
    event.connectionState = state;
    m_Events.Enqueue(std::move(event));
}

void CrystalRealtimeClient::HandleMessage(std::string message) {
    std::string error;
    const auto parsed = Engine::Json::Value::Parse(message, &error);
    if (!parsed) return;
    const std::string type = parsed->GetString("type");
    if (type == "realtime.ready") return;
    const CrystalRealtimeEventType eventType = RealtimeEventTypeFromWire(type);
    if (eventType == CrystalRealtimeEventType::Unknown) return;
    CrystalRealtimeEvent event;
    event.type = eventType;
    event.wireType = type;
    event.payload = *parsed;
    m_Events.Enqueue(std::move(event));
}

} // namespace Crystal
