#include "UI2D/TextEditing/UITextEditingUtils.h"

namespace Engine::UI2D {
namespace {

bool DecodeOne(
    std::string_view text, std::size_t& offset, char32_t& value) noexcept {
    if (offset >= text.size()) return false;
    const auto lead = static_cast<unsigned char>(text[offset]);
    std::size_t count = 0;
    char32_t scalar = 0;
    if (lead <= 0x7F) {
        count = 1; scalar = lead;
    } else if (lead >= 0xC2 && lead <= 0xDF) {
        count = 2; scalar = lead & 0x1Fu;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        count = 3; scalar = lead & 0x0Fu;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        count = 4; scalar = lead & 0x07u;
    } else {
        return false;
    }
    if (offset + count > text.size()) return false;
    for (std::size_t i = 1; i < count; ++i) {
        const auto unit = static_cast<unsigned char>(text[offset + i]);
        if ((unit & 0xC0u) != 0x80u) return false;
        scalar = (scalar << 6u) | (unit & 0x3Fu);
    }
    if ((count == 2 && scalar < 0x80) ||
        (count == 3 && scalar < 0x800) ||
        (count == 4 && scalar < 0x10000) ||
        scalar > 0x10FFFF ||
        (scalar >= 0xD800 && scalar <= 0xDFFF)) {
        return false;
    }
    offset += count;
    value = scalar;
    return true;
}

} // namespace

bool IsValidUtf8(std::string_view text) noexcept {
    std::size_t offset = 0;
    char32_t scalar = 0;
    while (offset < text.size()) {
        if (!DecodeOne(text, offset, scalar)) return false;
    }
    return true;
}

std::optional<std::u32string> DecodeUtf8(std::string_view text) {
    std::u32string result;
    std::size_t offset = 0;
    char32_t scalar = 0;
    while (offset < text.size()) {
        if (!DecodeOne(text, offset, scalar)) return std::nullopt;
        result.push_back(scalar);
    }
    return result;
}

std::string EncodeUtf8(std::u32string_view text) {
    std::string result;
    for (char32_t scalar : text) {
        if (scalar <= 0x7F) {
            result.push_back(static_cast<char>(scalar));
        } else if (scalar <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0u | (scalar >> 6u)));
            result.push_back(static_cast<char>(0x80u | (scalar & 0x3Fu)));
        } else if (scalar <= 0xFFFF &&
            !(scalar >= 0xD800 && scalar <= 0xDFFF)) {
            result.push_back(static_cast<char>(0xE0u | (scalar >> 12u)));
            result.push_back(static_cast<char>(
                0x80u | ((scalar >> 6u) & 0x3Fu)));
            result.push_back(static_cast<char>(0x80u | (scalar & 0x3Fu)));
        } else if (scalar <= 0x10FFFF) {
            result.push_back(static_cast<char>(0xF0u | (scalar >> 18u)));
            result.push_back(static_cast<char>(
                0x80u | ((scalar >> 12u) & 0x3Fu)));
            result.push_back(static_cast<char>(
                0x80u | ((scalar >> 6u) & 0x3Fu)));
            result.push_back(static_cast<char>(0x80u | (scalar & 0x3Fu)));
        }
    }
    return result;
}

std::size_t Utf8ScalarCount(std::string_view text) noexcept {
    std::size_t offset = 0;
    std::size_t count = 0;
    char32_t scalar = 0;
    while (offset < text.size()) {
        if (!DecodeOne(text, offset, scalar)) return 0;
        ++count;
    }
    return count;
}

std::optional<std::size_t> Utf8ByteOffset(
    std::string_view text, std::size_t scalarIndex) noexcept {
    std::size_t offset = 0;
    std::size_t index = 0;
    char32_t scalar = 0;
    while (index < scalarIndex && offset < text.size()) {
        if (!DecodeOne(text, offset, scalar)) return std::nullopt;
        ++index;
    }
    if (index != scalarIndex) return std::nullopt;
    return offset;
}

} // namespace Engine::UI2D
