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

#include <array>
#include <vector>

namespace openblack::land_avoid
{
/// The creature's walkable mask of the open landscape (ecs::systems::LandAvoidSystemInterface owns it); empty
/// before the first Validate
struct State
{
	int32_t size {0};
	std::vector<uint8_t> avoid;         ///< [z * size + x]
	std::array<int32_t, 2> seed {0, 0}; ///< the flood's seed, for the dump's log
};
} // namespace openblack::land_avoid
