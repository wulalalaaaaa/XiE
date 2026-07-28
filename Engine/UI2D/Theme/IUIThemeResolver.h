#pragma once

#include "UI2D/Theme/UIThemeDefinition.h"
#include "UI2D/Theme/ResolvedUITheme.h"

#include <memory>
#include <string>
#include <vector>

namespace Engine::UI2D {

struct UIThemeResolveResult {
    bool success = false;
    std::shared_ptr<const ResolvedUITheme> theme;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    [[nodiscard]] bool Succeeded() const noexcept { return success && theme != nullptr && errors.empty(); }
};
using ThemeResolveResult = UIThemeResolveResult;

class IUIThemeResolver {
public:
    virtual ~IUIThemeResolver() = default;
    virtual UIThemeResolveResult Resolve(const UIThemeDefinition& definition) = 0;
};

} // namespace Engine::UI2D
