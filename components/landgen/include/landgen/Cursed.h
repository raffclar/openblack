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

#include "landgen/Features.h"

/// The cursed places of a land: burnt trees, dark mist over boulders and smoking craters. Each part is a place of the
/// lands of the story, as the land scripts make it.
namespace openblack::landgen::cursed
{
/// The game's number of the crater of the features
constexpr uint8_t k_CraterFeature = 74;

/// A puff of mist, as a land script makes it
struct Mist
{
	/// The place on the map in metres
	float x;
	float z;
	/// The height of the puff above the land in metres
	float altitude;
	/// The colour, 0xAARRGGBB
	uint32_t colour;
	float size;
	/// How much smaller the puff is when the camera sees it edge on. 1 gives the puff the default shape.
	float edgeShrink;
};

/// The two dark mists of the story. Each one is a stack of three puffs over a clump of boulders.
enum class Stack : uint8_t
{
	/// The near black mist of the first land over its large boulders of limestone, on a steep slope
	Black,
	/// The dark mist of the fourth land over its clump of volcanic boulders, on high rock
	Volcanic,
};

/// One cursed place: a circle of the map in metres
struct Patch
{
	float x;
	float z;
	float radius;
	/// The dark mist of all the stacks of the place
	Stack stack;
	uint32_t burntTrees;
	uint32_t stacks;
	uint32_t craters;
};

struct Settings
{
	/// The fewest and the most cursed places on a land. The seed picks the number.
	uint32_t fewest {1};
	uint32_t most {3};
	/// The radius of a place in metres
	float smallestRadius {60.0f};
	float largestRadius {150.0f};
	/// The edge of a place keeps at least this far from the town, in metres
	float nearestTown {250.0f};
	/// The middle of a place keeps at least this far from the sea, in metres
	float nearestSea {100.0f};
	/// The middle of a place is between these heights above the sea, in metres
	float lowest {40.0f};
	float highest {110.0f};
	/// The edges of two places keep at least this far apart, in metres
	float gap {150.0f};
	/// The distance between the points where a place can start, in corners
	float spacing {8.0f};
};

/// The cursed places, and what they add to the land
struct Result
{
	std::vector<Patch> patches;
	std::vector<Mist> mists;
};

/// Puts the cursed places on the land:
/// - Each place is far from the town and the sea, on high ground, and keeps away from the other places and the big
///   forests.
/// - The burnt trees stand in groups of three, as the burnt trees of the first land of the story stand.
/// - Each stack of dark mist stands over the boulders of its land of the story, at the same places round the stack.
/// - Each crater has a puff of grey smoke at its middle, as the craters of the fifth land of the story have.
///
/// The ground of a place does not change. A feature of the land that is in the way of a stack or a crater goes away.
/// The same seed gives the same places.
[[nodiscard]] Result Place(const features::Ground& ground, std::vector<features::Feature>& placed, uint64_t seed,
                           const Settings& settings);

/// The number of cursed places on a land, from a random number from 0 to below 1: mostly one, sometimes more
[[nodiscard]] uint32_t PatchCount(float unit, const Settings& settings);
} // namespace openblack::landgen::cursed
