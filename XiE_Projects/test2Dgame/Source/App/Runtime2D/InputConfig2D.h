#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace Test2D {

struct InputConfig2D {
    std::optional<std::string> moveLeft;
    std::optional<std::string> moveRight;
    std::optional<std::string> moveUp;
    std::optional<std::string> moveDown;

    std::optional<std::string> moveLeftAlt;
    std::optional<std::string> moveRightAlt;
    std::optional<std::string> moveUpAlt;
    std::optional<std::string> moveDownAlt;

    std::optional<float> moveSpeed;
};

bool LoadInputConfig2D(const std::filesystem::path& filePath, InputConfig2D& outConfig, std::string& outError);

} // namespace Test2D
