/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellSeedArchetype.h"

#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity SpellSeedArchetype::Create(const glm::vec3& position, SpellSeedType seedType, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// a spell seed is an object: it takes the next creation index
	ecs::object_index::Assign(entity);
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(scale));
	// the seed info's mesh
	const auto& info = magic::GetSpellSeedInfo(Locator::infoConstants::value(), seedType);
	registry.Assign<Mesh>(entity, resources::HashIdentifier(info.mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
	return entity;
}
