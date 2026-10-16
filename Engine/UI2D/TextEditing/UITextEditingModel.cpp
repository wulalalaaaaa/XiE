#include "UI2D/TextEditing/UITextEditingModel.h"

#include "UI2D/TextEditing/UITextEditingUtils.h"

#include <algorithm>

namespace Engine::UI2D {

std::size_t UITextEditingModel::Length() const noexcept {
    return Utf8ScalarCount(m_Text);
}

bool UITextEditingModel::SetText(std::string text) {
    if (!IsValidUtf8(text) || Utf8ScalarCount(text) > m_MaxCodepoints)
        return false;
    if (text == m_Text) return true;
    PushUndo();
    m_Text = std::move(text);
    m_Selection = {Length(), Length()};
    return true;
}

void UITextEditingModel::PushUndo() {
    if (m_Undo.size() == 64) m_Undo.erase(m_Undo.begin());
    m_Undo.push_back({m_Text, m_Selection});
    m_Redo.clear();
}

bool UITextEditingModel::ReplaceSelection(std::string_view utf8) {
    if (!IsValidUtf8(utf8)) return false;
    const auto start = Utf8ByteOffset(m_Text, m_Selection.Start());
    const auto end = Utf8ByteOffset(m_Text, m_Selection.End());
    if (!start || !end) return false;
    const std::size_t replacementCount = Utf8ScalarCount(utf8);
    const std::size_t nextLength =
        Length() - (m_Selection.End() - m_Selection.Start()) +
        replacementCount;
    if (nextLength > m_MaxCodepoints) return false;
    PushUndo();
    m_Text.replace(*start, *end - *start, utf8);
    const std::size_t caret = m_Selection.Start() + replacementCount;
    m_Selection = {caret, caret};
    return true;
}

bool UITextEditingModel::InsertText(std::string_view utf8) {
    if (utf8.empty()) return true;
    return ReplaceSelection(utf8);
}

bool UITextEditingModel::DeleteBackward() {
    if (!m_Selection.Empty()) return ReplaceSelection({});
    if (m_Selection.active == 0) return false;
    m_Selection.anchor = m_Selection.active - 1;
    return ReplaceSelection({});
}

bool UITextEditingModel::DeleteForward() {
    if (!m_Selection.Empty()) return ReplaceSelection({});
    if (m_Selection.active >= Length()) return false;
    m_Selection.active += 1;
    return ReplaceSelection({});
}

void UITextEditingModel::CollapseForMove(
    std::size_t index, bool extendSelection) {
    if (extendSelection) {
        m_Selection.active = index;
    } else {
        m_Selection = {index, index};
    }
}

bool UITextEditingModel::MoveLeft(bool extendSelection) {
    std::size_t target = m_Selection.active;
    if (!extendSelection && !m_Selection.Empty())
        target = m_Selection.Start();
    else if (target > 0)
        --target;
    const auto before = m_Selection;
    CollapseForMove(target, extendSelection);
    return before.anchor != m_Selection.anchor ||
        before.active != m_Selection.active;
}

bool UITextEditingModel::MoveRight(bool extendSelection) {
    std::size_t target = m_Selection.active;
    if (!extendSelection && !m_Selection.Empty())
        target = m_Selection.End();
    else if (target < Length())
        ++target;
    const auto before = m_Selection;
    CollapseForMove(target, extendSelection);
    return before.anchor != m_Selection.anchor ||
        before.active != m_Selection.active;
}

bool UITextEditingModel::MoveHome(bool extendSelection) {
    const auto before = m_Selection;
    CollapseForMove(0, extendSelection);
    return before.anchor != m_Selection.anchor ||
        before.active != m_Selection.active;
}

bool UITextEditingModel::MoveEnd(bool extendSelection) {
    const auto before = m_Selection;
    CollapseForMove(Length(), extendSelection);
    return before.anchor != m_Selection.anchor ||
        before.active != m_Selection.active;
}

bool UITextEditingModel::SelectAll() {
    const auto before = m_Selection;
    m_Selection = {0, Length()};
    return before.anchor != m_Selection.anchor ||
        before.active != m_Selection.active;
}

void UITextEditingModel::SetSelection(UITextSelection selection) {
    const std::size_t length = Length();
    selection.anchor = std::min(selection.anchor, length);
    selection.active = std::min(selection.active, length);
    m_Selection = selection;
}

bool UITextEditingModel::SetCaret(std::size_t index) {
    if (index > Length()) return false;
    m_Selection = {index, index};
    return true;
}

bool UITextEditingModel::Restore(
    const UITextEditSnapshot& snapshot,
    std::vector<UITextEditSnapshot>& destination) {
    if (destination.size() == 64) destination.erase(destination.begin());
    destination.push_back({m_Text, m_Selection});
    m_Text = snapshot.text;
    m_Selection = snapshot.selection;
    return true;
}

bool UITextEditingModel::Undo() {
    if (m_Undo.empty()) return false;
    const auto snapshot = m_Undo.back();
    m_Undo.pop_back();
    return Restore(snapshot, m_Redo);
}

bool UITextEditingModel::Redo() {
    if (m_Redo.empty()) return false;
    const auto snapshot = m_Redo.back();
    m_Redo.pop_back();
    return Restore(snapshot, m_Undo);
}

} // namespace Engine::UI2D
