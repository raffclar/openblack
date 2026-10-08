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

#include "GameClock.h"
#include "Locator.h"

/// The few calls the editor makes on the game's clock: the turn, and pausing for its play, pause and step buttons. The
/// editor goes through these alone, so that moving it to another clock service changes this file only.
namespace openblack::editor::clock
{

[[nodiscard]] inline bool Available()
{
	return Locator::time::has_value();
}

[[nodiscard]] inline uint32_t Turn()
{
	return Locator::time::value().Turn();
}

[[nodiscard]] inline bool IsPaused()
{
	return Locator::time::value().IsPaused();
}

inline void SetPaused(bool paused)
{
	Locator::time::value().Pause(paused);
}

} // namespace openblack::editor::clock
