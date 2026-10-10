/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <memory>
#include <optional>

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

/// Taking a creature out of the game for good, as the game does when a creature is deleted. Everything that holds the
/// creature lets go of it first, in the game's order: the hand and any fight (which the game ends before it gets
/// there), the leash, the miracles and sounds it has going, the player's list of creatures, the reactions it set off,
/// what it was doing and carrying, the view that follows it, and last its physics, its place on the map and the
/// creature itself. When it was its player's primary creature, the next one they got becomes the primary one and may
/// take the leash.
namespace openblack::ecs::creature_removal
{

/// The game's side of each step, which the game goes through its systems for and tests fake
class WorldInterface
{
public:
	virtual ~WorldInterface() = default;
	/// The hand lets go of the creature if it holds it, or stops stroking it if it is on it
	virtual void LetGoFromHand(entt::entity creature) = 0;
	/// It leaves its fight at once, letting go of its arena; its opponent's fight ends as one given up does
	virtual void EndFight(entt::entity creature) = 0;
	/// Its leash comes off, and it stops being the creature its owner leads
	virtual void TakeOffLeash(entt::entity creature) = 0;
	/// The miracles it is casting stop and its beams go; its sounds stop
	virtual void ReleaseEffects(entt::entity creature) = 0;
	/// The reactions it set off in others end
	virtual void RemoveReactions(entt::entity creature) = 0;
	/// It stops what it is doing and where it is walking, and drops what it carries
	virtual void StopActing(entt::entity creature) = 0;
	/// The camera that follows it lets go of it
	virtual void LeaveView(entt::entity creature) = 0;
	/// Out of the physics, its fire and its place on the map, and gone from the registry
	virtual void RemoveFromWorld(entt::entity creature) = 0;
	/// A creature that is now its player's primary one may take the leash, if the player leads none
	virtual void ClaimLeash(entt::entity creature) = 0;
};

/// What a removal did
struct Removed
{
	/// Whose it was
	PlayerNames owner {PlayerNames::PLAYER_ONE};
	/// Whether it was its owner's primary creature, and the one that is now, if any
	bool wasPrimary {false};
	std::optional<entt::entity> newPrimary;
	/// The other things that pointed at it and were made to let go: markers, carried things, creatures following it
	size_t referencesCleared {0};
};

/// The player's creature lists forget it. The owner's primary creature after, if any
std::optional<entt::entity> ForgetInPlayerLists(Registry& registry, entt::entity creature, PlayerNames owner);

/// What else points at the creature lets go of it: the leash's order markers, the things it holds, creatures following
/// it, a hand resting on it. How many did
size_t ForgetReferences(Registry& registry, entt::entity creature);

/// Takes the creature out of the game through every step, in order. None when it isn't a creature
std::optional<Removed> Remove(Registry& registry, entt::entity creature, WorldInterface& world);

/// The game's steps, through its systems
[[nodiscard]] std::unique_ptr<WorldInterface> MakeGameWorld();

/// Takes a creature out of the game it is in, through the game's systems
std::optional<Removed> RemoveFromGame(entt::entity creature);

} // namespace openblack::ecs::creature_removal
