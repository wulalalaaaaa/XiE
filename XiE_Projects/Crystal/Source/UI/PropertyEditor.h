#pragma once

#include "Model/CrystalModels.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Crystal {

struct PropertyEditResult {
    bool valid = false;
    Engine::Json::Value value;
    std::string error;
};

class PropertyEditor {
public:
    static PropertyEditResult Parse(
        const CrystalPropertyDefinition& definition,
        std::string_view draft,
        const std::vector<std::string>& entityIds = {}) {
        const std::string text(draft);
        switch (definition.type) {
        case PropertyType::Text:
        case PropertyType::MultilineText:
        case PropertyType::Date:
        case PropertyType::URL:
            return {true, Engine::Json::Value(text), {}};
        case PropertyType::Number:
            return ParseNumber(definition, text);
        case PropertyType::Bool:
            if (text == "true" || text == "1") return {true, Engine::Json::Value(true), {}};
            if (text == "false" || text == "0") return {true, Engine::Json::Value(false), {}};
            return {false, {}, "Invalid boolean"};
        case PropertyType::Enum:
            if (std::find(definition.options.begin(), definition.options.end(), text) == definition.options.end())
                return {false, {}, "Invalid enum option"};
            return {true, Engine::Json::Value(text), {}};
        case PropertyType::EntityReference:
            if (std::find(entityIds.begin(), entityIds.end(), text) == entityIds.end())
                return {false, {}, "Entity reference not found"};
            return {true, Engine::Json::Value(text), {}};
        case PropertyType::EntityReferenceList: {
            Engine::Json::Value::Array references;
            for (const auto& item : Split(text, ',')) {
                if (item.empty()) continue;
                if (std::find(entityIds.begin(), entityIds.end(), item) == entityIds.end())
                    return {false, {}, "Entity reference not found: " + item};
                references.emplace_back(item);
            }
            return {true, Engine::Json::Value(std::move(references)), {}};
        }
        case PropertyType::Tags: {
            Engine::Json::Value::Array tags;
            std::set<std::string> unique;
            for (const auto& item : Split(text, ',')) {
                if (!item.empty() && unique.insert(item).second) tags.emplace_back(item);
            }
            return {true, Engine::Json::Value(std::move(tags)), {}};
        }
        case PropertyType::Progress: {
            CrystalPropertyDefinition progress = definition;
            progress.hasMinValue = true; progress.minValue = 0.0;
            progress.hasMaxValue = true; progress.maxValue = 100.0;
            return ParseNumber(progress, text);
        }
        default:
            return {false, {}, "Unsupported property type"};
        }
    }

    static std::string NextEnum(
        const CrystalPropertyDefinition& definition, std::string_view current) {
        if (definition.options.empty()) return {};
        const auto found = std::find(definition.options.begin(), definition.options.end(), current);
        if (found == definition.options.end() || found + 1 == definition.options.end()) return definition.options.front();
        return *(found + 1);
    }

    static std::vector<std::string> ToggleTag(std::vector<std::string> tags, std::string tag) {
        if (tag.empty()) return tags;
        const auto found = std::find(tags.begin(), tags.end(), tag);
        if (found == tags.end()) tags.push_back(std::move(tag));
        else tags.erase(found);
        return tags;
    }

    static std::vector<std::string> AddReference(
        std::vector<std::string> references, std::string id) {
        if (!id.empty() && std::find(references.begin(), references.end(), id) == references.end())
            references.push_back(std::move(id));
        return references;
    }

    static std::vector<std::string> RemoveReference(
        std::vector<std::string> references, std::string_view id) {
        references.erase(std::remove(references.begin(), references.end(), id), references.end());
        return references;
    }

private:
    static PropertyEditResult ParseNumber(const CrystalPropertyDefinition& definition, const std::string& text) {
        char* end = nullptr;
        const double value = std::strtod(text.c_str(), &end);
        if (text.empty() || end == text.c_str() || *end != '\0' || !std::isfinite(value))
            return {false, {}, "Invalid number"};
        if (definition.hasMinValue && value < definition.minValue)
            return {false, {}, "Invalid: minimum " + std::to_string(definition.minValue)};
        if (definition.hasMaxValue && value > definition.maxValue)
            return {false, {}, "Invalid: maximum " + std::to_string(definition.maxValue)};
        if (definition.hasStepValue && definition.stepValue > 0.0 && definition.hasMinValue) {
            const double units = (value - definition.minValue) / definition.stepValue;
            if (std::abs(units - std::round(units)) > 1e-6)
                return {false, {}, "Invalid: step " + std::to_string(definition.stepValue)};
        }
        return {true, Engine::Json::Value(value), {}};
    }

    static std::vector<std::string> Split(std::string_view text, char delimiter) {
        std::vector<std::string> result;
        std::string current;
        for (const char character : text) {
            if (character == delimiter) {
                Trim(current);
                result.push_back(std::move(current));
                current.clear();
            } else current.push_back(character);
        }
        Trim(current);
        result.push_back(std::move(current));
        return result;
    }
    static void Trim(std::string& text) {
        const auto first = text.find_first_not_of(" \t\r\n");
        const auto last = text.find_last_not_of(" \t\r\n");
        if (first == std::string::npos) { text.clear(); return; }
        text = text.substr(first, last - first + 1);
    }
};

} // namespace Crystal
