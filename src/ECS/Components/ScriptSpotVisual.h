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

/// A spot visual a script started, held by the script as a thing of the world: the script may keep it, test it and
/// delete it. Deleting the thing ends its visual at once; the thing goes once its visual has ended.
struct ScriptSpotVisual
{
	/// The particle system's effect
	uint32_t effect {0};
};

} // namespace openblack::ecs::components
