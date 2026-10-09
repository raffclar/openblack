/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "Sky.h"

namespace openblack
{

Sky::Sky() noexcept
{
	_clock.Reset();
	_dome = sky_dome::Follow(GetCurrentSkyType());
}

void Sky::SetTime(float time) noexcept
{
	_clock.SetScriptTime(time);
	_dome.Jump(GetCurrentSkyType());
}

float Sky::GetCurrentSkyType() const noexcept
{
	return _clock.SkyType(_clock.GetVisualTime());
}

} // namespace openblack
