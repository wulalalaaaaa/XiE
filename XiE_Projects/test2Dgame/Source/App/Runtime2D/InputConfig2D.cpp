#include "InputConfig2D.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>

namespace Test2D {

namespace {

std::string Trim(const std::string& text) {
    std::size_t begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }

    std::size_t end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }

    return text.substr(begin, end - begin);
}

std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string StripComment(const std::string& line) {
    bool inQuote = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            inQuote = !inQuote;
        }
        if (!inQuote && ch == '#') {
            return line.substr(0, i);
        }
    }
    return line;
}

bool ParseStringValue(const std::string& valueText, std::string& outValue) {
    const std::string trimmed = Trim(valueText);
    if (trimmed.size() < 2 || trimmed.front() != '"' || trimmed.back() != '"') {
        return false;
    }

    outValue = trimmed.substr(1, trimmed.size() - 2);
    return true;
}

bool ParseFloatValue(const std::string& valueText, float& outValue) {
    const std::string trimmed = Trim(valueText);
    if (trimmed.empty()) {
        return false;
    }

    try {
        std::size_t consumed = 0;
        const float value = std::stof(trimmed, &consumed);
        if (consumed != trimmed.size()) {
            return false;
        }
        outValue = value;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

bool LoadInputConfig2D(const std::filesystem::path& filePath, InputConfig2D& outConfig, std::string& outError) {
    std::ifstream input(filePath);
    if (!input.is_open()) {
        outError = "failed to open file";
        return false;
    }

    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;

        const std::string cleanLine = Trim(StripComment(line));
        if (cleanLine.empty()) {
            continue;
        }

        const std::size_t separator = cleanLine.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = ToLower(Trim(cleanLine.substr(0, separator)));
        const std::string value = Trim(cleanLine.substr(separator + 1));
        if (key.empty() || value.empty()) {
            continue;
        }

        std::string stringValue;
        float floatValue = 0.0f;

        if (key == "move_left") {
            if (!ParseStringValue(value, stringValue)) {
                outError = "invalid string for move_left at line " + std::to_string(lineNumber);
                return false;
            }
            outConfig.moveLeft = stringValue;
        } else if (key == "move_right") {
            if (!ParseStringValue(value, stringValue)) {
                outError = "invalid string for move_right at line " + std::to_string(lineNumber);
                return false;
            }
            outConfig.moveRight = stringValue;
        } else if (key == "move_up") {
            if (!ParseStringValue(value, stringValue)) {
                outError = "invalid string for move_up at line " + std::to_string(lineNumber);
                return false;
            }
            outConfig.moveUp = stringValue;
        } else if (key == "move_down") {
            if (!ParseStringValue(value, stringValue)) {
                outError = "invalid string for move_down at line " + std::to_string(lineNumber);
                return false;
            }
            outConfig.moveDown = stringValue;
        } else if (key == "move_left_alt") {
            if (!ParseStringValue(value, stringValue)) {
                outError = "invalid string for move_left_alt at line " + std::to_string(lineNumber);
                return false;
            }
            outConfig.moveLeftAlt = stringValue;
        } else if (key == "move_right_alt") {
            if (!ParseStringValue(value, stringValue)) {
                outError = "invalid string for move_right_alt at line " + std::to_string(lineNumber);
                return false;
            }
            outConfig.moveRightAlt = stringValue;
        } else if (key == "move_up_alt") {
            if (!ParseStringValue(value, stringValue)) {
                outError = "invalid string for move_up_alt at line " + std::to_string(lineNumber);
                return false;
            }
            outConfig.moveUpAlt = stringValue;
        } else if (key == "move_down_alt") {
            if (!ParseStringValue(value, stringValue)) {
                outError = "invalid string for move_down_alt at line " + std::to_string(lineNumber);
                return false;
            }
            outConfig.moveDownAlt = stringValue;
        } else if (key == "move_speed") {
            if (!ParseFloatValue(value, floatValue)) {
                outError = "invalid number for move_speed at line " + std::to_string(lineNumber);
                return false;
            }
            outConfig.moveSpeed = floatValue;
        }
    }

    return true;
}

} // namespace Test2D
