#pragma once

#include "UI2D/Widgets/Core/UIWidgetContext.h"
#include "UI2D/Widgets/Core/UIWidgetTypes.h"
#include "UI2D/Widgets/Virtualization/UIVirtualGridModel.h"

#include <functional>
#include <map>
#include <vector>

namespace Engine::UI2D {

struct UIVirtualizedGridCallbacks {
    std::function<UIWidgetHandle(UIWidgetContext&, UIWidgetHandle)> createItem;
    std::function<void(UIWidgetHandle, std::uint64_t)> bindItem;
    std::function<void(UIWidgetHandle, std::uint64_t)> unbindItem;
};

class UIVirtualizedGridBehavior {
public:
    UIVirtualizedGridBehavior(
        UIWindowRuntime& runtime,
        UIWidgetHandle root,
        UIVirtualGridMetrics metrics,
        std::uint64_t itemCount,
        UIVirtualizedGridCallbacks callbacks);
    ~UIVirtualizedGridBehavior();

    UIVirtualizedGridBehavior(const UIVirtualizedGridBehavior&) = delete;
    UIVirtualizedGridBehavior& operator=(const UIVirtualizedGridBehavior&) = delete;

    bool SetItemCount(std::uint64_t itemCount);
    bool SetMetrics(const UIVirtualGridMetrics& metrics);
    bool RefreshVisibleItems(
        float viewportWidth, float viewportHeight, float scrollOffsetY);
    void RefreshBindings();
    void Clear();

    [[nodiscard]] UIVirtualGridSnapshot Snapshot() const noexcept;
    [[nodiscard]] UIWidgetHandle Root() const noexcept { return m_Root; }

private:
    struct ActiveSlot {
        UIWidgetHandle widget{};
        std::uint64_t index = 0;
    };

    bool ApplyLayout(const UIVirtualGridLayout& next);
    UIWidgetHandle Acquire(std::uint64_t index);
    void Release(std::map<std::uint64_t, ActiveSlot>::iterator slot);
    void Position(UIWidgetHandle widget, std::uint64_t index);

    UIWindowRuntime* m_Runtime = nullptr;
    UIWidgetHandle m_Root{};
    UIVirtualGridModel m_Model;
    UIVirtualizedGridCallbacks m_Callbacks;
    UIVirtualGridState m_State{};
    UIVirtualGridLayout m_Layout{};
    std::map<std::uint64_t, ActiveSlot> m_Active;
    std::vector<UIWidgetHandle> m_Pool;
    bool m_HasLayout = false;
};

} // namespace Engine::UI2D
