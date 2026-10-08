/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/DayNightClock.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
/// Owns the day / night clock, at its start values when made
class DayNightClockSystem final: public DayNightClockSystemInterface
{
public:
	[[nodiscard]] DayNightClock& Clock() noexcept override { return _clock; }
	[[nodiscard]] const DayNightClock& Clock() const noexcept override { return _clock; }

private:
	DayNightClock _clock;
};
} // namespace openblack::ecs::systems
