/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SkyFrameSystem.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
constexpr float k_Day = 24.0f;
} // namespace

void SkyFrameSystem::SampleFrame(float visualHour) noexcept
{
	// The hour is brought into [0, 24) in floats, so an hour just under 0 such as -1e-7 comes out as 0 rather than just
	// under 24. A NaN hour falls through.
	float h = visualHour;
	while (h < 0.0f)
	{
		h += k_Day;
	}
	while (h >= k_Day)
	{
		h -= k_Day;
	}
	_hour = h;
	_skyType = sky_type::At(_hour, _thresholds);
}

void SkyFrameSystem::Jump(float visualHour) noexcept
{
	SampleFrame(visualHour);
	_dome.Jump(_skyType);
}
