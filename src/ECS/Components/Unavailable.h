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

/// The thing is unavailable (GAME_THING_FLAG_UNAVAILABLE): set when it is to be deleted; the thing waits on the dead
/// list and is freed when that list is processed. Readers ask ecs::IsAvailable (ECS/ToBeDeleted.h)
struct Unavailable
{
};

} // namespace openblack::ecs::components
