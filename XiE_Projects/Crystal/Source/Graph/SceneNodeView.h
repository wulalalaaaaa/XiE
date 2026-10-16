#pragma once

#include "Model/CrystalModels.h"

namespace Crystal {
struct SceneNodeView {
    const CrystalEntity* entity = nullptr;
    const EntityView* view = nullptr;
    const SceneLockModel* lock = nullptr;
};
}
