/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerWorshipCheck.h"

#include "ECS/Systems/Implementations/VillagerWorship.h"

using namespace openblack::ecs;
using namespace openblack::ecs::systems;

bool VillagerWorshipCheck::WorshipCheck(entt::entity villager)
{
	return villager_worship::CheckNeededForWorship(villager);
}
