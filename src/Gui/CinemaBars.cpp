/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CinemaBars.h"

#include <cmath>

#include <algorithm>
#include <limits>

using namespace openblack::gui;

void CinemaBars::Set(bool on, float transitionSeconds)
{
	if (on == _on)
	{
		return;
	}
	// Turning round partway goes back from where the bars are, on each clock
	_transitionSeconds = transitionSeconds;
	const float fraction = Fraction(_timer);
	const float drawnFraction = Fraction(_drawnTimer);
	_on = on;
	_timer = (on ? fraction : 1.0f - fraction) * transitionSeconds * 1000.0f;
	_drawnTimer = (on ? drawnFraction : 1.0f - drawnFraction) * transitionSeconds * 1000.0f;
}

void CinemaBars::Snap()
{
	_on = true;
	// So far along that the bars stay all the way in until they are next turned off
	_timer = -std::numeric_limits<float>::max();
	_fraction = Fraction(_timer);
	DrawFromTurns();
}

void CinemaBars::Update(float gameMilliseconds, float transitionSeconds)
{
	_transitionSeconds = transitionSeconds;
	_drawnTimer += gameMilliseconds;
	_drawnFraction = Fraction(_drawnTimer);
}

void CinemaBars::ProcessTurn(float turnMilliseconds, float transitionSeconds)
{
	_transitionSeconds = transitionSeconds;
	_timer += turnMilliseconds;
	_fraction = Fraction(_timer);
}

void CinemaBars::DrawFromTurns()
{
	_drawnTimer = _timer;
	_drawnFraction = _fraction;
}

float CinemaBars::Fraction(float timer) const
{
	const float slid = _transitionSeconds > 0.0f ? std::abs(timer * 0.001f / _transitionSeconds) : 1.0f;
	return std::clamp(_on ? slid : 1.0f - slid, 0.0f, 1.0f);
}

int CinemaBars::BarHeight(int width, int height, float fraction)
{
	// Half what the picture loses to 16:9, rounded towards zero
	const auto lost = static_cast<int>((static_cast<float>(height) - static_cast<float>(width) * 0.5625f) * fraction);
	return std::max(0, lost / 2);
}
