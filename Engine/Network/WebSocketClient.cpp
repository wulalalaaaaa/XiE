#include "Network/WebSocketClient.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>
#endif

namespace Engine::Network {
namespace {

#ifdef _WIN32
std::wstring Utf8ToWide(std::string_view value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), result.data(), size) <= 0) return {};
    return result;
}

bool CrackWebSocketUrl(
    std::string_view url, std::wstring& host, std::wstring& path,
    INTERNET_PORT& port, DWORD& flags) {
    const std::wstring wideUrl = Utf8ToWide(url);
    if (wideUrl.empty()) return false;
    URL_COMPONENTSW components{};
    components.dwStructSize = sizeof(components);
    wchar_t hostBuffer[256]{};
    wchar_t pathBuffer[4096]{};
    wchar_t extraBuffer[2048]{};
    components.lpszHostName = hostBuffer;
    components.dwHostNameLength = static_cast<DWORD>(std::size(hostBuffer));
    components.lpszUrlPath = pathBuffer;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(pathBuffer));
    components.lpszExtraInfo = extraBuffer;
    components.dwExtraInfoLength = static_cast<DWORD>(std::size(extraBuffer));
    if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &components))
        return false;
    if (components.nScheme != INTERNET_SCHEME_HTTP &&
        components.nScheme != INTERNET_SCHEME_HTTPS) return false;
    host.assign(components.lpszHostName, components.dwHostNameLength);
    path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    if (path.empty()) path = L"/";
    port = components.nPort;
    flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    return true;
}
#endif

} // namespace

WebSocketClient::WebSocketClient(std::string url) : m_Url(std::move(url)) {}

WebSocketClient::~WebSocketClient() { Stop(); }

void WebSocketClient::SetUrl(std::string url) {
    std::scoped_lock lock(m_Mutex);
    m_Url = std::move(url);
}

void WebSocketClient::SetMessageHandler(MessageHandler handler) {
    std::scoped_lock lock(m_Mutex);
    m_MessageHandler = std::move(handler);
}

void WebSocketClient::SetStateHandler(StateHandler handler) {
    std::scoped_lock lock(m_Mutex);
    m_StateHandler = std::move(handler);
}

void WebSocketClient::SetReconnectDelayMs(int milliseconds) noexcept {
    m_ReconnectDelayMs = std::max(100, milliseconds);
}

void WebSocketClient::SetState(WebSocketState state) {
    m_State.store(state);
    StateHandler handler;
    {
        std::scoped_lock lock(m_Mutex);
        handler = m_StateHandler;
    }
    if (handler) handler(state);
}

bool WebSocketClient::Start() {
    if (m_Worker.joinable()) return false;
    m_StopRequested.store(false);
    m_ForcedDisconnectRequested.store(false);
    m_Worker = std::thread(&WebSocketClient::Run, this);
    return true;
}

void WebSocketClient::Stop() {
    m_StopRequested.store(true);
    SetState(WebSocketState::Stopping);
#ifdef _WIN32
    {
        std::scoped_lock lock(m_Mutex);
        if (m_Socket != nullptr) {
            WinHttpWebSocketClose(static_cast<HINTERNET>(m_Socket),
                WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
        }
    }
#endif
    if (m_Worker.joinable()) m_Worker.join();
    m_ForcedDisconnectRequested.store(false);
    SetState(WebSocketState::Disconnected);
}

void WebSocketClient::ForceDisconnectForDebug() noexcept {
#ifdef _WIN32
    std::scoped_lock lock(m_Mutex);
    if (m_Socket != nullptr) {
        m_ForcedDisconnectRequested.store(true);
        WinHttpWebSocketClose(static_cast<HINTERNET>(m_Socket),
            WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
    }
#endif
}

bool WebSocketClient::SendText(std::string_view text) {
#ifdef _WIN32
    std::scoped_lock lock(m_Mutex);
    if (m_Socket == nullptr || m_State.load() != WebSocketState::Connected) return false;
    const HINTERNET socket = static_cast<HINTERNET>(m_Socket);
    return WinHttpWebSocketSend(socket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
        const_cast<char*>(text.data()), static_cast<DWORD>(text.size())) == ERROR_SUCCESS;
#else
    (void)text;
    return false;
#endif
}

void WebSocketClient::Run() {
#ifdef _WIN32
    bool connectedBefore = false;
    while (!m_StopRequested.load()) {
        SetState(connectedBefore ? WebSocketState::Reconnecting : WebSocketState::Connecting);
        std::wstring host;
        std::wstring path;
        INTERNET_PORT port = 0;
        DWORD flags = 0;
        std::string url;
        {
            std::scoped_lock lock(m_Mutex);
            url = m_Url;
        }
        if (!CrackWebSocketUrl(url, host, path, port, flags)) {
            SetState(WebSocketState::Disconnected);
        } else {
            const DWORD accessType = host == L"127.0.0.1" || host == L"localhost" || host == L"::1"
                ? WINHTTP_ACCESS_TYPE_NO_PROXY : WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
            HINTERNET session = WinHttpOpen(L"XiE-Crystal/1.0", accessType,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, 0);
            HINTERNET connection = session != nullptr
                ? WinHttpConnect(session, host.c_str(), port, 0) : nullptr;
            HINTERNET request = connection != nullptr
                ? WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr,
                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags) : nullptr;
            const BOOL websocketOption = request != nullptr && WinHttpSetOption(request,
                WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0);
            HINTERNET socket = nullptr;
            const BOOL sent = websocketOption && WinHttpSendRequest(request,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            const BOOL received = sent && WinHttpReceiveResponse(request, nullptr);
            DWORD statusCode = 0;
            DWORD statusSize = sizeof(statusCode);
            const BOOL statusRead = received && WinHttpQueryHeaders(request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
                WINHTTP_NO_HEADER_INDEX);
            if (statusRead && statusCode == 101) {
                socket = WinHttpWebSocketCompleteUpgrade(request, 0);
            }
            if (request != nullptr) WinHttpCloseHandle(request);
            if (socket != nullptr) {
                connectedBefore = true;
                {
                    std::scoped_lock lock(m_Mutex);
                    m_Session = session;
                    m_Connection = connection;
                    m_Socket = socket;
                }
                SetState(WebSocketState::Connected);
                std::string message;
                std::vector<char> buffer(16 * 1024);
                while (!m_StopRequested.load()) {
                    DWORD read = 0;
                    WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};
                    const DWORD result = WinHttpWebSocketReceive(socket, buffer.data(),
                        static_cast<DWORD>(buffer.size()), &read, &type);
                    if (result != ERROR_SUCCESS || type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE)
                        break;
                    if (type == WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE ||
                        type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                        message.append(buffer.data(), buffer.data() + read);
                        if (type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                            MessageHandler handler;
                            {
                                std::scoped_lock lock(m_Mutex);
                                handler = m_MessageHandler;
                            }
                            if (handler) handler(std::move(message));
                            message.clear();
                        }
                    }
                }
                if (!m_ForcedDisconnectRequested.exchange(false))
                    WinHttpWebSocketClose(socket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS,
                        nullptr, 0);
                WinHttpCloseHandle(socket);
                if (connection != nullptr) WinHttpCloseHandle(connection);
                if (session != nullptr) WinHttpCloseHandle(session);
                {
                    std::scoped_lock lock(m_Mutex);
                    m_Session = nullptr;
                    m_Connection = nullptr;
                    m_Socket = nullptr;
                }
            } else {
                if (connection != nullptr) WinHttpCloseHandle(connection);
                if (session != nullptr) WinHttpCloseHandle(session);
            }
            if (!m_StopRequested.load())
                SetState(connectedBefore ? WebSocketState::Reconnecting : WebSocketState::Disconnected);
        }
        for (int elapsed = 0; elapsed < m_ReconnectDelayMs && !m_StopRequested.load(); elapsed += 50)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
#else
    SetState(WebSocketState::Disconnected);
#endif
}

} // namespace Engine::Network
