/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeSounds.h"

#include <limits>

using namespace openblack::ecs;

uint32_t abode_sounds::Chance(std::chrono::duration<float, std::milli> gameTime) noexcept
{
	if (gameTime.count() <= 0.0f)
	{
		return 0;
	}
	const auto chance = k_SoundTime / gameTime;
	if (chance >= static_cast<float>(std::numeric_limits<uint32_t>::max()))
	{
		return 0;
	}
	return static_cast<uint32_t>(chance);
}
