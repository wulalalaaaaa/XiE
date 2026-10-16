#include "Network/Json.h"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace Engine::Json {
namespace {

const std::string kEmptyString;
const Value::Object kEmptyObject;
const Value::Array kEmptyArray;

void AppendEscaped(std::string& output, std::string_view value) {
    output.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (character < 0x20) {
                char buffer[7]{};
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", character);
                output += buffer;
            } else {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    output.push_back('"');
}

class Parser {
public:
    explicit Parser(std::string_view input) : m_Input(input) {}

    std::optional<Value> Parse(std::string* error) {
        SkipWhitespace();
        auto value = ParseValue(error);
        if (!value) return std::nullopt;
        SkipWhitespace();
        if (m_Position != m_Input.size()) {
            SetError(error, "unexpected trailing characters");
            return std::nullopt;
        }
        return value;
    }

private:
    void SkipWhitespace() noexcept {
        while (m_Position < m_Input.size()) {
            const char c = m_Input[m_Position];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
            ++m_Position;
        }
    }

    void SetError(std::string* error, std::string message) const {
        if (error != nullptr) *error = std::move(message);
    }

    std::optional<Value> ParseValue(std::string* error) {
        SkipWhitespace();
        if (m_Position >= m_Input.size()) {
            SetError(error, "unexpected end of input");
            return std::nullopt;
        }
        switch (m_Input[m_Position]) {
        case 'n': return ParseLiteral("null", Value{}, error);
        case 't': return ParseLiteral("true", Value(true), error);
        case 'f': return ParseLiteral("false", Value(false), error);
        case '"': {
            auto value = ParseString(error);
            if (!value) return std::nullopt;
            return Value(std::move(*value));
        }
        case '{': return ParseObject(error);
        case '[': return ParseArray(error);
        default: return ParseNumber(error);
        }
    }

    std::optional<Value> ParseLiteral(
        std::string_view literal, Value value, std::string* error) {
        if (m_Input.substr(m_Position, literal.size()) != literal) {
            SetError(error, "invalid literal");
            return std::nullopt;
        }
        m_Position += literal.size();
        return value;
    }

    std::optional<std::string> ParseString(std::string* error) {
        if (m_Input[m_Position] != '"') {
            SetError(error, "expected string");
            return std::nullopt;
        }
        ++m_Position;
        std::string result;
        while (m_Position < m_Input.size()) {
            const unsigned char character = static_cast<unsigned char>(m_Input[m_Position++]);
            if (character == '"') return result;
            if (character < 0x20) {
                SetError(error, "control character in string");
                return std::nullopt;
            }
            if (character != '\\') {
                result.push_back(static_cast<char>(character));
                continue;
            }
            if (m_Position >= m_Input.size()) break;
            const char escaped = m_Input[m_Position++];
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': {
                if (m_Position + 4 > m_Input.size()) {
                    SetError(error, "short unicode escape");
                    return std::nullopt;
                }
                unsigned int codepoint = 0;
                for (int i = 0; i < 4; ++i) {
                    const char hex = m_Input[m_Position++];
                    codepoint <<= 4;
                    if (hex >= '0' && hex <= '9') codepoint += static_cast<unsigned>(hex - '0');
                    else if (hex >= 'a' && hex <= 'f') codepoint += static_cast<unsigned>(hex - 'a' + 10);
                    else if (hex >= 'A' && hex <= 'F') codepoint += static_cast<unsigned>(hex - 'A' + 10);
                    else {
                        SetError(error, "invalid unicode escape");
                        return std::nullopt;
                    }
                }
                AppendUtf8(result, codepoint);
                break;
            }
            default:
                SetError(error, "invalid string escape");
                return std::nullopt;
            }
        }
        SetError(error, "unterminated string");
        return std::nullopt;
    }

    static void AppendUtf8(std::string& output, unsigned int codepoint) {
        if (codepoint <= 0x7F) output.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0x10FFFF) {
            output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    std::optional<Value> ParseObject(std::string* error) {
        ++m_Position;
        Value::Object object;
        SkipWhitespace();
        if (m_Position < m_Input.size() && m_Input[m_Position] == '}') {
            ++m_Position;
            return Value(std::move(object));
        }
        while (m_Position < m_Input.size()) {
            SkipWhitespace();
            auto key = ParseString(error);
            if (!key) return std::nullopt;
            SkipWhitespace();
            if (m_Position >= m_Input.size() || m_Input[m_Position] != ':') {
                SetError(error, "expected object colon");
                return std::nullopt;
            }
            ++m_Position;
            auto value = ParseValue(error);
            if (!value) return std::nullopt;
            object[std::move(*key)] = std::move(*value);
            SkipWhitespace();
            if (m_Position < m_Input.size() && m_Input[m_Position] == '}') {
                ++m_Position;
                return Value(std::move(object));
            }
            if (m_Position >= m_Input.size() || m_Input[m_Position] != ',') {
                SetError(error, "expected object comma");
                return std::nullopt;
            }
            ++m_Position;
        }
        SetError(error, "unterminated object");
        return std::nullopt;
    }

    std::optional<Value> ParseArray(std::string* error) {
        ++m_Position;
        Value::Array array;
        SkipWhitespace();
        if (m_Position < m_Input.size() && m_Input[m_Position] == ']') {
            ++m_Position;
            return Value(std::move(array));
        }
        while (m_Position < m_Input.size()) {
            auto value = ParseValue(error);
            if (!value) return std::nullopt;
            array.push_back(std::move(*value));
            SkipWhitespace();
            if (m_Position < m_Input.size() && m_Input[m_Position] == ']') {
                ++m_Position;
                return Value(std::move(array));
            }
            if (m_Position >= m_Input.size() || m_Input[m_Position] != ',') {
                SetError(error, "expected array comma");
                return std::nullopt;
            }
            ++m_Position;
        }
        SetError(error, "unterminated array");
        return std::nullopt;
    }

    std::optional<Value> ParseNumber(std::string* error) {
        const std::size_t start = m_Position;
        if (m_Input[m_Position] == '-') ++m_Position;
        if (m_Position >= m_Input.size()) {
            SetError(error, "invalid number");
            return std::nullopt;
        }
        if (m_Input[m_Position] == '0') ++m_Position;
        else if (m_Input[m_Position] >= '1' && m_Input[m_Position] <= '9') {
            while (m_Position < m_Input.size() &&
                   m_Input[m_Position] >= '0' && m_Input[m_Position] <= '9') ++m_Position;
        } else {
            SetError(error, "invalid number");
            return std::nullopt;
        }
        if (m_Position < m_Input.size() && m_Input[m_Position] == '.') {
            ++m_Position;
            const std::size_t fractionStart = m_Position;
            while (m_Position < m_Input.size() &&
                   m_Input[m_Position] >= '0' && m_Input[m_Position] <= '9') ++m_Position;
            if (fractionStart == m_Position) {
                SetError(error, "invalid number fraction");
                return std::nullopt;
            }
        }
        if (m_Position < m_Input.size() && (m_Input[m_Position] == 'e' || m_Input[m_Position] == 'E')) {
            ++m_Position;
            if (m_Position < m_Input.size() &&
                (m_Input[m_Position] == '+' || m_Input[m_Position] == '-')) ++m_Position;
            const std::size_t exponentStart = m_Position;
            while (m_Position < m_Input.size() &&
                   m_Input[m_Position] >= '0' && m_Input[m_Position] <= '9') ++m_Position;
            if (exponentStart == m_Position) {
                SetError(error, "invalid number exponent");
                return std::nullopt;
            }
        }
        try {
            const double number = std::stod(std::string(m_Input.substr(start, m_Position - start)));
            if (!std::isfinite(number)) throw std::out_of_range("non-finite number");
            return Value(number);
        } catch (const std::exception&) {
            SetError(error, "number is out of range");
            return std::nullopt;
        }
    }

    std::string_view m_Input;
    std::size_t m_Position = 0;
};

void DumpValue(const Value& value, std::string& output) {
    if (value.IsNull()) { output += "null"; return; }
    if (value.IsBool()) { output += value.AsBool() ? "true" : "false"; return; }
    if (value.IsNumber()) {
        std::ostringstream stream;
        stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value.AsNumber();
        output += stream.str();
        return;
    }
    if (value.IsString()) { AppendEscaped(output, value.AsString()); return; }
    if (value.IsArray()) {
        output.push_back('[');
        bool first = true;
        for (const Value& item : value.AsArray()) {
            if (!first) output.push_back(',');
            first = false;
            DumpValue(item, output);
        }
        output.push_back(']');
        return;
    }
    output.push_back('{');
    bool first = true;
    for (const auto& [key, item] : value.AsObject()) {
        if (!first) output.push_back(',');
        first = false;
        AppendEscaped(output, key);
        output.push_back(':');
        DumpValue(item, output);
    }
    output.push_back('}');
}

} // namespace

bool Value::IsNull() const noexcept { return std::holds_alternative<std::monostate>(m_Data); }
bool Value::IsBool() const noexcept { return std::holds_alternative<bool>(m_Data); }
bool Value::IsNumber() const noexcept { return std::holds_alternative<double>(m_Data); }
bool Value::IsString() const noexcept { return std::holds_alternative<std::string>(m_Data); }
bool Value::IsObject() const noexcept { return std::holds_alternative<Object>(m_Data); }
bool Value::IsArray() const noexcept { return std::holds_alternative<Array>(m_Data); }

bool Value::AsBool(bool fallback) const noexcept {
    return IsBool() ? std::get<bool>(m_Data) : fallback;
}
double Value::AsNumber(double fallback) const noexcept {
    return IsNumber() ? std::get<double>(m_Data) : fallback;
}
const std::string& Value::AsString() const noexcept {
    return IsString() ? std::get<std::string>(m_Data) : kEmptyString;
}
const Value::Object& Value::AsObject() const noexcept {
    return IsObject() ? std::get<Object>(m_Data) : kEmptyObject;
}
const Value::Array& Value::AsArray() const noexcept {
    return IsArray() ? std::get<Array>(m_Data) : kEmptyArray;
}
Value::Object& Value::AsObject() {
    if (!IsObject()) m_Data = Object{};
    return std::get<Object>(m_Data);
}
Value::Array& Value::AsArray() {
    if (!IsArray()) m_Data = Array{};
    return std::get<Array>(m_Data);
}

const Value* Value::Find(std::string_view key) const noexcept {
    if (!IsObject()) return nullptr;
    const auto found = std::get<Object>(m_Data).find(key);
    return found == std::get<Object>(m_Data).end() ? nullptr : &found->second;
}
Value* Value::Find(std::string_view key) noexcept {
    if (!IsObject()) return nullptr;
    auto& object = std::get<Object>(m_Data);
    const auto found = object.find(key);
    return found == object.end() ? nullptr : &found->second;
}
std::string Value::GetString(std::string_view key, std::string fallback) const {
    const Value* value = Find(key);
    return value != nullptr && value->IsString() ? value->AsString() : std::move(fallback);
}
double Value::GetNumber(std::string_view key, double fallback) const noexcept {
    const Value* value = Find(key);
    return value != nullptr ? value->AsNumber(fallback) : fallback;
}
bool Value::GetBool(std::string_view key, bool fallback) const noexcept {
    const Value* value = Find(key);
    return value != nullptr ? value->AsBool(fallback) : fallback;
}

Value& Value::operator[](std::string key) { return AsObject()[std::move(key)]; }
const Value& Value::operator[](std::string_view key) const noexcept {
    if (const Value* value = Find(key)) return *value;
    static const Value empty;
    return empty;
}

std::string Value::Dump() const {
    std::string output;
    DumpValue(*this, output);
    return output;
}

std::optional<Value> Value::Parse(std::string_view input, std::string* error) {
    return Parser(input).Parse(error);
}

} // namespace Engine::Json
