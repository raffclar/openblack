/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleHeartLook.h"

#include <array>

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

namespace
{
/// The times of the shell's two tables, in seconds of the destruction, and their values
constexpr std::array<float, 4> k_ShellTimes {0.0f, 1.0f, 20.0f, 22.0f};
constexpr std::array<float, 4> k_ShellSizes {0.0f, 1.0f, 1.0f, 2.0f};
constexpr std::array<float, 4> k_ShellReaches {0.0f, 5.0f, 5.0f, 10.0f};
constexpr float k_ShellSizeScale = 0.3f;

/// A table's value at a time: its first value up to its first time, its last from its last time, and in a straight line
/// between two times
float Table(const std::array<float, 4>& values, float clock)
{
	if (!(k_ShellTimes.front() < clock))
	{
		return values.front();
	}
	if (!(clock < k_ShellTimes.back()))
	{
		return values.back();
	}
	for (size_t i = 0; i + 1 < k_ShellTimes.size(); ++i)
	{
		if (clock < k_ShellTimes.at(i + 1))
		{
			const float share = (clock - k_ShellTimes.at(i)) / (k_ShellTimes.at(i + 1) - k_ShellTimes.at(i));
			return ((values.at(i + 1) - values.at(i)) * share) + values.at(i);
		}
	}
	return values.back();
}
} // namespace

float ShellScale(float clock) noexcept
{
	return Table(k_ShellSizes, clock) * k_ShellSizeScale;
}

float ShellReach(float clock) noexcept
{
	return Table(k_ShellReaches, clock);
}

bool Drawn(bool destroying, float clock) noexcept
{
	return !destroying || temple_destruction::LanternsShown(clock);
}

} // namespace openblack::graphics::temple_heart_look
