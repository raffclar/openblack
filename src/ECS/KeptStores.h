/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/MapCellResident.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/ResourcePile.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

/// The stores of the components that the rules of life and the walks test for each thing in each turn. The registry
/// makes these stores and this set as the registry is made. The stores stay until the registry goes, so the set gives
/// the same answers as the registry with no search of the registry. Nothing changes the set after it is made, so
/// other threads can read it too.
struct KeptStores
{
	explicit KeptStores(const Registry& registry)
	    : life(registry)
	    , walk(registry)
	{
	}

	/// The kinds that make the life of a villager follow another rule than the rule of a villager
	using Life =
	    ComponentLookups<components::Villager, components::VillageTotem, components::Abode, components::Field,
	                     components::ResourcePile, components::CreatureNeeds, components::BuildProgress,
	                     components::WorshipAltar, components::Animal, components::ObjectLife, components::Indestructible>;
	/// The kinds that a walker's state reads
	using Walk =
	    ComponentLookups<components::TurningWalker, components::MoveStateLinearTag, components::MoveStateOrbitTag,
	                     components::MoveStateExitCircleTag, components::MoveStateStepThroughTag,
	                     components::MoveStateFinalStepTag, components::MoveStateArrivedTag, components::MapCellResident>;

	/// Make the stores of all the kinds in the set, so that the set finds each of them
	static void MakeStores(entt::registry& registry);

	Life life;
	Walk walk;
};

} // namespace openblack::ecs
