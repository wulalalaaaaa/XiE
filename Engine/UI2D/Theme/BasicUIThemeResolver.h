#pragma once

#include "UI2D/Theme/IUIThemeResolver.h"

namespace Engine::UI2D {

class BasicUIThemeResolver final : public IUIThemeResolver {
public:
    UIThemeResolveResult Resolve(const UIThemeDefinition& definition) override;
};

} // namespace Engine::UI2D
