/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

namespace openblack::ecs::components
{

/// The glows of the creed on the creature's hands: for each of the four kinds of glow, its scale and its power. The
/// saved game keeps these values. openblack does not show the glows yet, so it only keeps the values.
struct CreatureCreedPowers
{
	struct Glow
	{
		float scale {0.0f};
		float power {0.0f};
	};
	std::array<Glow, 4> glows {};
};

} // namespace openblack::ecs::components
