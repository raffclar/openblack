/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{

/// The player's alignment (one per PlayerNames in the player system, kept across lands): -1 evil .. 1 good, and the
/// change gathered this turn that the player's turn folds in (capped by GPlayerInfo.maxAlignmentChangePerGameTurn).
/// The alignment updates add to `pending` (ECS/Effects/Alignment.h).
struct Alignment
{
	float value {0.0f};
	float pending {0.0f};
};

} // namespace openblack::ecs::components
