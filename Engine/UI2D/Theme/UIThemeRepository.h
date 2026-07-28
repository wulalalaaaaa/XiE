#pragma once

#include "UI2D/Theme/IUIThemeRepository.h"

#include <vector>

namespace Engine::UI2D {

class UIThemeRepository final : public IUIThemeRepository {
public:
    UIThemeHandle Add(std::shared_ptr<const ResolvedUITheme> theme) override;
    bool Remove(UIThemeHandle handle) override;
    std::shared_ptr<const ResolvedUITheme> TryGet(UIThemeHandle handle) const override;
private:
    struct Slot {
        std::uint32_t generation = 0;
        std::shared_ptr<const ResolvedUITheme> theme;
    };
    std::vector<Slot> m_Slots{1};
    std::vector<std::uint32_t> m_Free;
};

} // namespace Engine::UI2D
