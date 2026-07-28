#pragma once

#include "DrawList2D.h"

#include <string>
#include <vector>

namespace Engine {

struct DrawListValidationResult {
    bool valid = true;
    int clipDepth = 0;
    std::vector<std::string> errors;
};

class DrawListValidator {
public:
    DrawListValidationResult Validate(const DrawList2D& drawList) const;
};

} // namespace Engine
