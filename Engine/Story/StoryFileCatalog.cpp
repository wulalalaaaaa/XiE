#include "StoryFileCatalog.h"

#include "StoryParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>

namespace Engine::Story {
namespace {

struct SegmentFile {
    std::filesystem::path path;
    std::string fileName;
    std::string briefDescription;
    std::uint32_t order = 0;
};

std::string PathToUtf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.generic_u8string();
    return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
}

bool HasStoryExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return extension == ".story";
}

bool ParseSegmentFileName(const std::filesystem::path& path, SegmentFile& outSegment) {
    const std::string stem = PathToUtf8(path.stem());
    std::size_t digitCount = 0;
    while (digitCount < stem.size() &&
           std::isdigit(static_cast<unsigned char>(stem[digitCount])) != 0) {
        ++digitCount;
    }
    if (digitCount == 0) {
        return false;
    }

    try {
        const unsigned long value = std::stoul(stem.substr(0, digitCount));
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            return false;
        }
        outSegment.order = static_cast<std::uint32_t>(value);
    } catch (...) {
        return false;
    }

    std::string description = stem.substr(digitCount);
    while (!description.empty()) {
        const char first = description.front();
        if (first != '_' && first != '-' && first != '.' && first != ' ') {
            break;
        }
        description.erase(description.begin());
    }

    outSegment.path = path;
    outSegment.fileName = PathToUtf8(path.filename());
    outSegment.briefDescription = std::move(description);
    return true;
}

void HashBytes(StoryContentVersion& hash, const char* bytes, const std::size_t count) {
    constexpr StoryContentVersion kPrime = 1099511628211ull;
    for (std::size_t index = 0; index < count; ++index) {
        hash ^= static_cast<unsigned char>(bytes[index]);
        hash *= kPrime;
    }
}

bool HashFile(
    StoryContentVersion& hash,
    const std::filesystem::path& root,
    const std::filesystem::path& path,
    std::string& outError
) {
    std::error_code relativeError;
    const std::filesystem::path relative = std::filesystem::relative(path, root, relativeError);
    if (relativeError) {
        outError = "Cannot make story path relative: " + PathToUtf8(path);
        return false;
    }

    const std::string relativeText = PathToUtf8(relative);
    HashBytes(hash, relativeText.data(), relativeText.size());
    const char separator = '\n';
    HashBytes(hash, &separator, 1);

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        outError = "Cannot hash story file: " + PathToUtf8(path);
        return false;
    }

    char buffer[4096];
    while (input) {
        input.read(buffer, sizeof(buffer));
        HashBytes(hash, buffer, static_cast<std::size_t>(input.gcount()));
    }
    HashBytes(hash, &separator, 1);
    return true;
}

} // namespace

bool StoryFileCatalog::Load(const std::filesystem::path& rootPath) {
    std::map<StoryKey, StoryDefinition> loadedStories;
    StoryContentVersion loadedVersion = 14695981039346656037ull;

    std::error_code rootError;
    if (!std::filesystem::is_directory(rootPath, rootError) || rootError) {
        m_LastError = "Story root is not a directory: " + PathToUtf8(rootPath);
        return false;
    }

    std::vector<std::filesystem::path> categoryPaths;
    for (const auto& entry : std::filesystem::directory_iterator(rootPath)) {
        if (entry.is_directory()) {
            categoryPaths.push_back(entry.path());
        }
    }
    std::sort(categoryPaths.begin(), categoryPaths.end());

    for (const std::filesystem::path& categoryPath : categoryPaths) {
        std::vector<std::filesystem::path> storyPaths;
        for (const auto& entry : std::filesystem::directory_iterator(categoryPath)) {
            if (entry.is_directory()) {
                storyPaths.push_back(entry.path());
            }
        }
        std::sort(storyPaths.begin(), storyPaths.end());

        for (const std::filesystem::path& storyPath : storyPaths) {
            StoryDefinition definition{};
            definition.key.category = PathToUtf8(categoryPath.filename());
            definition.key.title = PathToUtf8(storyPath.filename());

            std::vector<SegmentFile> segments;
            for (const auto& entry : std::filesystem::directory_iterator(storyPath)) {
                if (!entry.is_regular_file() || !HasStoryExtension(entry.path())) {
                    continue;
                }

                SegmentFile segment{};
                if (!ParseSegmentFileName(entry.path(), segment)) {
                    m_LastError = "Story segment must begin with digits: " + PathToUtf8(entry.path());
                    return false;
                }
                segments.push_back(std::move(segment));
            }

            std::sort(segments.begin(), segments.end(), [](const SegmentFile& lhs, const SegmentFile& rhs) {
                if (lhs.order != rhs.order) {
                    return lhs.order < rhs.order;
                }
                return lhs.fileName < rhs.fileName;
            });

            if (segments.empty()) {
                continue;
            }

            std::set<std::uint32_t> segmentOrders;
            for (const SegmentFile& segment : segments) {
                if (!segmentOrders.insert(segment.order).second) {
                    m_LastError =
                        "Duplicate segment order in story " + definition.key.ToString() +
                        ": " + std::to_string(segment.order);
                    return false;
                }

                StoryParseResult parsed = StoryParser::ParseFile(
                    segment.path,
                    segment.order,
                    segment.fileName,
                    segment.briefDescription);
                if (!parsed.success) {
                    m_LastError = std::move(parsed.error);
                    return false;
                }

                if (!HashFile(loadedVersion, rootPath, segment.path, m_LastError)) {
                    return false;
                }

                definition.lines.insert(
                    definition.lines.end(),
                    std::make_move_iterator(parsed.lines.begin()),
                    std::make_move_iterator(parsed.lines.end()));
            }

            loadedStories.emplace(definition.key, std::move(definition));
        }
    }

    m_RootPath = rootPath;
    m_Stories = std::move(loadedStories);
    m_ContentVersion = loadedVersion;
    m_LastError.clear();
    return true;
}

StoryContentVersion StoryFileCatalog::ContentVersion() const {
    return m_ContentVersion;
}

const StoryDefinition* StoryFileCatalog::FindStory(const StoryKey& key) const {
    const auto found = m_Stories.find(key);
    return found == m_Stories.end() ? nullptr : &found->second;
}

const StoryLine* StoryFileCatalog::FindLine(
    const StoryKey& key,
    const StoryPosition position
) const {
    const StoryDefinition* story = FindStory(key);
    if (story == nullptr) {
        return nullptr;
    }
    const auto found = std::find_if(story->lines.begin(), story->lines.end(), [position](const StoryLine& line) {
        return line.position == position;
    });
    return found == story->lines.end() ? nullptr : &*found;
}

bool StoryFileCatalog::FirstPosition(const StoryKey& key, StoryPosition& outPosition) const {
    const StoryDefinition* story = FindStory(key);
    if (story == nullptr || story->lines.empty()) {
        return false;
    }
    outPosition = story->lines.front().position;
    return true;
}

bool StoryFileCatalog::NextPosition(
    const StoryKey& key,
    const StoryPosition current,
    StoryPosition& outPosition
) const {
    const StoryDefinition* story = FindStory(key);
    if (story == nullptr) {
        return false;
    }
    for (std::size_t index = 0; index < story->lines.size(); ++index) {
        if (story->lines[index].position != current) {
            continue;
        }
        if (index + 1 >= story->lines.size()) {
            return false;
        }
        outPosition = story->lines[index + 1].position;
        return true;
    }
    return false;
}

std::vector<StoryKey> StoryFileCatalog::StoryKeys() const {
    std::vector<StoryKey> keys;
    keys.reserve(m_Stories.size());
    for (const auto& [key, definition] : m_Stories) {
        (void)definition;
        keys.push_back(key);
    }
    return keys;
}

const std::filesystem::path& StoryFileCatalog::RootPath() const {
    return m_RootPath;
}

const std::string& StoryFileCatalog::LastError() const {
    return m_LastError;
}

} // namespace Engine::Story
