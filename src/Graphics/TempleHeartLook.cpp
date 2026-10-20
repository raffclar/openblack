/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleHeartLook.h"

#include "Temple/TempleDestruction.h"

namespace openblack::graphics::temple_heart_look
{

uint8_t GlowStrength(float clock, float pulse) noexcept
{
	// Each step rounds to a float: the share of the start, cut to 0 to 1, then by the pulse, then between the strengths
	float share = (clock - k_GlowFrom) / k_GlowOver;
	if (share <= 0.0f)
	{
		share = 0.0f;
	}
	else if (!(share < 1.0f))
	{
		share = 1.0f;
	}
	share = share * pulse;
	const float strength = share * (k_GlowHigh - k_GlowLow) + k_GlowLow;
	return static_cast<uint8_t>(static_cast<int32_t>(strength));
}

bool Drawn(bool destroying, float clock) noexcept
{
	return !destroying || temple_destruction::LanternsShown(clock);
}

} // namespace openblack::graphics::temple_heart_look
