#include <vector>

#include "EntityManager.h"

#include <Entities/Components/Component.h>

namespace OpenBlack::Entities
{
void EntityManager::Update()
{
	RemoveInactive();
}

void EntityManager::RemoveInactive()
{
	std::vector<EntityId> to_remove;

	for (const auto& kv : _entities)
	{
		auto id     = kv.first;
		auto entity = kv.second;

		if (!entity.active)
		{
			to_remove.push_back(id);
		}
	}

	for (const auto& id : to_remove)
	{
		_entities.erase(id);
		_villagerComponents.erase(id);
	}
}

void EntityManager::UpdateVillagers()
{
	for (const auto& kv : _villagerComponents)
	{
		auto com = kv.second;
		// Update age
		// Update health
		// Update hunger
		// Update task
	}
}

void EntityManager::UpdateBuildings()
{
	for (const auto& kv : _buildingComponents)
	{
		auto com = kv.second;
	}
}

} // namespace OpenBlack::Entities
