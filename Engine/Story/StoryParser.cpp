#include "StoryParser.h"

#include <fstream>
#include <sstream>

namespace Engine::Story {
namespace {

constexpr const char* kChineseColon = "\xEF\xBC\x9A";
constexpr std::size_t kChineseColonSize = 3;

std::string TrimCopy(const std::string& value) {
    constexpr const char* whitespace = " \t";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

void ReplaceEscapedChineseColons(std::string& value) {
    const std::string escaped = std::string("\\") + kChineseColon;
    std::size_t position = 0;
    while ((position = value.find(escaped, position)) != std::string::npos) {
        value.replace(position, escaped.size(), kChineseColon);
        position += kChineseColonSize;
    }
}

std::size_t FindUnescapedChineseColon(const std::string& line) {
    std::size_t position = 0;
    while ((position = line.find(kChineseColon, position)) != std::string::npos) {
        if (position == 0 || line[position - 1] != '\\') {
            return position;
        }
        position += kChineseColonSize;
    }
    return std::string::npos;
}

} // namespace

StoryParseResult StoryParser::ParseFile(
    const std::filesystem::path& path,
    const std::uint32_t segmentOrder,
    const std::string& segmentFileName,
    const std::string& briefDescription
) {
    StoryParseResult result{};

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        result.error = "Cannot open story file: " + path.string();
        return result;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    std::string text = buffer.str();
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    std::istringstream lines(text);
    std::string rawLine;
    std::uint32_t lineIndex = 0;
    while (std::getline(lines, rawLine)) {
        if (!rawLine.empty() && rawLine.back() == '\r') {
            rawLine.pop_back();
        }

        std::string line = TrimCopy(rawLine);
        if (line.empty()) {
            continue;
        }

        StoryLine parsed{};
        parsed.position = {segmentOrder, lineIndex++};
        parsed.briefDescription = briefDescription;
        parsed.segmentFileName = segmentFileName;

        const std::size_t colon = FindUnescapedChineseColon(line);
        if (colon != std::string::npos) {
            std::string speaker = TrimCopy(line.substr(0, colon));
            if (!speaker.empty()) {
                parsed.speaker = std::move(speaker);
                parsed.content = TrimCopy(line.substr(colon + kChineseColonSize));
                parsed.isNarration = false;
            } else {
                parsed.content = std::move(line);
            }
        } else {
            parsed.content = std::move(line);
        }

        ReplaceEscapedChineseColons(parsed.speaker);
        ReplaceEscapedChineseColons(parsed.content);
        result.lines.push_back(std::move(parsed));
    }

    if (result.lines.empty()) {
        result.error = "Story file contains no non-empty lines: " + path.string();
        return result;
    }

    result.success = true;
    return result;
}

} // namespace Engine::Story
