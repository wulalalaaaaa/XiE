#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace Engine::UI2D {

enum class SelectionKind {
    EntityView,
    Board,
    InkStroke,
    InkGroup,
    DashboardWidget,
};

struct SelectionItem {
    SelectionKind kind = SelectionKind::EntityView;
    std::string id;
};

class SelectionModel {
public:
    void Select(std::string entityId, bool additive = false) {
        SelectItem(SelectionKind::EntityView, std::move(entityId), additive);
    }
    void ToggleEntity(std::string entityId) {
        ToggleItem(SelectionKind::EntityView, std::move(entityId));
    }
    void SelectBoard(std::string boardId, bool additive = false) {
        SelectItem(SelectionKind::Board, std::move(boardId), additive);
    }
    void ToggleBoard(std::string boardId) {
        ToggleItem(SelectionKind::Board, std::move(boardId));
    }
    void SelectWidget(std::string widgetId, bool additive = false) {
        SelectItem(SelectionKind::DashboardWidget, std::move(widgetId), additive);
    }
    void ToggleWidget(std::string widgetId) {
        ToggleItem(SelectionKind::DashboardWidget, std::move(widgetId));
    }
    void SelectInkGroup(std::string groupId, bool additive = false) {
        SelectItem(SelectionKind::InkGroup, std::move(groupId), additive);
    }
    void ToggleInkGroup(std::string groupId) {
        ToggleItem(SelectionKind::InkGroup, std::move(groupId));
    }
    void SelectInk(std::string strokeId, bool additive = false) {
        SelectItem(SelectionKind::InkStroke, std::move(strokeId), additive);
    }
    void ToggleInk(std::string strokeId) {
        ToggleItem(SelectionKind::InkStroke, std::move(strokeId));
    }
    void ClearInk() noexcept { ClearKind(SelectionKind::InkStroke); }
    void Clear() noexcept {
        m_Items.clear();
        m_SelectedId.clear();
        m_SelectedInkIds.clear();
    }
    [[nodiscard]] bool HasSelection() const noexcept { return !m_Items.empty(); }
    [[nodiscard]] bool IsSelected(const std::string& entityId) const noexcept {
        return IsSelected(SelectionKind::EntityView, entityId);
    }
    [[nodiscard]] const std::string& SelectedId() const noexcept { return m_SelectedId; }
    [[nodiscard]] bool IsSelected(SelectionKind kind, const std::string& id) const noexcept {
        return std::find_if(m_Items.begin(), m_Items.end(),
            [&](const SelectionItem& item) { return item.kind == kind && item.id == id; }) != m_Items.end();
    }
    [[nodiscard]] const std::vector<SelectionItem>& Items() const noexcept { return m_Items; }
    [[nodiscard]] bool IsInkSelected(const std::string& strokeId) const noexcept {
        return IsSelected(SelectionKind::InkStroke, strokeId);
    }
    [[nodiscard]] const std::vector<std::string>& SelectedInkIds() const noexcept { return m_SelectedInkIds; }

private:
    void SelectItem(SelectionKind kind, std::string id, bool additive) {
        if (id.empty()) return;
        if (!additive) Clear();
        if (!IsSelected(kind, id)) m_Items.push_back({kind, std::move(id)});
        SyncLegacyViews();
    }
    void ToggleItem(SelectionKind kind, std::string id) {
        if (id.empty()) return;
        const auto found = std::find_if(m_Items.begin(), m_Items.end(),
            [&](const SelectionItem& item) { return item.kind == kind && item.id == id; });
        if (found == m_Items.end()) m_Items.push_back({kind, std::move(id)});
        else m_Items.erase(found);
        SyncLegacyViews();
    }
    void ClearKind(SelectionKind kind) noexcept {
        m_Items.erase(std::remove_if(m_Items.begin(), m_Items.end(),
            [&](const SelectionItem& item) { return item.kind == kind; }), m_Items.end());
        SyncLegacyViews();
    }
    void SyncLegacyViews() {
        m_SelectedId.clear();
        m_SelectedInkIds.clear();
        for (const SelectionItem& item : m_Items) {
            if (item.kind == SelectionKind::EntityView && m_SelectedId.empty()) m_SelectedId = item.id;
            if (item.kind == SelectionKind::InkStroke) m_SelectedInkIds.push_back(item.id);
        }
    }

    std::string m_SelectedId;
    std::vector<std::string> m_SelectedInkIds;
    std::vector<SelectionItem> m_Items;
};

} // namespace Engine::UI2D
