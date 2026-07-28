#include "InputSystem2D.h"

#include "InputConfig2D.h"
#include "Core/Log.h"

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

Engine::KeyCode KeyNameToKeyCode(const std::string& keyName) {
    const std::string normalized = ToLower(keyName);
    if (normalized.size() == 1 && normalized[0] >= 'a' && normalized[0] <= 'z') {
        return static_cast<Engine::KeyCode>(static_cast<int>(Engine::KeyCode::A) + static_cast<int>(normalized[0] - 'a'));
    }

    constexpr std::array<std::pair<const char*, Engine::KeyCode>, 5> keyMap = {{
        {"left", Engine::KeyCode::Left},
        {"right", Engine::KeyCode::Right},
        {"up", Engine::KeyCode::Up},
        {"down", Engine::KeyCode::Down},
        {"space", Engine::KeyCode::Space},
    }};
    for (const auto& [name, keyCode] : keyMap) {
        if (normalized == name) {
            return keyCode;
        }
    }

    return Engine::KeyCode::Unknown;
}

bool IsPressed(const Engine::IInputState& input, Engine::KeyCode primaryKey, Engine::KeyCode altKey) {
    if (primaryKey != Engine::KeyCode::Unknown && input.IsKeyDown(primaryKey)) {
        return true;
    }
    if (altKey != Engine::KeyCode::Unknown && input.IsKeyDown(altKey)) {
        return true;
    }
    return false;
}

void LogInvalidBinding(const char* configKey, const std::string& value) {
    const std::string message = std::string("Input config ignored invalid key for ") + configKey + ": " + value;
    XLOG_WARN(message.c_str());
}

} // namespace

void InputSystem2D::Tick(const Engine::IInputState* input, World2D& world) const {
    if (input == nullptr) {
        return;
    }

    Entity2D* player = world.FindEntity(world.GetPlayer());
    if (player == nullptr) {
        return;
    }

    float axisX = 0.0f;
    float axisY = 0.0f;

    if (IsPressed(*input, m_MoveLeftKey, m_MoveLeftAltKey)) {
        axisX -= 1.0f;
    }
    if (IsPressed(*input, m_MoveRightKey, m_MoveRightAltKey)) {
        axisX += 1.0f;
    }
    if (IsPressed(*input, m_MoveUpKey, m_MoveUpAltKey)) {
        axisY += 1.0f;
    }
    if (IsPressed(*input, m_MoveDownKey, m_MoveDownAltKey)) {
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

    auto applyKey = [&](const std::optional<std::string>& keyName, const char* configKey, Engine::KeyCode& destination) {
        if (!keyName.has_value()) {
            return;
        }
        const Engine::KeyCode parsedKey = KeyNameToKeyCode(*keyName);
        if (parsedKey == Engine::KeyCode::Unknown) {
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
