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
#include "3D/SkyInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack
{

class Sky final: public SkyInterface
{
public:
	Sky() noexcept;

	void SetTime(float time) noexcept override;
	[[nodiscard]] float GetCurrentSkyType() const noexcept override;
	[[nodiscard]] float GetTime() const noexcept override { return _clock.GetVisualTime(); }
	[[nodiscard]] DayNightTimes GetDayNightTimes() const noexcept override
	{
		const auto& times = _clock.GetVisualTimes();
		return {.nightFull = times[0], .duskStart = times[1], .duskEnd = times[2], .dayFull = times[3]};
	}
	[[nodiscard]] DayNightClock& GetClock() noexcept override { return _clock; }
	[[nodiscard]] const DayNightClock& GetClock() const noexcept override { return _clock; }
	[[nodiscard]] sky_dome::FrameRows AdvanceDome() noexcept override { return _dome.Advance(GetCurrentSkyType()); }

private:
	DayNightClock _clock;
	sky_dome::Follow _dome {2.0f};
};

} // namespace openblack
