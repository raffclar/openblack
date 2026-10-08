/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FixedClock.h"

#include <cstdio>
#include <cstdlib>

#include "GameClock.h"
#include "Locator.h"

namespace openblack::fixed_clock
{

void InstallFromEnvironment()
{
	if (const char* ms = std::getenv("OPENBLACK_FIXED_FRAME_MS"); ms != nullptr)
	{
		const long value = std::strtol(ms, nullptr, 10);
		if (value >= 1 && value <= 1000)
		{
			Install(static_cast<uint32_t>(value));
		}
	}
}

void Install(uint32_t frameMs)
{
	if (!Locator::time::has_value())
	{
		// Uninstalling after the game has gone (and its clock with it) has nothing left to do
		if (frameMs == 0)
		{
			return;
		}
		std::fputs("fixed_clock: no TimeSystemInterface in the locator to install the fixed step on\n", stderr);
		std::abort();
	}
	// The fixed step is a mode of the game clock: its ticks start at 0
	Locator::time::value().SetFixedFrameMs(frameMs);
}

bool Enabled()
{
	return Locator::time::has_value() && Locator::time::value().FixedFrameMs() != 0;
}

std::chrono::microseconds AdvanceFrame()
{
	auto& clock = Locator::time::value();
	clock.AdvanceFixedFrame();
	return std::chrono::milliseconds(clock.FixedFrameMs());
}

} // namespace openblack::fixed_clock
