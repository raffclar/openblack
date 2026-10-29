/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs::components
{

/// A puzzle game that a script made. It has only its kind for now: none of its pieces, so the player cannot solve it.
struct PuzzleGame
{
	script::PuzzleGameType type {script::PuzzleGameType::None};
};

} // namespace openblack::ecs::components
