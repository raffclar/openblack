/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WeatherLoop.h"

#include <cstdio>
#include <cstdlib>

#include "3D/DayNightClock.h"
#include "Atmos.h"
#include "Camera/Camera.h"
#include "Climate.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "LightningFlash.h"
#include "Locator.h"
#include "Rain.h"
#include "StormClouds.h"
#include "WeatherThing.h"

using namespace openblack;
using namespace openblack::weather;

void weather::OnLoadMap()
{
	climate::Reset();
	weather_thing::Reset();
	atmos::Reset();
	rain::Reset();
	storm_clouds::Clear();
	ResetDebugHooks();
}

void weather::ProcessTurnStart([[maybe_unused]] uint32_t turn)
{
	if (!Locator::dayNightClock::has_value())
	{
		std::fputs("weather: no day/night clock in the locator (Locator::dayNightClock)\n", stderr);
		std::abort();
	}
	const float visualTime = Locator::dayNightClock::value().Clock().GetVisualTime();
	atmos::UpdateGame(visualTime, game_clock::k_TurnSeconds); // 0.1 s a turn
	// then the game turn sets the ambient wind direction from the ambient weather's wind (normalised; 0 in a game): no
	// reader in openblack
}

void weather::ProcessWeatherThings()
{
	weather_thing::ProcessWeatherThings();
}

void weather::ProcessClimate()
{
	climate::ProcessAll(game_clock::Turn());
	RunDebugHooks(game_clock::Turn());
}

void weather::UpdateFrame(float seconds)
{
	// First each storm's flash, then the camera's (in flash::AtCamera)
	flash::UpdateFrame();
	if (Locator::camera::has_value())
	{
		rain::Update(seconds, Locator::camera::value().GetOrigin());
		// every frame: the clouds of every storm
		storm_clouds::DrawFrame(seconds * 1000.0f);
	}
}
