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

#include <algorithm>
#include <array>

/// The order of the steps that lay out a new land after its map script. The game does them in a fixed order: the towns
/// get their scenic forests and their forests, then the players get their worship sites and their influence power, and
/// then the footpaths load. Only after all of them do the slots of a network land with no player get their computer
/// gods and creatures. Before these steps, openblack puts the things in their map cells and the footpaths go round
/// them. The order keeps no game state, so that the game and the tests use it both.
namespace openblack::land_laid_out
{

/// The steps that lay out a land after its map script
enum class Step : uint8_t
{
	/// The things that the script made go into their map cells
	PlaceThings,
	/// The footpaths go round the things in the map cells
	FootpathObstacles,
	/// Each town gathers the lone trees about it into its scenic forest
	ScenicForests,
	/// Each town finds the forests near it with wood in them
	ForestsToTowns,
	/// Each town of a player with a temple gets its worship site if it has none
	WorshipSites,
	/// Each player with a citadel gets its influence power one time
	InfluencePower,
	/// The footpaths of the land load from their file, when the land has one
	FootpathFile,
	/// Each slot of a network land with a citadel and no player gets a computer god and its creature
	FillSlots,
};

/// The steps in their order
constexpr std::array k_Order {
    Step::PlaceThings,  Step::FootpathObstacles, Step::ScenicForests, Step::ForestsToTowns,
    Step::WorshipSites, Step::InfluencePower,    Step::FootpathFile,  Step::FillSlots,
};

/// The place of a step in the order
[[nodiscard]] constexpr size_t PlaceOf(Step step)
{
	return static_cast<size_t>(std::ranges::find(k_Order, step) - k_Order.begin());
}

} // namespace openblack::land_laid_out
