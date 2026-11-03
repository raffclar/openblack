/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptDestination.h"

#include "ECS/Components/Animal.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;

std::optional<map_coords::MapCoords> ecs::script_destination::Of(const Registry& registry, entt::entity thing)
{
	if (thing == entt::null || !registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (const auto* locomotion = registry.TryGet<const CreatureLocomotion>(thing))
	{
		return locomotion->scriptDestination;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		return map_coords::MapCoords {.x = animal->move.goal.x, .z = animal->move.goal.y, .altitude = animal->goalHeight};
	}
	if (const auto* walk = registry.TryGet<const WallHug>(thing); walk != nullptr && registry.AllOf<Villager>(thing))
	{
		return map_coords::MapCoords {.x = walk->goalPlace.x, .z = walk->goalPlace.y, .altitude = 0.0f};
	}
	return std::nullopt;
}
