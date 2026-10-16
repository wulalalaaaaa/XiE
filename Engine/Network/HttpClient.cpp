#include "Network/HttpClient.h"

#include <algorithm>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winhttp.h>
#endif

namespace Engine::Network {
namespace {

std::string JoinUrl(std::string base, std::string_view path) {
    if (base.empty()) return std::string(path);
    while (!base.empty() && base.back() == '/') base.pop_back();
    if (path.empty()) return base;
    if (path.front() == '/') return base + std::string(path);
    return base + '/' + std::string(path);
}

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

std::string Win32Error(DWORD code) {
    char buffer[256]{};
    const DWORD written = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, buffer, static_cast<DWORD>(sizeof(buffer)), nullptr);
    if (written == 0) return "WinHTTP error " + std::to_string(code);
    std::string result(buffer, buffer + written);
    while (!result.empty() && (result.back() == '\r' || result.back() == '\n')) result.pop_back();
    return result;
}
#endif

} // namespace

HttpClient::HttpClient(std::string baseUrl) : m_BaseUrl(std::move(baseUrl)) {}

void HttpClient::SetBaseUrl(std::string baseUrl) { m_BaseUrl = std::move(baseUrl); }

HttpResponse HttpClient::Get(std::string_view path) const { return Request("GET", path); }

HttpResponse HttpClient::Post(std::string_view path, const Engine::Json::Value& body) const {
    return Request("POST", path, body.Dump());
}

HttpResponse HttpClient::Put(std::string_view path, const Engine::Json::Value& body) const {
    return Request("PUT", path, body.Dump());
}

HttpResponse HttpClient::Patch(std::string_view path, const Engine::Json::Value& body) const {
    return Request("PATCH", path, body.Dump());
}

HttpResponse HttpClient::Delete(std::string_view path) const { return Request("DELETE", path); }

HttpResponse HttpClient::Delete(
    std::string_view path, const Engine::Json::Value& body) const {
    return Request("DELETE", path, body.Dump());
}

HttpResponse HttpClient::Request(
    std::string_view method, std::string_view path, std::string_view body) const {
    const std::string url = JoinUrl(m_BaseUrl, path);
#ifdef _WIN32
    const std::wstring wideUrl = Utf8ToWide(url);
    const std::wstring wideMethod = Utf8ToWide(method);
    if (wideUrl.empty() || wideMethod.empty()) return {0, {}, "invalid UTF-8 URL or method"};

    URL_COMPONENTSW components{};
    components.dwStructSize = sizeof(components);
    wchar_t host[256]{};
    wchar_t urlPath[4096]{};
    wchar_t extraInfo[2048]{};
    components.lpszHostName = host;
    components.dwHostNameLength = static_cast<DWORD>(std::size(host));
    components.lpszUrlPath = urlPath;
    components.dwUrlPathLength = static_cast<DWORD>(std::size(urlPath));
    components.lpszExtraInfo = extraInfo;
    components.dwExtraInfoLength = static_cast<DWORD>(std::size(extraInfo));
    if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &components)) {
        return {0, {}, Win32Error(GetLastError())};
    }

    const HINTERNET session = WinHttpOpen(
        L"XiE-Crystal/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return {0, {}, Win32Error(GetLastError())};
    WinHttpSetTimeouts(session, 5000, 5000, 10000, 10000);

    const HINTERNET connection = WinHttpConnect(
        session, components.lpszHostName, components.nPort, 0);
    if (!connection) {
        const std::string error = Win32Error(GetLastError());
        WinHttpCloseHandle(session);
        return {0, {}, error};
    }
    const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    std::wstring requestPath(components.lpszUrlPath, components.dwUrlPathLength);
    requestPath.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    if (requestPath.empty()) requestPath = L"/";
    const HINTERNET request = WinHttpOpenRequest(
        connection, wideMethod.c_str(), requestPath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        const std::string error = Win32Error(GetLastError());
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return {0, {}, error};
    }

    const wchar_t* headers = body.empty()
        ? L"Accept: application/json\r\n"
        : L"Accept: application/json\r\nContent-Type: application/json\r\n";
    const BOOL sent = WinHttpSendRequest(
        request, headers, static_cast<DWORD>(-1L),
        body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
        body.empty() ? 0 : static_cast<DWORD>(body.size()),
        body.empty() ? 0 : static_cast<DWORD>(body.size()), 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
        const std::string error = Win32Error(GetLastError());
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return {0, {}, error};
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    std::string responseBody;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
        const std::size_t oldSize = responseBody.size();
        responseBody.resize(oldSize + available);
        DWORD read = 0;
        if (!WinHttpReadData(request, responseBody.data() + oldSize, available, &read)) {
            responseBody.resize(oldSize);
            break;
        }
        responseBody.resize(oldSize + read);
        if (read == 0) break;
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return {static_cast<int>(statusCode), std::move(responseBody), {}};
#else
    (void)method;
    (void)body;
    return {0, {}, "HttpClient is only implemented for Windows in this MVP: " + url};
#endif
}

} // namespace Engine::Network
