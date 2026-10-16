#pragma once

#include "UI2D/TextEditing/UITextEditingTypes.h"
#include "UI2D/Widgets/Core/UIWidgetTypes.h"

#include <functional>
#include <string>

namespace Engine::UI2D {

struct UIWidgetContext;

struct UITextBoxWidgetDesc {
    UIWidgetCommonProperties common;
    std::string initialText;
    std::string placeholder;
    std::size_t maxCodepoints = 256;
    bool readOnly = false;
    bool selectAllOnFocus = false;
    UIStyleClassId rootStyle{};
    UIStyleClassId placeholderStyle{};
    UIStyleClassId selectionStyle{};
    UIStyleClassId caretStyle{};
    Engine::FontHandle font{1, 1};
    float fontSize = 16.0f;
    Engine::Color4f textColor{0.92f, 0.94f, 0.98f, 1.0f};
    Engine::Color4f placeholderColor{0.55f, 0.58f, 0.64f, 1.0f};
    Engine::Color4f selectionColor{0.20f, 0.48f, 0.92f, 0.45f};
    Engine::Color4f caretColor{0.92f, 0.94f, 0.98f, 1.0f};
    std::function<void(std::string_view)> onTextChanged;
    std::function<void(std::string_view)> onSubmitted;
    std::function<void()> onEditingCanceled;
    std::function<void()> onFocusEditingStarted;
    std::function<void()> onFocusEditingEnded;

    UITextBoxWidgetDesc() {
        common.focusable = true;
        common.tabIndex = 0;
    }
};

struct UITextBoxWidget {
    static UIWidgetHandle Create(
        UIWidgetContext& context, const UITextBoxWidgetDesc& desc,
        UIWidgetHandle parent = {});
};

} // namespace Engine::UI2D
