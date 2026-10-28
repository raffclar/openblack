/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/VillagerScriptRules.h"

namespace openblack::ecs::script_objects
{

/// The kinds of object a script lets go of differently
enum class Kind : uint8_t
{
	Other,
	/// A villager or a villager's child
	Villager,
	Animal,
	Creature,
};

/// What the scripts' object table needs of the world: the objects, their script flags, and how each kind goes back into
/// the game. The game's own world works on the entities; tests give a fake.
class World
{
public:
	virtual ~World() = default;

	/// The object is still in the world
	[[nodiscard]] virtual bool Exists(entt::entity object) const = 0;
	/// The object can still be dealt with: in the world, and for a villager not on its way to dying
	[[nodiscard]] virtual bool IsAvailable(entt::entity object) const = 0;
	[[nodiscard]] virtual Kind KindOf(entt::entity object) const = 0;
	/// A script holds a reference to the object
	[[nodiscard]] virtual bool IsInScript(entt::entity object) const = 0;
	virtual void SetInScript(entt::entity object, bool inScript) = 0;
	/// A script made the object: a named villager never shows its name after
	virtual void HideName(entt::entity /*object*/) {}
	/// A script made the object: it never fades out of sight in the distance
	virtual void KeepInView(entt::entity /*object*/) {}
	/// A script controls the object
	[[nodiscard]] virtual bool IsControlled(entt::entity object) const = 0;
	virtual void SetControlled(entt::entity object, bool controlled) = 0;
	/// Flying or lying in the physics
	[[nodiscard]] virtual bool IsInPhysics(entt::entity object) const = 0;
	/// Standing in the map's cells: not held, flying or carried
	[[nodiscard]] virtual bool IsInMap(entt::entity object) const = 0;
	/// A villager is set to decide what to do next as a script sets it: the state it was in is kept as its previous
	/// one, that state is left, and its clip and its wait start afresh
	virtual void SetVillagerDecideWhatToDo(entt::entity villager) = 0;
	/// A villager in the physics goes back to deciding what to do when it is out: its previous state is set to that
	virtual void SetVillagerPreviousDecideWhatToDo(entt::entity villager) = 0;
	/// A living leaves the flock that it is in, and an empty flock stays. A flock that it was the shepherd of then has no
	/// shepherd.
	virtual void LeaveFlock(entt::entity living) = 0;
	/// The facts that decide what a villager does when a script lets it go
	[[nodiscard]] virtual villager_script_rules::ReleaseFacts VillagerReleaseFacts(entt::entity villager) const = 0;
	/// A villager decides at once what to do in its town
	virtual void VillagerDecideNow(entt::entity villager) = 0;
	/// A villager with no town starts to wander as a vagrant
	virtual void VillagerStartVagrant(entt::entity villager) = 0;
	/// A villager dies with no cause and no player to blame. It drops the wood that it shows as a log.
	virtual void VillagerDie(entt::entity villager) = 0;
	/// A creature gives up what it is doing as a failure
	virtual void AbandonCreatureAction(entt::entity creature) = 0;
	/// An animal in the physics goes back to wandering after a reaction: its state before a reaction is set to wandering
	virtual void SetAnimalWanderBeforeReaction(entt::entity animal) = 0;
	/// An animal goes back to its own life: a flock of its own if it has none, then a leg of its own, or its death
	virtual void ReleaseAnimal(entt::entity animal) = 0;
	/// The object goes from the world at once
	virtual void Delete(entt::entity object) = 0;
	/// A flock, a dance or a town: something scripts fill with other things
	[[nodiscard]] virtual bool IsContainer(entt::entity object) const = 0;
	/// Every member leaves a flock or a dance, and one a script controls waits for the script: the members that left. A
	/// town keeps its own (none left). Nothing when the object is none of these
	virtual std::optional<std::vector<entt::entity>> Disband(entt::entity container) = 0;
	/// The members of a flock (none for anything else)
	[[nodiscard]] virtual std::vector<entt::entity> FlockMembers(entt::entity object) const = 0;
	/// A marker or a timer, which has no use outside the script that holds it
	[[nodiscard]] virtual bool IsDeletedWhenReleased(entt::entity object) const = 0;
	/// A scroll or sign a script put up
	[[nodiscard]] virtual bool IsHighlight(entt::entity object) const = 0;
};

} // namespace openblack::ecs::script_objects
