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

#include "landgen/Grid.h"

/// The shape of the land: the first heights and the erosion that wears them. Heights are in the altitude units of a
/// landscape (0.67 m each), with the sea bed below 0. The corners are 10 m apart.
namespace openblack::landgen::terrain
{
/// The highest altitude that a landscape can hold
constexpr float k_HighestAltitude = 255.0f;
/// The highest altitude that the game draws at sea level
constexpr float k_SeaLevel = 3.0f;

struct BaseSettings
{
	/// The corners next to each edge of the map that stay under the sea, so that the land is an island
	int seaMargin {40};
	/// How many mountain ranges cross the island
	int ranges {3};
	/// The height of a crest of a range, before the erosion wears it
	float mountainHeight {300.0f};
	/// The height of the inland plains
	float plainHeight {60.0f};
	/// The height of the hills on the plains, up or down
	float hillHeight {22.0f};
	/// How much of the map the island takes, from about -0.3 (small) to 0.3 (large)
	float landAmount {0.0f};
	/// How many hollows the low land has, each a radius in corners, to hold the lakes and lagoons
	int hollows {6};
	float hollowRadius {16.0f};
	/// The highest ground where a hollow can be, and the altitude of its floor
	float hollowGround {28.0f};
	float hollowFloor {-6.0f};
};

/// The first heights of the island: an outline of warped noise, plains and hills, and mountain ranges along random
/// lines, each with ridged noise for its crests and valleys. Hollows in the low land below the hollow ground hold the
/// lakes.
[[nodiscard]] Grid<float> BaseHeights(int side, uint64_t seed, const BaseSettings& settings);

struct HydraulicSettings
{
	/// How many drops of rain fall, each at a random corner
	int drops {400000};
	/// The most steps that a drop takes before it dries up
	int steps {80};
	/// How much a drop keeps its direction rather than flow down the slope, from 0 to 1
	float inertia {0.05f};
	/// How much sediment a drop can hold, for its speed, water and the drop of its height
	float capacity {4.0f};
	float minCapacity {0.01f};
	/// The part of the free capacity that a drop takes from the ground in a step
	float erosion {0.3f};
	/// The part of the sediment above the capacity that a drop puts down in a step
	float deposition {0.3f};
	/// The part of its water that a drop loses in a step
	float evaporation {0.012f};
	float gravity {4.0f};
	/// The radius in corners from which a drop takes ground
	int radius {3};
	/// The scale from altitude units to the units of the drop's slope. Lower values make the slopes gentler for the drops.
	float heightScale {0.067f};
};

/// Rain wears the land: drops run down the slopes, take ground where they speed up and put it down where they slow
/// down or go into the sea. Valleys cut into the slopes and the sediment fills the low ground.
void ErodeHydraulic(Grid<float>& heights, uint64_t seed, const HydraulicSettings& settings);

struct ThermalSettings
{
	int passes {30};
	/// The steepest difference of altitude to the next corner along an axis that the ground keeps on the plains and on
	/// the mountains. Between them the limit changes with the altitude.
	float plainTalus {9.0f};
	float mountainTalus {18.0f};
	/// The part of the ground above the limit that slides down in a pass
	float rate {0.4f};
};

/// The ground of the slopes that are too steep slides down to the corners below them, until no slope is steeper than
/// its limit. All the corners slide at the same time in a pass, so that the order of the corners has no effect.
void ErodeThermal(Grid<float>& heights, const ThermalSettings& settings);

/// The largest difference of height to the next corner along an axis, at each corner
[[nodiscard]] Grid<float> Slopes(const Grid<float>& heights);
} // namespace openblack::landgen::terrain
