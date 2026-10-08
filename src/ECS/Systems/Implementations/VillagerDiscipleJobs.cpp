/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerDiscipleJobs.h"

#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerFisherman.h"
#include "ECS/Villager/VillagerSatisfy.h"

using namespace openblack::ecs;
using namespace openblack::ecs::systems;

uint32_t VillagerDiscipleJobs::DiscipleJob(entt::entity villager, uint8_t disciple)
{
	switch (disciple)
	{
	case 2: // forester
		return villager::CheckSatisfyWoodDesire(villager);
	case 3: // fisherman
		return villager::FishermanLookForWater(villager);
	case 4: // builder
		return villager::CheckNeededForBuilding(villager);
	case 5: // breeder
		return villager::SetupBreederDisciple(villager);
	case 8: // craftsman
		return villager::CheckSatisfySupplyWorkshop(villager);
	case 9: // trader
		return villager::CheckTrader(villager);
	default:
		return 0;
	}
}
