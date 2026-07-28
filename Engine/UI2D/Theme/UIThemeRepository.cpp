#include "UI2D/Theme/UIThemeRepository.h"

namespace Engine::UI2D {

UIThemeHandle UIThemeRepository::Add(std::shared_ptr<const ResolvedUITheme> theme) {
    if (!theme) return {};
    std::uint32_t index = 0;
    if (!m_Free.empty()) { index = m_Free.back(); m_Free.pop_back(); }
    else { index = static_cast<std::uint32_t>(m_Slots.size()); m_Slots.push_back({}); }
    Slot& slot = m_Slots[index];
    ++slot.generation;
    if (slot.generation == 0) ++slot.generation;
    slot.theme = std::move(theme);
    return {index, slot.generation};
}
bool UIThemeRepository::Remove(UIThemeHandle handle) {
    if (!handle.IsValid() || handle.index >= m_Slots.size()) return false;
    Slot& slot = m_Slots[handle.index];
    if (slot.generation != handle.generation || !slot.theme) return false;
    slot.theme.reset();
    m_Free.push_back(handle.index);
    return true;
}
std::shared_ptr<const ResolvedUITheme> UIThemeRepository::TryGet(UIThemeHandle handle) const {
    if (!handle.IsValid() || handle.index >= m_Slots.size()) return nullptr;
    const Slot& slot = m_Slots[handle.index];
    return slot.generation == handle.generation ? slot.theme : nullptr;
}

} // namespace Engine::UI2D
