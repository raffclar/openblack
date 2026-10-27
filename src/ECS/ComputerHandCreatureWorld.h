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

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::computer_hand
{

/// The walk speed and the run speed of a creature, as parts of its top speed
struct CreatureSpeeds
{
	float walk {0.0f};
	float run {0.0f};
};

/// What the hand reads and changes in the game when the god gives orders to its creature, plays with it or leashes it.
/// The steps of the creature add their calls here.
class CreatureWorld
{
public:
	virtual ~CreatureWorld() = default;
	/// The creature does an action of the game's table to a thing, as an order of its god. It comes across the thing
	/// first, and a desire that was made dominant ends. False when the thing that gets the order is not a creature.
	virtual bool ForceAction(entt::entity creature, uint32_t action, entt::entity object) = 0;
	/// The creature still does a plan that was forced on it
	[[nodiscard]] virtual bool PlanForced(entt::entity creature) const = 0;
	/// What the creature does stops as a failure, and the desire becomes its dominant desire for the seconds. Its
	/// bodily needs are not held down.
	virtual void ForceDominantDesire(entt::entity creature, int32_t desire, float seconds) = 0;
	/// The walk and run speeds of the creature, none when the thing is not a creature
	[[nodiscard]] virtual std::optional<CreatureSpeeds> SpeedsOf(entt::entity creature) const = 0;
	/// The game draws a random number for a short journey of the creature, and does not use it
	virtual void DrawForShortJourney() = 0;
	/// The creature keeps this part of its top speed for its next walks, from 0 to 1
	virtual void SetRequiredSpeed(entt::entity creature, float speed) = 0;
	/// The creature belongs to a player, so that the leash of the player can go on it
	[[nodiscard]] virtual bool HasPlayer(entt::entity creature) const = 0;
	/// The god picks the type of the leash of a creature that belongs to a player
	virtual void SetLeashType(entt::entity creature, LeashType type) = 0;
	/// A leash on the creature that is tied to a thing comes off
	virtual void TakeOffTiedLeash(entt::entity creature) = 0;
	/// The leash of the player goes on the creature, and the god ties it to the thing. False when the thing has no place.
	virtual bool TieLeash(entt::entity creature, entt::entity object) = 0;
	/// The creature feels a stroke or a slap of its god, from -1 (a slap) to 1 (a stroke)
	virtual void Feedback(entt::entity creature, float feedback) = 0;
	/// The game draws a random number for the seconds that the hand stays after a stroke or a slap
	[[nodiscard]] virtual float DrawFeedbackSeconds() = 0;
	/// At the start of a cast of the miracle, the god can let go of the thing that the leash of its creature is tied to.
	/// The hand is at the map position.
	virtual void LetGoForCast(MagicType type, const map_coords::MapCoords& hand) = 0;
};

} // namespace openblack::computer_hand
