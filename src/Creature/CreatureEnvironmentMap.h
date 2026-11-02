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

#include "Creature/CreatureFizzLook.h"

// A creature whose mind file has an environment map value that is not 0 draws its body with the first environment map
// added over every part. The body then goes through a draw of its own, which takes the place of the usual draw. That
// draw has no fizz passes, no frozen tint and no ice. Thus the body shows whole while the creature fizzes or freezes,
// and its hair shows at all times. The eyes and the sea's reflection keep their usual draw.
namespace openblack::creature_environment_map
{

/// Whether the body draws with the environment map: when the value from the mind file is not 0
[[nodiscard]] constexpr bool DrawnWithMap(uint32_t environmentMap) noexcept
{
	return environmentMap != 0;
}

/// Whether any of the body shows: all of it with the map, else until it has fizzed out of sight
[[nodiscard]] inline bool BodyDrawn(float fizz, uint32_t environmentMap)
{
	return DrawnWithMap(environmentMap) || creature_fizz_look::Drawn(fizz);
}

/// Whether the body draws through the fizz's static: never with the map
[[nodiscard]] inline bool BodyFizzes(float fizz, uint32_t environmentMap)
{
	return !DrawnWithMap(environmentMap) && creature_fizz_look::Fizzing(fizz);
}

/// Whether a frozen body takes the frozen tint and the ice over it: never with the map
[[nodiscard]] constexpr bool FrozenLookShown(uint32_t environmentMap) noexcept
{
	return !DrawnWithMap(environmentMap);
}

/// Whether the hair shows: at all times with the map, else until the creature has fizzed or frozen a fifth of the way
[[nodiscard]] inline bool HairShown(float fizz, float freeze, uint32_t environmentMap)
{
	return DrawnWithMap(environmentMap) || creature_fizz_look::HairShown(fizz, freeze);
}

} // namespace openblack::creature_environment_map
