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
#include <cstdint>

#include <optional>
#include <span>

/// The rules by which a computer god attacks another player: the thing in a town that it wants to destroy, the creature
/// that it wants to attack, and the rock that it throws. Pure functions of what the god sees.
namespace openblack::computer_mind_rules
{

/// Nearer than this, in metres, the god fully wants to attack a creature
constexpr float k_AttackCreatureNear = 100.0f;
/// The distance in metres at which the god no longer wants to attack a creature
constexpr float k_AttackCreatureReach = 500.0f;
/// How much the god wants to attack the creature of another player, from the distance from its hand to the creature
/// and the nearness of the creature on the scale of the reach
[[nodiscard]] float AttackCreatureWish(float distance, float nearness);

/// The distance in metres at which a building or a field of a town no longer draws an attack
constexpr float k_TownTargetReach = 1000.0f;
/// How much the god wants to destroy a building: how much of it stands, its life and how much the game wants computer
/// gods to attack its kind
[[nodiscard]] float BuildingTargetValue(float built, float life, float attackDesire);
/// How much the god wants to destroy a field: the share of the food that it holds and how much the game wants computer
/// gods to attack fields
[[nodiscard]] float FieldTargetValue(float food, float totalFood, float attackDesire);
/// The value of a target in a town when the god knows the place of its hand: the value times the nearness of the target
[[nodiscard]] float NearTargetValue(float value, float nearness);
/// The target that the god wants most: the first of the largest values above 0. None when no value is above 0.
[[nodiscard]] std::optional<size_t> MostWantedTarget(std::span<const float> values);

/// The god casts the angry miracle at the creature at least this many times
constexpr uint32_t k_LeastAttackCasts = 2;
/// The number of casts that the god adds at random: 0 or 1
constexpr uint32_t k_AttackCastRange = 2;

/// The radius around the hand, in metres, in which the god looks for a rock to throw at a creature
constexpr float k_RockSearchRadius = 250.0f;
/// What the god knows of a thing that it can throw at a creature
struct RockFacts
{
	/// The player can throw the thing
	bool throwable {false};
	/// The thing knocks down buildings when it hits them
	bool destroysBuildings {false};
	/// The thing is a standing tree
	bool tree {false};
	/// The weight of the thing as the physics weighs it
	float weight {0.0f};
};
/// The score of a thing that the god can throw at a creature: its weight when the player can throw it and it knocks
/// down buildings or is a tree, else 0
[[nodiscard]] float RockScore(const RockFacts& facts);

} // namespace openblack::computer_mind_rules
