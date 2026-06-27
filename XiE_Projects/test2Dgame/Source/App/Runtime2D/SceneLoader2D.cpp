#include "SceneLoader2D.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace Test2D {

namespace {

struct ParsedEntityLine {
    Entity2D entity{};
    std::string spritePath = "Assets/Sprite/main.xsprite";
    bool isPlayer = false;
};

std::vector<std::string> g_SceneSpriteStorage;

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

std::vector<std::string> SplitTokensRespectingQuotes(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuote = false;
    for (char ch : text) {
        if (ch == '"') {
            inQuote = !inQuote;
            current.push_back(ch);
            continue;
        }
        if (!inQuote && std::isspace(static_cast<unsigned char>(ch)) != 0) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

bool ParseBool(const std::string& text, bool& outValue) {
    const std::string lowered = ToLower(Trim(text));
    if (lowered == "true" || lowered == "1") {
        outValue = true;
        return true;
    }
    if (lowered == "false" || lowered == "0") {
        outValue = false;
        return true;
    }
    return false;
}

bool ParseFloat(const std::string& text, float& outValue) {
    try {
        const std::string trimmed = Trim(text);
        std::size_t consumed = 0;
        const float parsed = std::stof(trimmed, &consumed);
        if (consumed != trimmed.size()) {
            return false;
        }
        outValue = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseInt(const std::string& text, int& outValue) {
    try {
        const std::string trimmed = Trim(text);
        std::size_t consumed = 0;
        const int parsed = std::stoi(trimmed, &consumed);
        if (consumed != trimmed.size()) {
            return false;
        }
        outValue = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseStringLiteral(const std::string& text, std::string& outValue) {
    const std::string trimmed = Trim(text);
    if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
        outValue = trimmed.substr(1, trimmed.size() - 2);
        return true;
    }
    outValue = trimmed;
    return !outValue.empty();
}

bool ParseEntityLine(const std::string& line, ParsedEntityLine& outEntity, std::string& outError) {
    outEntity.entity.collider.halfExtent = {16.0f, 16.0f};
    outEntity.entity.collider.isStatic = false;
    outEntity.entity.sprite.spritePath = "Assets/Sprite/main.xsprite";
    outEntity.entity.renderable = {};
    outEntity.isPlayer = false;

    const std::vector<std::string> tokens = SplitTokensRespectingQuotes(line);
    for (const std::string& token : tokens) {
        const std::size_t eqPos = token.find('=');
        if (eqPos == std::string::npos || eqPos == 0 || eqPos + 1 >= token.size()) {
            continue;
        }

        const std::string key = ToLower(token.substr(0, eqPos));
        const std::string rawValue = token.substr(eqPos + 1);

        if (key == "name") {
            continue;
        }
        if (key == "x") {
            if (!ParseFloat(rawValue, outEntity.entity.transform.position.x)) {
                outError = "invalid x value";
                return false;
            }
            continue;
        }
        if (key == "y") {
            if (!ParseFloat(rawValue, outEntity.entity.transform.position.y)) {
                outError = "invalid y value";
                return false;
            }
            continue;
        }
        if (key == "half_x") {
            if (!ParseFloat(rawValue, outEntity.entity.collider.halfExtent.x)) {
                outError = "invalid half_x value";
                return false;
            }
            continue;
        }
        if (key == "half_y") {
            if (!ParseFloat(rawValue, outEntity.entity.collider.halfExtent.y)) {
                outError = "invalid half_y value";
                return false;
            }
            continue;
        }
        if (key == "static") {
            if (!ParseBool(rawValue, outEntity.entity.collider.isStatic)) {
                outError = "invalid static value";
                return false;
            }
            continue;
        }
        if (key == "sprite") {
            std::string spritePath;
            if (!ParseStringLiteral(rawValue, spritePath)) {
                outError = "invalid sprite value";
                return false;
            }
            outEntity.spritePath = spritePath;
            continue;
        }
        if (key == "player") {
            if (!ParseBool(rawValue, outEntity.isPlayer)) {
                outError = "invalid player value";
                return false;
            }
            continue;
        }
        if (key == "visible") {
            if (!ParseBool(rawValue, outEntity.entity.renderable.visible)) {
                outError = "invalid visible value";
                return false;
            }
            continue;
        }
        if (key == "layer") {
            if (!ParseInt(rawValue, outEntity.entity.renderable.layer)) {
                outError = "invalid layer value";
                return false;
            }
            continue;
        }
    }

    return true;
}

} // namespace

bool LoadScene2D(const std::filesystem::path& scenePath, World2D& world, std::string& outError) {
    std::ifstream file(scenePath);
    if (!file.is_open()) {
        outError = "failed to open scene file";
        return false;
    }

    std::vector<ParsedEntityLine> parsedEntities;
    std::optional<std::size_t> playerIndex;
    bool headerValidated = false;

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        const std::string content = Trim(StripComment(line));
        if (content.empty()) {
            continue;
        }

        if (!headerValidated) {
            if (content != "xie_scene2d 1") {
                outError = "invalid scene header at line " + std::to_string(lineNumber);
                return false;
            }
            headerValidated = true;
            continue;
        }

        if (content.rfind("entity ", 0) != 0) {
            continue;
        }

        ParsedEntityLine parsed{};
        std::string parseError;
        if (!ParseEntityLine(content.substr(7), parsed, parseError)) {
            outError = parseError + " at line " + std::to_string(lineNumber);
            return false;
        }

        if (parsed.isPlayer) {
            if (playerIndex.has_value()) {
                outError = "multiple player=true entities are not supported";
                return false;
            }
            playerIndex = parsedEntities.size();
        }

        parsedEntities.push_back(parsed);
    }

    if (!headerValidated) {
        outError = "missing scene header";
        return false;
    }
    if (parsedEntities.empty()) {
        outError = "scene has no entities";
        return false;
    }
    if (!playerIndex.has_value()) {
        outError = "scene has no player=true entity";
        return false;
    }

    g_SceneSpriteStorage.clear();
    g_SceneSpriteStorage.reserve(parsedEntities.size());

    world.Clear();
    for (std::size_t i = 0; i < parsedEntities.size(); ++i) {
        Entity2D entity = parsedEntities[i].entity;
        g_SceneSpriteStorage.push_back(parsedEntities[i].spritePath);
        entity.sprite.spritePath = g_SceneSpriteStorage.back().c_str();
        const EntityId id = world.CreateEntity(entity);
        if (i == *playerIndex) {
            world.SetPlayer(id);
        }
    }

    return true;
}

} // namespace Test2D
