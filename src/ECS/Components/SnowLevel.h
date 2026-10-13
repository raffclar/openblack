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

/// The snow that a thing shows, 0 to 255. The level moves slowly towards the snow where the thing stands, but only while
/// the land draws the thing. A thing without this component shows no snow, as a new thing does.
struct SnowLevel
{
	int32_t level {0};
};

} // namespace openblack::ecs::components
