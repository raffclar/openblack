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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// How a creature aims at a villager or an animal that moves, when it walks up to it and when it reaches for it. The
/// creature aims at the place where the living thing will be after a time, not at the place where it is now.
namespace openblack::creature_lead
{

/// A villager or an animal as a thing that a creature aims at
struct Living
{
	/// Where it is, in 16.16 map units
	glm::ivec2 mapPosition {0};
	/// The height of the place where it is. The lead does not change the height.
	float height {0.0f};
	/// Its speed as the game keeps it: 655.36 units are one metre a second
	uint16_t speed {0};
	/// The way that it faces, in game angles
	uint16_t gameAngle {0};
	/// Whether its last turn moved it across the land. The creature aims ahead only of a living thing that moved.
	bool moved {false};
};

/// A living thing slower than this, in metres a second, stays where it is in the lead
constexpr float k_LeastLeadSpeed = 0.1f;
/// The game's speed of a living thing in metres a second, from the speed that it keeps
[[nodiscard]] float SpeedOf(uint16_t speed);
/// Where a living thing is now: its map position in metres, at its height
[[nodiscard]] glm::vec3 PlaceOf(const Living& living);
/// Where a living thing will be after some seconds. It continues along the way that it faces at its speed. A thing
/// that is not faster than k_LeastLeadSpeed stays where it is.
[[nodiscard]] glm::vec3 WhereAfter(const Living& living, float seconds);

/// A creature changes its speed by this many units a second in each second
constexpr float k_Acceleration = 12.0f;
/// The time of a walk counts no distance more than this many times the creature's size
constexpr float k_MostWalkPerSize = 30.0f;

/// The creature that walks up to the living thing
struct Walker
{
	/// Where its body is
	glm::vec3 position {0.0f};
	/// Its size, which limits the distance in the time of the walk
	float size {1.0f};
	/// Its top speed and the fraction of it that it walks at, in units a second
	float runSpeed {0.0f};
	float fraction {0.0f};
};
/// The place that a creature walks to, to get a living thing in reach:
/// 1. The distance to the living thing, not more than k_MostWalkPerSize times the size, gives a time for the walk.
/// 2. The time is the time to go that distance at the speed, less the distance to stop, plus the time to stop.
/// 3. The place of the thing after that time gives a second distance and a second time.
/// 4. The creature walks to the place of the thing after the second time. The stop distance stays from step 2.
[[nodiscard]] glm::vec3 WalkTo(const Walker& walker, const Living& living);

/// How many seconds the hand or foot takes to get to a living thing: the milliseconds of the reach, times the square
/// root of twice the creature's size, over one thousand
[[nodiscard]] float ReachSeconds(float size, int32_t milliseconds);

} // namespace openblack::creature_lead
