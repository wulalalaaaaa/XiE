#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace Engine::Network {

enum class WebSocketState { Disconnected, Connecting, Connected, Reconnecting, Stopping };

class WebSocketClient {
public:
    using MessageHandler = std::function<void(std::string)>;
    using StateHandler = std::function<void(WebSocketState)>;

    explicit WebSocketClient(std::string url = {});
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    void SetUrl(std::string url);
    void SetMessageHandler(MessageHandler handler);
    void SetStateHandler(StateHandler handler);
    void SetReconnectDelayMs(int milliseconds) noexcept;

    // Start owns a small receive/reconnect loop. Stop is safe to call more
    // than once and joins the worker before returning.
    bool Start();
    void Stop();
    // Development-only fault injection: closes the active socket while
    // leaving the reconnect loop alive. It never changes server data.
    void ForceDisconnectForDebug() noexcept;
    [[nodiscard]] bool SendText(std::string_view text);
    [[nodiscard]] WebSocketState State() const noexcept { return m_State.load(); }

private:
    void Run();
    void SetState(WebSocketState state);

    std::string m_Url;
    MessageHandler m_MessageHandler;
    StateHandler m_StateHandler;
    int m_ReconnectDelayMs = 1000;
    std::atomic<WebSocketState> m_State{WebSocketState::Disconnected};
    std::atomic<bool> m_StopRequested{false};
    std::atomic<bool> m_ForcedDisconnectRequested{false};
    std::thread m_Worker;
    mutable std::mutex m_Mutex;

#ifdef _WIN32
    void* m_Session = nullptr;
    void* m_Connection = nullptr;
    void* m_Request = nullptr;
    void* m_Socket = nullptr;
#endif
};

} // namespace Engine::Network
