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

#include "3D/AllMeshes.h"

namespace openblack::ecs::components
{

/// The clip a script asked a villager to play when it puts it in the playing state, and how many more times to play it
struct ScriptAnimation
{
	AnimId clip {AnimId::Invalid};
	uint32_t playsLeft {0};
};

} // namespace openblack::ecs::components
