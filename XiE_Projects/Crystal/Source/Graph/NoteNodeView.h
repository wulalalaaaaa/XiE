#pragma once

#include "Model/CrystalModels.h"

namespace Crystal {
struct NoteNodeView {
    const CrystalEntity* entity = nullptr;
    const EntityView* view = nullptr;
};
}
