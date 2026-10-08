/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerChildFactory.h"

#include "ECS/Archetypes/VillagerArchetype.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

entt::entity VillagerChildFactory::CreateChild(const glm::vec3& position, VillagerInfo info, uint32_t age)
{
	// The child joins no town or abode here: the birth does it
	return archetypes::VillagerArchetype::Create(position, position, info, age, false);
}
