#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Engine::Json {

class Value {
public:
    using Object = std::map<std::string, Value, std::less<>>;
    using Array = std::vector<Value>;

    Value() = default;
    Value(std::nullptr_t) noexcept {}
    Value(bool value) : m_Data(value) {}
    Value(int value) : m_Data(static_cast<double>(value)) {}
    Value(std::int64_t value) : m_Data(static_cast<double>(value)) {}
    Value(double value) : m_Data(value) {}
    Value(const char* value) : m_Data(std::string(value != nullptr ? value : "")) {}
    Value(std::string value) : m_Data(std::move(value)) {}
    Value(Object value) : m_Data(std::move(value)) {}
    Value(Array value) : m_Data(std::move(value)) {}

    [[nodiscard]] bool IsNull() const noexcept;
    [[nodiscard]] bool IsBool() const noexcept;
    [[nodiscard]] bool IsNumber() const noexcept;
    [[nodiscard]] bool IsString() const noexcept;
    [[nodiscard]] bool IsObject() const noexcept;
    [[nodiscard]] bool IsArray() const noexcept;

    [[nodiscard]] bool AsBool(bool fallback = false) const noexcept;
    [[nodiscard]] double AsNumber(double fallback = 0.0) const noexcept;
    [[nodiscard]] const std::string& AsString() const noexcept;
    [[nodiscard]] const Object& AsObject() const noexcept;
    [[nodiscard]] const Array& AsArray() const noexcept;
    [[nodiscard]] Object& AsObject();
    [[nodiscard]] Array& AsArray();

    [[nodiscard]] const Value* Find(std::string_view key) const noexcept;
    [[nodiscard]] Value* Find(std::string_view key) noexcept;
    [[nodiscard]] std::string GetString(
        std::string_view key, std::string fallback = {}) const;
    [[nodiscard]] double GetNumber(std::string_view key, double fallback = 0.0) const noexcept;
    [[nodiscard]] bool GetBool(std::string_view key, bool fallback = false) const noexcept;

    Value& operator[](std::string key);
    [[nodiscard]] const Value& operator[](std::string_view key) const noexcept;

    [[nodiscard]] std::string Dump() const;
    [[nodiscard]] static std::optional<Value> Parse(
        std::string_view input, std::string* error = nullptr);

private:
    using Storage = std::variant<std::monostate, bool, double, std::string, Object, Array>;
    Storage m_Data;
};

} // namespace Engine::Json
