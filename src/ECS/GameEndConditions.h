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
#include <span>
#include <string_view>

#include "ECS/Components/GameStanding.h"
#include "ECS/GameEndRules.h"
#include "Enums.h"

// The winning conditions of a network game: how the world counts them, and the time limit of the game.
//
// Each condition counts only in a network game, and only for a player that has the condition. Some conditions go up when
// a thing happens in the world: a villager dies, a building is finished, a town changes hands. Other conditions take a
// value before each player's turn: the food and the wood in the stores, the prayer power and the belief.
//
// Pure, tested with fakes.

namespace openblack::game_end
{

/// The time limit of a network game in minutes, when the host does not change it
inline constexpr uint32_t k_DefaultTimeLimitMinutes = 30;
/// The game turns in one minute of a time limit
inline constexpr uint32_t k_TurnsPerMinute = 600;
/// The shortest time limit in minutes that the host can give a network game
inline constexpr uint32_t k_ShortestTimeLimitMinutes = 10;
/// The longest time limit in minutes that the host can give a network game: 24 hours
inline constexpr uint32_t k_LongestTimeLimitMinutes = 1440;
/// The shortest time limit in minutes that a build with the developer tools accepts, for short test games
inline constexpr uint32_t k_ShortestTestTimeLimitMinutes = 1;

/// Why the operator of a game server cannot give a network game this time limit in minutes, or none when the operator
/// can. The limits are the limits of the host's controls. A build with the developer tools also accepts a shorter limit,
/// for short test games.
[[nodiscard]] constexpr std::optional<std::string_view> TimeLimitRefusal(uint32_t minutes, bool developerTools) noexcept
{
	if (minutes > k_LongestTimeLimitMinutes)
	{
		return "the time limit must be 1440 minutes (24 hours) or less";
	}
	const auto shortest = developerTools ? k_ShortestTestTimeLimitMinutes : k_ShortestTimeLimitMinutes;
	if (minutes < shortest)
	{
		return developerTools ? "the time limit must be 1 minute or more" : "the time limit must be 10 minutes or more";
	}
	return std::nullopt;
}

/// The turn at which the time limit of a network game ends the game. The start multiplies the minutes with no test.
[[nodiscard]] constexpr uint32_t TimeLimitTurn(uint32_t minutes)
{
	return minutes * k_TurnsPerMinute;
}

/// A winning condition of a network game that the host picks, and the value that a player must get to
struct ConditionPick
{
	Condition condition {Condition::VillagersKilled};
	int32_t value {0};

	[[nodiscard]] bool operator==(const ConditionPick&) const = default;
};

/// A winning condition that the host switches on. With no value, the condition keeps the first value that the land gives
/// it, as when the host only checks the box of the condition.
struct SwitchedCondition
{
	Condition condition {Condition::VillagersKilled};
	std::optional<int32_t> value;

	[[nodiscard]] bool operator==(const SwitchedCondition&) const = default;
};

/// At the start of a network game, the player gets each condition that the host picked, with its value as the target.
/// The player starts each condition from 0.
void GiveConditions(ecs::components::GameStanding& standing, std::span<const ConditionPick> picks);

/// A condition of the player goes up by an amount, when the player has the condition
void AddToCondition(ecs::components::GameStanding& standing, Condition condition, int32_t amount);
/// A condition of the player takes a value, when the player has the condition
void SetCondition(ecs::components::GameStanding& standing, Condition condition, int32_t value);
/// The player has the condition
[[nodiscard]] bool HasCondition(const ecs::components::GameStanding& standing, Condition condition);

/// A death of a villager counts as a villager killed by the killer. Deaths of hunger, of age, of tiredness, by an animal,
/// at a chant and with no cause do not count.
[[nodiscard]] bool CountsAsKilled(DeathReason reason);

/// A building that loses life
struct BuildingHarm
{
	float lifeBefore {1.0f};
	float lifeAfter {1.0f};
	/// The life under which the building stops working
	float stopsWorkingAt {0.0f};
	/// A player did the harm
	bool byPlayer {false};
	/// The age of the building, which stops at k_SettledAge
	uint32_t age {0};
	/// A creature did the harm with a kick, or with a thing that a creature threw
	bool byCreature {false};
};
/// The age at which a building that stops working counts as smashed
inline constexpr uint32_t k_SettledAge = 200;
/// The condition that a harm to a building counts for its player, if any:
///
/// - A building that keeps all its life counts nothing.
/// - A building that stops working counts when a player did it and the building is settled.
/// - Another harm counts when the building had all its life before.
///
/// A harm that a creature did counts as smashed by a creature, any other harm as smashed by the player.
[[nodiscard]] std::optional<Condition> SmashedCondition(const BuildingHarm& harm);

/// A building of a town, for the count of new towns
struct TownBuilding
{
	AbodeType type {AbodeType::General};
	/// The building is all built
	bool built {true};
};
/// A building that starts to work completes a new town when the town did not have a storage pit, a town centre and a
/// house before, and has all three now. Of the other buildings, only those that are all built count.
[[nodiscard]] bool CompletesNewTown(std::span<const TownBuilding> others, AbodeType starts);

/// The amount in the stores: the sum of the amounts of the piles
[[nodiscard]] int32_t AmountInStores(std::span<const uint32_t> piles);
/// The prayer power of a player: the sum of what each worship site of its temple has available, made a whole number
[[nodiscard]] int32_t PrayerPower(std::span<const float> sites);
/// The belief of a player: for each of its towns, the town's belief in the player times 1000 made a whole number, and
/// the sum made a whole number
[[nodiscard]] int32_t BeliefInWorld(std::span<const float> towns);

} // namespace openblack::game_end
