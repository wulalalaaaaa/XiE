#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::UI2D {

[[nodiscard]] bool IsValidUtf8(std::string_view text) noexcept;
[[nodiscard]] std::optional<std::u32string> DecodeUtf8(
    std::string_view text);
[[nodiscard]] std::string EncodeUtf8(std::u32string_view text);
[[nodiscard]] std::size_t Utf8ScalarCount(std::string_view text) noexcept;
[[nodiscard]] std::optional<std::size_t> Utf8ByteOffset(
    std::string_view text, std::size_t scalarIndex) noexcept;

} // namespace Engine::UI2D
