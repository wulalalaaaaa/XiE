#pragma once

#include "UI2D/Core/UINodeRecord.h"

#include <vector>

namespace Engine::UI2D {

class UINodeStore {
public:
    UINodeHandle Create(std::string debugName = {});
    bool Destroy(UINodeHandle handle);
    UINodeRecord* TryGet(UINodeHandle handle);
    const UINodeRecord* TryGet(UINodeHandle handle) const;
    [[nodiscard]] std::vector<UINodeHandle> LiveHandles() const;
    [[nodiscard]] std::size_t Size() const noexcept { return m_Size; }

private:
    struct Slot {
        UINodeRecord record{};
        std::uint32_t generation = 1;
        bool occupied = false;
    };
    std::vector<Slot> m_Slots;
    std::size_t m_Size = 0;
};

} // namespace Engine::UI2D
