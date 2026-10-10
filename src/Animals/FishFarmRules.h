/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>

#include <glm/vec2.hpp>

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
	/// The fishermen it is meant to take
	uint32_t maxFishermen {0};
};

/// How near a farm is to a fisherman counts for less the further it is, out to this many metres
inline constexpr float k_FishermanReach = 500.0f;
/// A fisherman picks his spot within this square round the farm, in metres along each of x and z
inline constexpr float k_FishingSpread = 5.0f;
/// A fisherman's catch is this share of what he can carry, by the season: spring, summer, autumn, winter
inline constexpr float k_CatchShare = 0.25f;
inline constexpr std::array<float, 4> k_SeasonCatch {1.0f, 0.9f, 0.7f, 0.6f};

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

/// How much a town wants a farm fished: what share of its fishermen it still lacks, rounded down, so 1 while it has
/// none and 0 once it has one
[[nodiscard]] uint32_t DesireToBeFished(size_t fishermen, uint32_t maxFishermen);

/// A farm of a town as the town weighs it for a fisherman: how far it is from him, in metres, and its fishermen
struct Candidate
{
	float distance {0.0f};
	size_t fishermen {0};
};
/// The farm a town sends a fisherman to: the best by its wish to be fished times how near it is (least from 500 m), the
/// first of equals; none when no farm wants one
[[nodiscard]] std::optional<size_t> BestFarm(std::span<const Candidate> farms, uint32_t maxFishermen);

/// Where a fisherman stands to fish, in 16.16 map units: the farm's place moved by an offset in metres along x and z
[[nodiscard]] glm::ivec2 FishingSpot(glm::ivec2 farm, glm::vec2 offset);

/// A fisherman's catch as a new fishing animation starts, when the fish bite: a share of what he can carry, less out of
/// season, no more than he has room for, times his tribe's skill, in whole fish. The fish come out of the sea, not the
/// farm. Negative only for a villager already carrying more than he can.
[[nodiscard]] int32_t Catch(uint32_t capacity, uint32_t held, uint32_t season, float tribalPower);
/// Whether, his catch taken, a fisherman goes to put his food in the town's store: once he has less room left than he
/// just caught, or none
[[nodiscard]] bool TakesCatchToStore(uint32_t capacity, uint32_t held, uint32_t caught);

} // namespace openblack::fish_farm
