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

/// The light that the land is laid with. The lands of the story keep a luminosity for each corner, which their makers
/// gave them. A generated land works its luminosity out from the sun.
namespace openblack::landgen::lighting
{
/// The direction towards the sun, of length 1, with y up. It comes from the light of the lands of the story: their
/// slopes that face the low x and the low z are the brightest.
struct Sun
{
	float x;
	float y;
	float z;
};
constexpr Sun k_Sun {-0.474f, 0.743f, -0.473f};

struct Settings
{
	/// The luminosity is base plus gain times the cosine of the angle between the ground and the sun
	float base {100.0f};
	float gain {200.0f};
	/// The darkest luminosity, which the shadows of the lands of the story also use
	uint8_t darkest {48};
	/// How many corners towards the sun the shadow test looks
	int shadowReach {24};
};

/// The luminosity of each corner from the altitudes. The game draws the land below the sea level at height 0, so the
/// light does too. A corner that the land hides from the sun is in shadow, at half its luminosity.
[[nodiscard]] Grid<uint8_t> Luminosity(const Grid<uint8_t>& altitudes, const Settings& settings);
} // namespace openblack::landgen::lighting
