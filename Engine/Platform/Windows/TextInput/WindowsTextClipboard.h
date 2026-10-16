#pragma once

#include "Platform/TextInput/ITextClipboard.h"

namespace Engine {

class WindowsTextClipboard final : public ITextClipboard {
public:
    [[nodiscard]] bool HasText() const override;
    TextClipboardReadResult ReadText() override;
    TextClipboardWriteResult WriteText(std::string_view utf8) override;
};

} // namespace Engine
