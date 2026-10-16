#pragma once

#include "UI2D/TextEditing/UITextEditingTypes.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Engine::UI2D {

class UITextEditingModel {
public:
    explicit UITextEditingModel(std::size_t maxCodepoints = 256)
        : m_MaxCodepoints(maxCodepoints) {}

    [[nodiscard]] const std::string& Text() const noexcept { return m_Text; }
    [[nodiscard]] UITextSelection Selection() const noexcept {
        return m_Selection;
    }
    [[nodiscard]] std::size_t CaretIndex() const noexcept {
        return m_Selection.active;
    }
    [[nodiscard]] std::size_t Length() const noexcept;
    [[nodiscard]] std::size_t MaxCodepoints() const noexcept {
        return m_MaxCodepoints;
    }

    bool SetText(std::string text);
    bool InsertText(std::string_view utf8);
    bool DeleteBackward();
    bool DeleteForward();
    bool MoveLeft(bool extendSelection);
    bool MoveRight(bool extendSelection);
    bool MoveHome(bool extendSelection);
    bool MoveEnd(bool extendSelection);
    bool SelectAll();
    bool ReplaceSelection(std::string_view utf8);
    void SetSelection(UITextSelection selection);
    bool SetCaret(std::size_t index);

    bool Undo();
    bool Redo();
    [[nodiscard]] std::size_t UndoDepth() const noexcept {
        return m_Undo.size();
    }

private:
    void PushUndo();
    bool Restore(
        const UITextEditSnapshot& snapshot,
        std::vector<UITextEditSnapshot>& destination);
    void CollapseForMove(std::size_t index, bool extendSelection);

    std::string m_Text;
    UITextSelection m_Selection;
    std::size_t m_MaxCodepoints = 256;
    std::vector<UITextEditSnapshot> m_Undo;
    std::vector<UITextEditSnapshot> m_Redo;
};

} // namespace Engine::UI2D
