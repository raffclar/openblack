/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Transform.h"
#include "ECS/Flocks.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

/// An animal the scripts let go (ECS/ScriptHeld.h).
namespace openblack::ecs::animal_ai
{
using components::Animal;
using components::AnimalBrain;
using components::Flock;
using components::Life;
using components::Transform;

void ReleaseFromScript(entt::entity entity)
{
	auto* brain = detail::BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	// The WANDER script state is the caller's: script_held::ReleaseScriptThingIntoTheGame (case 2) sets it before
	// this. Nothing while it is in the physics or in the hand (openblack: the physics component or the IN_HAND / FLYING
	// states [approximate]) or while the game flag that makes the scripts skip their look-in is set (openblack has no
	// such flag, so it is never set: not tested)
	const auto top = static_cast<AnimalState>(brain->topState);
	if (physics::PhysicsObjects::IsFlying(entity) || top == AnimalState::InHand || top == AnimalState::Flying)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& animal = registry.Get<Animal>(entity);
	const auto& info = detail::InfoOf(animal);
	if (detail::FlockOf(animal) == nullptr)
	{
		// Its own flock, radius domainRadius, distance (int)flockDistance
		// maxMembers is not written for a flock made for one animal, as PlaceInHand's own flock
		const auto flockEntity = flocks::CreateFor(entity);
		if (auto* flock = registry.TryGet<Flock>(flockEntity); flock != nullptr)
		{
			flock->domainRadius = static_cast<uint16_t>(info.domainRadius);
			flock->flockDistance = static_cast<uint16_t>(static_cast<int32_t>(info.flockDistance));
		}
	}
	const auto* life = registry.TryGet<const Life>(entity);
	if (life != nullptr && life->value <= 0.0f)
	{
		// Dying; one that was dying already (status & 1) goes straight to DEAD
		const bool wasDying = (brain->status & 1) != 0;
		detail::SetDying(entity, *brain);
		if (wasDying)
		{
			detail::SetTopState(entity, *brain, AnimalState::Dead);
		}
		return;
	}
	// InteractDecideWhatToDo, at once
	detail::Context ctx {entity, animal, *brain, registry.Get<Transform>(entity), info};
	detail::InteractDecideWhatToDo(ctx);
}

} // namespace openblack::ecs::animal_ai
