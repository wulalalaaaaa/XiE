#pragma once

#include "Model/CrystalModels.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace Crystal {

// Host-independent drafts shared by the HUD and future native UI widgets.
// They keep query/schema editing deterministic and make preview results use
// the same predicate semantics as the workspace refresh path.
struct QueryBuilderDraft {
    std::string entitySchema;
    std::vector<QueryFilterModel> filters;
    int resultLimit = 50;

    [[nodiscard]] QueryViewModel ToModel(
        std::string projectId, std::string spaceId, std::string name = "Query") const {
        QueryViewModel result;
        result.projectId = std::move(projectId);
        result.spaceId = std::move(spaceId);
        result.name = std::move(name);
        result.entitySchema = entitySchema;
        result.filters = filters;
        result.resultLimit = std::clamp(resultLimit, 1, 500);
        return result;
    }
};

struct SchemaEditorDraft {
    CrystalEntitySchema schema;
    std::string error;

    bool SetPropertyType(
        const std::string& propertyId, PropertyType type, bool valuesInUse) {
        auto found = std::find_if(schema.properties.begin(), schema.properties.end(),
            [&](const CrystalPropertyDefinition& property) { return property.id == propertyId; });
        if (found == schema.properties.end()) {
            error = "property_not_found";
            return false;
        }
        if (valuesInUse) {
            error = "property_type_in_use";
            return false;
        }
        found->type = type;
        error.clear();
        return true;
    }

    bool RemoveEnumOptions(
        const std::string& propertyId, const std::vector<std::string>& usedOptions) {
        auto found = std::find_if(schema.properties.begin(), schema.properties.end(),
            [&](const CrystalPropertyDefinition& property) { return property.id == propertyId; });
        if (found == schema.properties.end()) {
            error = "property_not_found";
            return false;
        }
        for (const std::string& option : usedOptions) {
            if (std::find(found->options.begin(), found->options.end(), option) != found->options.end()) {
                error = "property_option_in_use";
                return false;
            }
        }
        found->options.erase(std::remove_if(found->options.begin(), found->options.end(),
            [&](const std::string& option) {
                return std::find(usedOptions.begin(), usedOptions.end(), option) == usedOptions.end();
            }), found->options.end());
        error.clear();
        return true;
    }
};

} // namespace Crystal
