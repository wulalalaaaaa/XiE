#include "World2D.h"

namespace Engine::Runtime2D {

EntityId World2D::CreateEntity(const Entity2D& templateData) {
    Entity2D entity = templateData;
    entity.id = m_NextId++;
    m_Entities.push_back(entity);
    return entity.id;
}

Entity2D* World2D::FindEntity(EntityId id) {
    for (Entity2D& entity : m_Entities) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

const Entity2D* World2D::FindEntity(EntityId id) const {
    for (const Entity2D& entity : m_Entities) {
        if (entity.id == id) {
            return &entity;
        }
    }
    return nullptr;
}

void World2D::Clear() {
    m_Entities.clear();
    m_Player = kInvalidEntityId;
    m_NextId = 1;
}

void World2D::SetPlayer(EntityId id) {
    m_Player = id;
}

EntityId World2D::GetPlayer() const {
    return m_Player;
}

std::vector<Entity2D>& World2D::Entities() {
    return m_Entities;
}

const std::vector<Entity2D>& World2D::Entities() const {
    return m_Entities;
}

} // namespace Engine::Runtime2D
