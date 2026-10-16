#pragma once

#include "Network/Json.h"

#include <string>
#include <string_view>

namespace Engine::Network {

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::string error;

    [[nodiscard]] bool IsSuccess() const noexcept {
        return statusCode >= 200 && statusCode < 300 && error.empty();
    }
};

class HttpClient {
public:
    explicit HttpClient(std::string baseUrl = {});

    void SetBaseUrl(std::string baseUrl);
    [[nodiscard]] const std::string& BaseUrl() const noexcept { return m_BaseUrl; }

    [[nodiscard]] HttpResponse Get(std::string_view path) const;
    [[nodiscard]] HttpResponse Post(
        std::string_view path, const Engine::Json::Value& body) const;
    [[nodiscard]] HttpResponse Put(
        std::string_view path, const Engine::Json::Value& body) const;
    [[nodiscard]] HttpResponse Patch(
        std::string_view path, const Engine::Json::Value& body) const;
    [[nodiscard]] HttpResponse Delete(std::string_view path) const;
    [[nodiscard]] HttpResponse Delete(
        std::string_view path, const Engine::Json::Value& body) const;

private:
    [[nodiscard]] HttpResponse Request(
        std::string_view method,
        std::string_view path,
        std::string_view body = {}) const;

    std::string m_BaseUrl;
};

} // namespace Engine::Network
