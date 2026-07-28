#pragma once

#include "UI2D/Theme/ResolvedUITheme.h"

#include <string_view>

namespace Engine::UI2D {

class IUIThemeRepository {
public:
    virtual ~IUIThemeRepository() = default;
    virtual UIThemeHandle Add(std::shared_ptr<const ResolvedUITheme> theme) = 0;
    virtual bool Remove(UIThemeHandle handle) = 0;
    virtual std::shared_ptr<const ResolvedUITheme> TryGet(UIThemeHandle handle) const = 0;
};

} // namespace Engine::UI2D
