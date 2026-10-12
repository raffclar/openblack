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

#include <optional>
#include <string>
#include <string_view>

/// The words and numbers of the tooltips the hand shows over things in the world: a village centre's belief and
/// population, a temple's health and way in, a store's food and wood, and the amount a pile gives. Each is one of the
/// game's 170 tooltips (from text 0xE73).
namespace openblack::gui::world_tooltip
{

/// "Enter Temple", over the way into the player's own temple, by the Apply button
constexpr uint32_t k_EnterTemple = 88;
/// "%3.0f Belief Left", over a village centre the player holds the most belief in
constexpr uint32_t k_BeliefLeft = 89;
/// "%3.0f Belief Needed", over a village centre the player does not hold the most belief in
constexpr uint32_t k_BeliefNeeded = 109;
/// "Amount: %3.0f", forced as the hand takes food or wood from a pile
constexpr uint32_t k_HeldAmount = 119;
/// "Temple Health: %3.0f%%"
constexpr uint32_t k_TempleHealth = 117;
/// "Stored - Food: %3.0f Wood: %3.0f", over a village store
constexpr uint32_t k_Stored = 134;
/// "Population: %3.0f", written as "Population:  12/34": its people over the room of its homes
constexpr uint32_t k_Population = 137;
/// "Food Amount: %3.0f" and "Wood Amount: %3.0f", over a pile
constexpr uint32_t k_FoodAmount = 138;
constexpr uint32_t k_WoodAmount = 139;

/// A belief tooltip is shown in thousandths
constexpr double k_BeliefScale = 1000.0;
/// Shares and lives show as percentages
constexpr double k_PercentScale = 100.0;

/// A tooltip and the numbers written into its words
struct Numbered
{
	uint32_t index;
	double first;
	std::optional<double> second;
};

/// A village centre's belief tooltip, from how much more the town believes in the player than in the player it believes
/// in most besides: the belief left the player before another takes the town, or once they trail, what is needed to
/// catch up
[[nodiscard]] Numbered Belief(float margin);

/// A temple's health is its heart's life, 0 to 1, as a percentage
[[nodiscard]] Numbered TempleHealth(float life);

/// A village centre's people, and the places in the homes of its town
[[nodiscard]] Numbered Population(uint32_t people, uint32_t places);

/// A village store's food and wood
[[nodiscard]] Numbered Stored(uint32_t food, uint32_t wood);

/// A pile's food or wood amount, by what it holds
[[nodiscard]] Numbered PileAmount(bool food, uint32_t amount);

/// The amount shown as the hand takes from a pile
[[nodiscard]] Numbered HeldAmount(uint32_t amount);

/// A tooltip's words with its numbers written in, as the game writes them
[[nodiscard]] std::u16string Compose(uint32_t index, std::u16string_view words, double first, std::optional<double> second);

} // namespace openblack::gui::world_tooltip
