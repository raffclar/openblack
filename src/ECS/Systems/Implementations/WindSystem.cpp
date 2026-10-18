/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the one wind of the experiment

#define LOCATOR_IMPLEMENTATIONS

#include "WindSystem.h"

#include <algorithm>

#include "Camera/Camera.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "Graphics/Underwater.h"
#include "Locator.h"

using namespace openblack::ecs::systems;

void WindSystem::Update(std::chrono::milliseconds frameRealTime)
{
	// A long frame, such as a load, moves the wind on by a quarter of a second at most
	const float seconds = std::min(std::chrono::duration<float>(frameRealTime).count(), 0.25f);
	_seconds += seconds;
	if (!Locator::weatherSystem::has_value() || !Locator::camera::has_value())
	{
		return;
	}
	const auto weather = Locator::weatherSystem::value().GetWeatherSmooth(Locator::camera::value().GetOrigin());
	const glm::vec2 weatherWind {static_cast<float>(weather.windX), static_cast<float>(weather.windZ)};
	const float storm = graphics::underwater::Storminess(weather.rain, weather.overcast, weatherWind);
	auto target = wind::Target(weatherWind, storm);
	target.angle = _directionOverride.value_or(target.angle);
	_wind = wind::Step(_wind, target, seconds);
}
