/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "MoonSystem.h"

#include "3D/SkyDome.h"

using namespace openblack::ecs::systems;

void MoonSystem::Update(const MoonFrame& frame)
{
	// The computer's date is read at most once every two seconds, and kept between reads
	if (frame.realTime - _readAt > k_DateReadInterval)
	{
		_readAt = frame.realTime;
		_readDate = 0;
	}
	if (_readDate == 0)
	{
		_readDate = frame.wallClock;
	}
	_phase = graphics::moon::Phase(GetDate());

	_placement = graphics::moon::Place(frame.scriptHour);
	_strength = _placement ? sky_dome::ThroughOvercast(_placement->alpha, frame.overcast, frame.fog) : 0.0f;
	// Only a moon that shows takes up the phase that scripts are told of
	if (_strength > 0.0f)
	{
		_shownPhase = _phase;
	}
}

float MoonSystem::GetScriptPercentage() const
{
	return graphics::moon::ScriptPercentage(_shownPhase);
}

int64_t MoonSystem::GetDate() const
{
	return _dateOverride.value_or(_readDate);
}

void MoonSystem::SetDateOverride(std::optional<int64_t> date)
{
	_dateOverride = date;
	_phase = graphics::moon::Phase(GetDate());
}
