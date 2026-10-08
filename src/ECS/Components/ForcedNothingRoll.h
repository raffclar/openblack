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

namespace openblack::ecs::components
{

/// The roll the next idle choice of this villager takes instead of its random one (the draw is still made). Set by the
/// debug hooks and the tests, removed once used
struct ForcedNothingRoll
{
	uint32_t roll;
};

} // namespace openblack::ecs::components
