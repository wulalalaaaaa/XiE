#pragma once

#include <string>

namespace Engine {

struct TextClipboardReadResult {
    bool success = false;
    std::string text;
    std::string error;
};

struct TextClipboardWriteResult {
    bool success = false;
    std::string error;
};

} // namespace Engine
