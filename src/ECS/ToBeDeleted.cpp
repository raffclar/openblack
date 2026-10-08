/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ToBeDeleted.h"

#include <cstdio>
#include <cstdlib>

#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/MapCells.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/ToBeDeletedSystemInterface.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerDeath.h"
#include "Locator.h"

namespace openblack::ecs
{
using namespace components;

namespace
{
/// The game's dead list; stops with a message when there is none (before the game or after it has gone)
systems::ToBeDeletedSystemInterface& DeadList()
{
	if (!Locator::toBeDeletedSystem::has_value())
	{
		std::fputs("ecs::ToBeDeleted: no dead list in the locator (Locator::toBeDeletedSystem)\n", stderr);
		std::abort();
	}
	return Locator::toBeDeletedSystem::value();
}

/// The object's fire ToBeDeleted(0), then unlinked (fire::ToBeDeleted unlinks it)
void DeleteFire(entt::entity entity)
{
	if (auto* effect = fire::Find(entity))
	{
		fire::ToBeDeleted(*effect);
	}
}
} // namespace

void ToBeDeleted(entt::entity entity, bool now)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return;
	}
	// already unavailable, nothing (it is on the list)
	if (registry.AllOf<Unavailable>(entity))
	{
		return;
	}
	if (registry.AnyOf<Tree, DeadTree>(entity))
	{
		// a tree or a dead tree: out of its forest, the deletion listeners told
		DeleteTree(entity);
		// then its fire, after it
		DeleteFire(entity);
		return;
	}
	if (registry.AllOf<Villager>(entity))
	{
		// a villager (ECS/Villager/VillagerDeath.h), when it is marked: its dependants (SET_DYING through the real exits, a
		// mother's orphans, out of its abode / town / the vagrants), then it stops reacting (its reaction, the mourning too)
		ecs::villager::ToBeDeletedOverride(entity);
	}
	if (registry.AnyOf<Villager, Animal>(entity))
	{
		// out of its town's list, its town cleared, then out of its flock; prey and hunter links stay (their readers ask
		// IsAvailable). (pending) the town list and the town reset
		animal_ai::Forget(entity);
	}
	if (registry.AllOf<ScriptHighlight>(entity))
	{
		// out of the highlights' list, its two effects closed
		script_highlight::OnToBeDeleted(entity);
	}
	// the buildings' part (ECS/Abodes.h): before the physics, the cells and the mark, which the original sets last, so
	// the searches it makes still see the building; others ignored
	abodes::OnToBeDeleted(entity);
	// (openblack, guard) the class part may have deleted this entity already: a building site's builders' exits
	// re-enter ToBeDeleted for it (BuildingSites.cpp step 4) and the inner call finished it (mark or Destroy, and its
	// DeleteFire), so this one stops here
	if (!ecs::IsAvailable(entity))
	{
		return;
	}
	if (now || !DeadList().Deferred())
	{
		// `now` (deleted at once), or the deferral still off: out of the physics first, as
		// PhysicsObjects::RemoveObject puts a still valid entity back in its cells; then out of its cells
		physics::PhysicsObjects::RemoveObject(entity);
		map_cells::RemoveMapObject(entity);
		registry.Destroy(entity);
		registry.SetDirty();
		// its fire after it, as when FireEffect's Process found the object gone (no EndOnFire on it)
		DeleteFire(entity);
		return;
	}
	// out of its cells
	map_cells::RemoveMapObject(entity);
	// marked unavailable, not yet passed, pushed at the head. The physics body stays: the original does not take it out
	// here; the physics drops the unavailable ones at the start of the next turn
	registry.Assign<Unavailable>(entity);
	DeadList().Push(entity);
	DeleteFire(entity);
}

bool IsAvailable(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) && !registry.AllOf<Unavailable>(entity);
}

void ProcessDeadList(bool drain)
{
	DeadList().Process(drain);
}

void SetDeferredDeletion(bool deferred)
{
	DeadList().SetDeferred(deferred);
}

bool DeferredDeletion()
{
	return DeadList().Deferred();
}

} // namespace openblack::ecs
