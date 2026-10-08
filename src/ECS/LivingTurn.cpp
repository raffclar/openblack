/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingTurn.h"

#include <algorithm>

#include <entt/entity/entity.hpp>

#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "Locator.h"

namespace openblack::ecs::living_turn
{
using namespace components;

std::vector<entt::entity> LivingList()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> list;
	// (pending) the creatures: they are livings too, so they are on this list (openblack's CreatureArchetype entities
	// are not); a separate item from the turn's creature part
	registry.Each<const Villager, const LivingAction>([&list](entt::entity entity, const Villager&, const LivingAction&) {
		if (ecs::IsAvailable(entity))
		{
			list.push_back(entity);
		}
	});
	registry.Each<const Animal, const Transform>([&registry, &list](entt::entity entity, const Animal&, const Transform&) {
		// (openblack, guard) an animal is never a villager; one that were would be listed once
		if (ecs::IsAvailable(entity) && !registry.AllOf<Villager, LivingAction>(entity))
		{
			list.push_back(entity);
		}
	});
	// newest first (a new living goes at the head); an entity without an index (-1) at the tail, in
	// the order above (stable)
	std::stable_sort(list.begin(), list.end(),
	                 [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
	return list;
}

void WalkList(const std::vector<entt::entity>& list, const std::function<void(entt::entity)>& run)
{
	// on the list = not unlinked: a living is unlinked when it is marked for deletion (ecs::IsAvailable false), or the
	// entity is gone (openblack's deletion at once while the deferral is off)
	const auto linked = [](entt::entity entity) { return ecs::IsAvailable(entity); };
	size_t i = 0;
	while (i < list.size())
	{
		const auto current = list[i];
		// the next one is read before the turn: the first one after it still on the list
		size_t next = i + 1;
		while (next < list.size() && !linked(list[next]))
		{
			++next;
		}
		run(current);
		if (next < list.size() && !linked(list[next]))
		{
			// unlinked during this turn: the loop holds it and its next is cleared. Still
			// in memory (the dead list) it takes its turn and the loop ends; gone already (the deferral off), it ends now
			if (Locator::entitiesRegistry::value().Valid(list[next]))
			{
				run(list[next]);
			}
			return;
		}
		i = next;
	}
}

void ProcessLiving(float visualTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	const uint32_t turn = villager::CurrentTurn();
	// (openblack) the villagers' test hooks, before the list as LivingActionSystem::Update ran them
	villager::RunDebugHooks(turn);
	const bool animals = animal_ai::BeginAnimalsTurn(visualTime);
	WalkList(LivingList(), [&registry, turn, animals](entt::entity entity) {
		// the position becomes the start of this turn's move (DrawPosition::turnStart). It may assign
		// DrawPosition: never inside a view over DrawPosition; the loop walks its own vector
		BeginLivingTurn(entity);
		if (registry.AllOf<Villager, LivingAction>(entity))
		{
			villager::ProcessReaction(entity);
			// (openblack, guard) deleted at once by its own reaction while the deferral is off; the original's object
			// stays in memory until ProcessDeadList and goes on to its ProcessState. Likewise (guard) an unlinked next
			// that still takes its turn does not move: every PathfindingSystem::Step phase excludes Unavailable, where
			// the original's walk would step it
			if (!registry.Valid(entity))
			{
				return;
			}
			villager::ProcessState(entity, turn);
		}
		else if (animals && registry.AllOf<Animal, Transform>(entity))
		{
			animal_ai::ProcessAnimal(entity); // its reaction and state
		}
	});
	if (animals)
	{
		animal_ai::EndAnimalsTurn();
	}
	// (pending) the original sets a game value to 20 here; its reader is not known
}

void MoveToStep(entt::entity villager)
{
	// (openblack, guard) the unit tests run state functions without the systems
	if (Locator::pathfindingSystem::has_value())
	{
		Locator::pathfindingSystem::value().Step(villager);
	}
}
} // namespace openblack::ecs::living_turn
