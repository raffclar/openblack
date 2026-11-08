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

/// The rules that make a storm and the snow it leaves part of the weather of the land
namespace openblack::storm_fade
{

/// A storm's strength and inner radius after its fade
struct Faded
{
	float strength {0.0f};
	float innerRadius {0.0f};

	bool operator==(const Faded&) const = default;
};

/// A live storm's strength and inner radius at its age. The storm fades in over its fade time, and it fades out over the
/// same time at the end of its life. The game multiplies by the time in the fade first, then divides by the fade time.
[[nodiscard]] Faded Fade(float age, float lastsFor, float fadeTime, float strength, float innerRadius);

/// After its step in a turn, a dead storm counts the turn. It goes after the second turn that it counts. Thus a storm
/// that ends at the end of its life goes on the next turn, and a storm that a script or a miracle ends takes two more
/// full steps.
[[nodiscard]] bool DeadStormGoes(uint8_t& deadTurns);

/// The lying snow that the weather of a cell keeps from the snow depth at the cell: half of the depth, cut towards zero
/// and kept in the range of a signed byte
[[nodiscard]] int8_t SnowCover(float depth);

} // namespace openblack::storm_fade
