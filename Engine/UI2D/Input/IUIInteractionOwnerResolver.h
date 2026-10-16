#pragma once

#include "UI2D/Core/UINodeHandle.h"

namespace Engine::UI2D {

// Resolves the nearest composite control that owns pointer activation while
// leaving the original hit-test leaf available for normal event routing.
class IUIInteractionOwnerResolver {
public:
    virtual ~IUIInteractionOwnerResolver() = default;
    // Returns invalid when the hit leaf has no declared interaction owner.
    [[nodiscard]] virtual UINodeHandle ResolveInteractionOwner(
        UINodeHandle hitLeaf) const = 0;
};

} // namespace Engine::UI2D
