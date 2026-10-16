#pragma once

#include "Platform/TextInput/TextInputTypes.h"

#include <string_view>

namespace Engine {

class ITextClipboard {
public:
    virtual ~ITextClipboard() = default;
    [[nodiscard]] virtual bool HasText() const = 0;
    virtual TextClipboardReadResult ReadText() = 0;
    virtual TextClipboardWriteResult WriteText(std::string_view utf8) = 0;
};

} // namespace Engine
