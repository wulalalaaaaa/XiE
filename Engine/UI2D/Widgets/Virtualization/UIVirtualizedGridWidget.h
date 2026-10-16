#pragma once

#include "UI2D/Widgets/Core/UIWidgetTypes.h"
#include "UI2D/Widgets/Virtualization/UIVirtualizedGridBehavior.h"

#include <memory>

namespace Engine::UI2D {

struct UIVirtualizedGridWidgetDesc {
    UIWidgetCommonProperties common;
    UIVirtualGridMetrics metrics{};
    std::uint64_t itemCount = 0;
    UIVirtualizedGridCallbacks callbacks;
    Engine::Color4f backgroundColor{};
};

class UIVirtualizedGridWidget {
public:
    static std::unique_ptr<UIVirtualizedGridWidget> Create(
        UIWidgetContext& context,
        const UIVirtualizedGridWidgetDesc& desc,
        UIWidgetHandle scrollView);
    ~UIVirtualizedGridWidget();

    bool SetItemCount(std::uint64_t itemCount);
    bool SetMetrics(const UIVirtualGridMetrics& metrics);
    bool RefreshVisibleItems();
    void RefreshBindings();
    [[nodiscard]] UIVirtualGridSnapshot Snapshot() const noexcept;
    [[nodiscard]] UIWidgetHandle Root() const noexcept;

private:
    UIVirtualizedGridWidget(
        UIWindowRuntime& runtime,
        UIWidgetHandle scrollView,
        UIWidgetConnectionHandle scrollConnection,
        std::unique_ptr<UIVirtualizedGridBehavior> behavior);

    UIWindowRuntime* m_Runtime = nullptr;
    UIWidgetHandle m_ScrollView{};
    UIWidgetConnectionHandle m_ScrollConnection{};
    std::unique_ptr<UIVirtualizedGridBehavior> m_Behavior;
};

} // namespace Engine::UI2D
