/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

/// A fish farm: a stretch of sea a town fishes, full of fish when made, whose fish come back slowly once taken
namespace openblack::fish_farm
{

/// How far it reaches across the land, and how high over it the hand stays while it scoops fish out of it
inline constexpr float k_Radius = 5.0f;
inline constexpr float k_HandHeight = 5.0f;

/// What a fish farm's row of the tables says
struct Type
{
	/// The food a full farm holds, and the kind of food it is
	float foodValue {0.0f};
	uint32_t foodType {0};
	/// A fish comes back once in every so many game turns
	uint32_t turnsPerFish {0};
};

/// How many fish a full farm of its type holds: its food, for a type of food
[[nodiscard]] float Full(const Type& type);

/// A game turn: on every so many a fish comes back, never beyond full
[[nodiscard]] float Grow(float fish, uint32_t turn, const Type& type);

/// Some fish are taken out of a farm: as many as it has; what was taken
uint32_t Take(float& fish, uint32_t wanted);

/// The hand's first handful of a scoop: the handful's first scoop, no more than a full farm of its type holds. It takes
/// nothing out of the farm.
[[nodiscard]] uint32_t FirstHandful(uint32_t initialScoop, const Type& type);
/// A game turn of the hand scooping fish: the ramp's amount, no more than a full farm holds nor than the handful has room
/// for (when it has a limit)
[[nodiscard]] uint32_t ScoopWanted(uint32_t ramp, const Type& type, uint32_t held, uint32_t maxPickedUp);

} // namespace openblack::fish_farm
