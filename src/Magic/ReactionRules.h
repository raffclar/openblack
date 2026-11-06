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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

// How the living take up the reactions spread round them: how urgent a reaction is to each by its kind and distance,
// how long they keep reacting and how soon they may react to the same kind again, and when a new reaction takes the
// place of the one a living thing has. Pure, tested with made-up tables.

namespace openblack::magic
{

/// The map's units are this many to a metre
inline constexpr float k_MapUnitsPerMetre = 6553.6f;
/// Within this many map units of a miracle that makes it flee, a living thing flees it more urgently the closer it is
inline constexpr float k_FleeUrgencyUnits = 600000.0f;

/// The quick distance in map units: the greater of the two ways across the land and half the lesser
[[nodiscard]] float FastMapDistance(glm::vec3 a, glm::vec3 b);

/// How urgent a kind of reaction is to a living thing before its distance counts (villager_reaction::Priority scales it
/// by that): a miracle that frightens more urgently the closer it is, by whole steps; a creature takes no notice of a
/// miracle it cast itself, frightening or nice
[[nodiscard]] uint32_t KindPriority(Reaction type, uint32_t tablePriority, float fastDistance, bool isCaster);

/// A living thing reacting to one thing changes to another (of a different kind, or for a few kinds another reaction of
/// the same kind) only when it is more urgent, and only once it has reacted for this many whole seconds: one when what
/// it reacts to now is the hand picking something up
inline constexpr uint32_t k_ReactSecondsBeforeChanging = 10;
inline constexpr uint32_t k_ReactSecondsBeforeChangingForHand = 1;
[[nodiscard]] bool ChangesReaction(Reaction current, uint32_t currentPriority, uint32_t nextPriority, uint32_t secondsReacting);

/// A villager fleeing goes this far at a time
inline constexpr float k_FleeStep = 10.0f;
/// Something whose way points within this of a villager is coming towards it, by the cosine
inline constexpr float k_ComingTowardsCosine = 0.8f;
/// Each way across the land, the point of a flight from something moving moves by a random number up to this, less half
inline constexpr float k_FleeJitter = 8.0f;
/// Whether a thing moves: the length of its velocity is more than 0. The squares add in the order y, z, x
[[nodiscard]] bool Moves(glm::vec3 velocity);
/// The point that a living thing flees to from something that does not move, in metres across the land. The point is the
/// run distance from the thing, straight away from it. The step is the run distance divided by the length, then times
/// each axis. When the two map positions are the same, the point is the position of the living thing
[[nodiscard]] glm::vec2 FleePointFromStill(const map_coords::MapCoords& living, const map_coords::MapCoords& thing, float run);
/// The map position of that point. The position keeps the height of the living thing in the world: its altitude is the
/// height of the living thing over the ground there, less the ground at the point. groundAt gives the ground at a map
/// position
template <typename GroundAt>
[[nodiscard]] map_coords::MapCoords FleePlaceFromStill(const map_coords::MapCoords& living, const map_coords::MapCoords& thing,
                                                       float run, const GroundAt& groundAt)
{
	const float height = static_cast<float>(groundAt(living)) + living.altitude;
	const auto point = FleePointFromStill(living, thing, run);
	map_coords::MapCoords place {map_coords::ToFixed(point.x), map_coords::ToFixed(point.y), 0.0f};
	// The height has no change across the land, but the sum with 0 changes a height of -0 to +0
	place.altitude = (height + 0.0f) - static_cast<float>(groundAt(place));
	return place;
}
/// The map position that a living thing flees to from something that moves: the run distance across the way of the
/// thing, on the side of the living thing. Each way across the land also moves by its random number less half of
/// k_FleeJitter. The metres go back to map units by times 65536 and divided by 10. The altitude stays as it is
[[nodiscard]] map_coords::MapCoords FleePlaceFromMoving(const map_coords::MapCoords& living, const map_coords::MapCoords& thing,
                                                        glm::vec3 velocity, float run, glm::vec2 random);
/// The map position that a living thing flees to, the run distance away. It runs across the way of a thing that moves,
/// with the two random numbers. It runs straight away from a thing that does not move, with no random numbers. The
/// caller gets random numbers only when Moves is true. A position off the map gives no place
template <typename GroundAt>
[[nodiscard]] std::optional<map_coords::MapCoords>
FleePlace(const map_coords::MapCoords& living, const map_coords::MapCoords& thing, glm::vec3 velocity,
          std::optional<glm::vec2> random, float run, const GroundAt& groundAt)
{
	const auto place = random.has_value() ? FleePlaceFromMoving(living, thing, velocity, run, *random)
	                                      : FleePlaceFromStill(living, thing, run, groundAt);
	if (!map_coords::InBounds(place))
	{
		return std::nullopt;
	}
	return place;
}
/// Whether something moving with a velocity comes towards a villager
[[nodiscard]] bool ComingTowards(glm::vec3 villager, glm::vec3 object, glm::vec3 velocity);

/// Whether a kind of reaction ends once its time is up: most do, never fleeing a miracle nor one impressed by a miracle
[[nodiscard]] bool TimesOut(Reaction type);
/// Whether a reaction of a kind has outlived its time: so many turns since it was cast, or since anyone first took it up
[[nodiscard]] bool TimedOut(Reaction type, std::optional<uint32_t> stamp, uint32_t age, uint32_t turns);
/// How far a reaction reaches as it starts: from nothing, for the kinds that grow, else as far as its table says
[[nodiscard]] float StartingReach(bool grows, float maxDistance);

} // namespace openblack::magic
