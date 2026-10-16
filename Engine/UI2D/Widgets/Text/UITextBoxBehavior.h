#pragma once

#include "UI2D/Animation/UIAnimationTypes.h"
#include "UI2D/TextEditing/UITextEditingModel.h"
#include "UI2D/TextEditing/UITextHitMap.h"
#include "UI2D/Widgets/Text/UITextBoxWidget.h"

namespace Engine::UI2D {

struct UITextBoxState {
    explicit UITextBoxState(std::size_t maxCodepoints)
        : model(maxCodepoints) {}

    UITextEditingModel model;
    UITextCompositionState composition;
    UITextHitMap hitMap;
    std::string placeholder;
    bool readOnly = false;
    bool selectAllOnFocus = false;
    bool selecting = false;
    Engine::PointerId selectionPointer = 0;
    float horizontalTextOffset = 0.0f;
    float viewportWidth = 0.0f;
    Engine::FontHandle font{1, 1};
    float fontSize = 16.0f;
    Engine::Color4f textColor{};
    Engine::Color4f placeholderColor{};
    Engine::Color4f selectionColor{};
    Engine::Color4f caretColor{};
    UIStyleClassId placeholderStyle{};
    UIStyleClassId selectionStyle{};
    UIStyleClassId caretStyle{};
    UINodeHandle selectionNode{};
    UINodeHandle textNode{};
    UINodeHandle compositionNode{};
    UINodeHandle caretNode{};
    UIAnimationHandle caretBlink{};
    std::function<void(std::string_view)> onTextChanged;
    std::function<void(std::string_view)> onSubmitted;
    std::function<void()> onEditingCanceled;
    std::function<void()> onFocusEditingStarted;
    std::function<void()> onFocusEditingEnded;
};

} // namespace Engine::UI2D
