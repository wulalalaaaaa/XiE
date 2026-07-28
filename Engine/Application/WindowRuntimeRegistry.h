#pragma once

#include "Application/WindowContext.h"

#include <functional>
#include <optional>
#include <vector>

namespace Engine {

class WindowRuntimeRegistry {
public:
    WindowHandle Register(WindowContext context);
    bool Unregister(WindowHandle handle);

    WindowContext* TryGet(WindowHandle handle);
    const WindowContext* TryGet(WindowHandle handle) const;

    void ForEachActiveWindow(const std::function<void(WindowContext&)>& visitor);
    [[nodiscard]] std::size_t Size() const noexcept { return m_Size; }

private:
    struct Slot {
        std::uint32_t generation = 0;
        std::optional<WindowContext> context;
    };
    std::vector<Slot> m_Slots;
    std::size_t m_Size = 0;
};

} // namespace Engine
