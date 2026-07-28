#pragma once

#include "StoryTypes.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Engine::Story {

struct StoryParseResult {
    bool success = false;
    std::vector<StoryLine> lines;
    std::string error;
};

class StoryParser {
public:
    static StoryParseResult ParseFile(
        const std::filesystem::path& path,
        std::uint32_t segmentOrder,
        const std::string& segmentFileName,
        const std::string& briefDescription
    );
};

} // namespace Engine::Story
