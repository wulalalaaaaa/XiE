#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Engine::UI2D {

// Host-independent local command history for visual workspace operations.
// Commands must contain only reversible local visual mutations; server facts
// such as locks, activities and worker state are deliberately out of scope.
class WorkspaceCommandHistory {
public:
    struct Command {
        std::string label;
        std::function<void()> undo;
        std::function<void()> redo;
    };

    void Execute(Command command) {
        if (command.redo) command.redo();
        m_Undo.push_back(std::move(command));
        m_Redo.clear();
    }
    bool Undo() {
        if (m_Undo.empty()) return false;
        Command command = std::move(m_Undo.back());
        m_Undo.pop_back();
        if (command.undo) command.undo();
        m_Redo.push_back(std::move(command));
        return true;
    }
    bool Redo() {
        if (m_Redo.empty()) return false;
        Command command = std::move(m_Redo.back());
        m_Redo.pop_back();
        if (command.redo) command.redo();
        m_Undo.push_back(std::move(command));
        return true;
    }
    void Clear() noexcept { m_Undo.clear(); m_Redo.clear(); }
    [[nodiscard]] bool CanUndo() const noexcept { return !m_Undo.empty(); }
    [[nodiscard]] bool CanRedo() const noexcept { return !m_Redo.empty(); }
    [[nodiscard]] std::size_t UndoCount() const noexcept { return m_Undo.size(); }
    [[nodiscard]] std::size_t RedoCount() const noexcept { return m_Redo.size(); }

private:
    std::vector<Command> m_Undo;
    std::vector<Command> m_Redo;
};

} // namespace Engine::UI2D
