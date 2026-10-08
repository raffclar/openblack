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
/// A heal spell's chakra is on this object (as many chakras as `chakras`), so that a second heal spell picks another
struct ChakraMark
{
	uint16_t chakras {1};
};
} // namespace openblack::ecs::components
