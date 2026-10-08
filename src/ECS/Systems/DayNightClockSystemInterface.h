/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack
{
class DayNightClock;
}

namespace openblack::ecs::systems
{
/// The game's day / night clock (3D/DayNightClock.h), made with the Game and kept until it goes
/// (Locator::dayNightClock). The game turn advances it; a land load resets it
class DayNightClockSystemInterface
{
public:
	virtual ~DayNightClockSystemInterface() = default;

	[[nodiscard]] virtual DayNightClock& Clock() noexcept = 0;
	[[nodiscard]] virtual const DayNightClock& Clock() const noexcept = 0;
};
} // namespace openblack::ecs::systems
