/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MachineClock.h"

#include <chrono>

#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"

using namespace openblack;

uint32_t machine_clock::Ticks()
{
	if (Locator::time::has_value())
	{
		return Locator::time::value().GetTicks();
	}
	using namespace std::chrono;
	return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

int64_t machine_clock::UnixTime()
{
	if (Locator::time::has_value())
	{
		return Locator::time::value().GetUnixTime();
	}
	using namespace std::chrono;
	return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}
