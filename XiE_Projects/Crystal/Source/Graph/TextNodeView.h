#pragma once

#include "Model/CrystalModels.h"

namespace Crystal {
struct TextNodeView {
    const CrystalEntity* entity = nullptr;
    const EntityView* view = nullptr;
};
}
