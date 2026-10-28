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

/// The first random number of each living thing that the game makes: a villager, an animal or a creature
namespace openblack::ecs::living_birth
{

/// Each living thing that the game makes draws a number below half of the start age of its kind. The game draws it
/// before all other numbers of the thing, and then gives the thing its age in a different way. Thus the game does not
/// use the number.
[[nodiscard]] constexpr uint32_t AgeDrawRange(uint32_t startAge) noexcept
{
	return startAge >> 1u;
}

} // namespace openblack::ecs::living_birth
