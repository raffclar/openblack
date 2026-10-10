/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ScriptHeaders/ScriptTimers.h"

namespace openblack::ecs::components
{

/// A timer a script made, which counts the game's turns from when it was set
struct ScriptTimer
{
	script::timers::Timer timer;
};

} // namespace openblack::ecs::components
