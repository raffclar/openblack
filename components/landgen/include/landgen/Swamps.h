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

#include <vector>

#include "landgen/Cursed.h"
#include "landgen/Features.h"

/// The mist of the swamps: the dark green mist of the fourth land of the story, as its land script makes it, over the
/// wet ground of a swamp
namespace openblack::landgen::swamps
{
/// The colour of the green mist without its alpha, 0xRRGGBB
constexpr uint32_t k_GreenMistColour = 0x50632Fu;

struct Settings
{
	/// The land for each sheet of green mist, in square kilometres. The fourth land of the story has one sheet on
	/// 2.99 square kilometres of land, and no land of the story has more sheets for its area.
	float landPerSheet {2.99f};
	/// The most sheets of green mist on a land, also on a large land
	uint32_t most {8};
	/// The distance between the points where a sheet can start, in corners
	float spacing {2.0f};
};

/// One sheet of green mist: the place of its first puff in metres
struct Sheet
{
	float x;
	float z;
};

/// The sheets of green mist, and their puffs
struct Result
{
	std::vector<Sheet> sheets;
	std::vector<cursed::Mist> mists;
};

/// The number of puffs in one sheet of green mist
[[nodiscard]] size_t PuffsInSheet();

/// The number of sheets of green mist that a land of this area can have, in square kilometres
[[nodiscard]] uint32_t SheetsFor(float landKm2, const Settings& settings);

/// Puts the sheets of green mist over the swamps of the land:
/// - A land has one sheet for each landPerSheet square kilometres of land, and no more than the most sheets.
/// - Each sheet has the puffs of the sheet of the story, with the same places, colours, sizes and shapes.
/// - Each puff is over the free ground of a swamp, and the ground under the sheet rises no more than under the sheet of
///   the story.
/// - The sheets do not turn, as the land script places them.
///
/// The features under a sheet stay, as the bushes and oaks under the sheet of the story stay. The same seed gives the
/// same sheets.
[[nodiscard]] Result Place(const features::Ground& ground, uint64_t seed, const Settings& settings);
} // namespace openblack::landgen::swamps
