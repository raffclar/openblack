/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerFlock.h"

#include <LNDFile.h>
#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ScriptFlock.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFlockRules.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerHome.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_flock = openblack::ecs::villager_flock;
namespace rules = openblack::ecs::script_flock_rules;
using map_coords::MapCoords;

namespace
{
auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

constexpr auto Bit(CollideType type)
{
	return static_cast<uint32_t>(type);
}
} // namespace

uint32_t villager_flock::CellCollision(const MapCoords& coords)
{
	if (!map_coords::InBounds(coords))
	{
		return Bit(CollideType::Edge);
	}
	const auto cell = map_coords::Cell(coords);
	uint32_t collision = Bit(CollideType::Water);
	if (Locator::terrainSystem::has_value())
	{
		const auto* land = Locator::terrainSystem::value().FindCell(glm::u16vec2(cell));
		// A cell of no block counts as water
		if (land != nullptr && land->properties.hasWater == 0)
		{
			collision = Bit(CollideType::Land);
		}
	}
	if (Locator::entitiesMap::has_value())
	{
		const auto& registry = Entities();
		for (const auto thing : Locator::entitiesMap::value().GetAllInCell(cell))
		{
			if (!registry.Valid(thing))
			{
				continue;
			}
			if (registry.AllOf<Tree>(thing))
			{
				collision |= Bit(CollideType::Tree);
			}
			else if (registry.AllOf<Field>(thing))
			{
				collision |= Bit(CollideType::Field);
			}
		}
	}
	return collision;
}

bool villager_flock::CanStandAt(entt::entity villager, const MapCoords& coords)
{
	if (!map_coords::InBounds(coords))
	{
		return false;
	}
	const auto* person = Entities().TryGet<const Villager>(villager);
	if (person == nullptr || !Locator::infoConstants::has_value())
	{
		return true;
	}
	const auto& info =
	    Locator::infoConstants::value().villager.at(static_cast<size_t>(GVillagerInfo::Find(person->tribe, person->number)));
	return (CellCollision(coords) & static_cast<uint32_t>(info.collideType)) == 0;
}

uint32_t villager_flock::MoveInFlock(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto flock = script_flocks::FlockOf(registry, villager);
	if (flock == entt::null)
	{
		return 1;
	}
	const auto& data = registry.Get<const ScriptFlock>(flock);
	const auto leader = script_flocks::Leader(registry, flock);
	const rules::FlockView view {.place = data.place,
	                             .domainRadius = data.domainRadius,
	                             .flockDistance = data.flockDistance,
	                             .isLeader = leader == villager,
	                             .leader = script_flocks::Position(registry, flock)};
	const auto random = [](float limit) { return Locator::gameRandom::value().GameFloatRand(limit); };
	const auto accepts = [villager](const MapCoords& coords) { return CanStandAt(villager, coords); };
	const auto step = rules::MoveInFlock(script_flocks::PositionOf(registry, villager), view, random, accepts);
	if (step.goal.has_value())
	{
		// It walks there, and keeps up with its flock again on arrival
		if (Locator::livingActionSystem::value().VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos,
		                                                                               VillagerStates::MoveInFlock))
		{
			villager_home::SetupMobileMoveTo(action, map_coords::ToMetres(*step.goal), VillagerStates::MoveInFlock);
		}
	}
	return step.result;
}
