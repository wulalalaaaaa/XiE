#include "InputSystem2D.h"

#include "InputConfig2D.h"
#include "Core/Log.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>

namespace Test2D {

namespace {

std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

int KeyNameToGlfwKey(const std::string& keyName) {
    const std::string normalized = ToLower(keyName);
    if (normalized.size() == 1 && normalized[0] >= 'a' && normalized[0] <= 'z') {
        return GLFW_KEY_A + static_cast<int>(normalized[0] - 'a');
    }

    constexpr std::array<std::pair<const char*, int>, 5> keyMap = {{
        {"left", GLFW_KEY_LEFT},
        {"right", GLFW_KEY_RIGHT},
        {"up", GLFW_KEY_UP},
        {"down", GLFW_KEY_DOWN},
        {"space", GLFW_KEY_SPACE},
    }};
    for (const auto& [name, keyCode] : keyMap) {
        if (normalized == name) {
            return keyCode;
        }
    }

    return GLFW_KEY_UNKNOWN;
}

bool IsPressed(GLFWwindow* window, int primaryKey, int altKey) {
    if (primaryKey != GLFW_KEY_UNKNOWN && glfwGetKey(window, primaryKey) == GLFW_PRESS) {
        return true;
    }
    if (altKey != GLFW_KEY_UNKNOWN && glfwGetKey(window, altKey) == GLFW_PRESS) {
        return true;
    }
    return false;
}

void LogInvalidBinding(const char* configKey, const std::string& value) {
    const std::string message = std::string("Input config ignored invalid key for ") + configKey + ": " + value;
    XLOG_WARN(message.c_str());
}

} // namespace

void InputSystem2D::Tick(GLFWwindow* window, World2D& world) const {
    if (window == nullptr) {
        return;
    }

    Entity2D* player = world.FindEntity(world.GetPlayer());
    if (player == nullptr) {
        return;
    }

    float axisX = 0.0f;
    float axisY = 0.0f;

    if (IsPressed(window, m_MoveLeftKey, m_MoveLeftAltKey)) {
        axisX -= 1.0f;
    }
    if (IsPressed(window, m_MoveRightKey, m_MoveRightAltKey)) {
        axisX += 1.0f;
    }
    if (IsPressed(window, m_MoveUpKey, m_MoveUpAltKey)) {
        axisY += 1.0f;
    }
    if (IsPressed(window, m_MoveDownKey, m_MoveDownAltKey)) {
        axisY -= 1.0f;
    }

    if (axisX != 0.0f || axisY != 0.0f) {
        const float length = std::sqrt(axisX * axisX + axisY * axisY);
        if (length > 0.0f) {
            axisX /= length;
            axisY /= length;
        }
    }

    player->velocity.linear.x = axisX * m_MoveSpeed;
    player->velocity.linear.y = axisY * m_MoveSpeed;
}

bool InputSystem2D::LoadConfig(const std::filesystem::path& configPath) {
    InputConfig2D config{};
    std::string error;
    if (!LoadInputConfig2D(configPath, config, error)) {
        const std::string message = "Input config load failed, using defaults: " + configPath.generic_string() +
            " (" + error + ")";
        XLOG_WARN(message.c_str());
        return false;
    }

    auto applyKey = [&](const std::optional<std::string>& keyName, const char* configKey, int& destination) {
        if (!keyName.has_value()) {
            return;
        }
        const int parsedKey = KeyNameToGlfwKey(*keyName);
        if (parsedKey == GLFW_KEY_UNKNOWN) {
            LogInvalidBinding(configKey, *keyName);
            return;
        }
        destination = parsedKey;
    };

    applyKey(config.moveLeft, "move_left", m_MoveLeftKey);
    applyKey(config.moveRight, "move_right", m_MoveRightKey);
    applyKey(config.moveUp, "move_up", m_MoveUpKey);
    applyKey(config.moveDown, "move_down", m_MoveDownKey);
    applyKey(config.moveLeftAlt, "move_left_alt", m_MoveLeftAltKey);
    applyKey(config.moveRightAlt, "move_right_alt", m_MoveRightAltKey);
    applyKey(config.moveUpAlt, "move_up_alt", m_MoveUpAltKey);
    applyKey(config.moveDownAlt, "move_down_alt", m_MoveDownAltKey);

    if (config.moveSpeed.has_value()) {
        SetMoveSpeed(*config.moveSpeed);
    }

    const std::string message = "Input config loaded: " + configPath.generic_string();
    XLOG_INFO(message.c_str());
    return true;
}

void InputSystem2D::SetMoveSpeed(float unitsPerSecond) {
    if (unitsPerSecond > 0.0f) {
        m_MoveSpeed = unitsPerSecond;
    }
}

} // namespace Test2D
