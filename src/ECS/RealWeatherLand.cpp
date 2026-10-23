/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RealWeatherLand.h"

#include <stdexcept>

#include "3D/LandIslandInterface.h"
#include "Camera/ZoomToPlaces.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs
{

components::WeatherInfo RealWeatherLand::GetBackground() const
{
	return Locator::weatherSystem::has_value() ? Locator::weatherSystem::value().GetBackgroundWeather()
	                                           : components::WeatherInfo {};
}

void RealWeatherLand::SetBackground(const components::WeatherInfo& weather)
{
	if (Locator::weatherSystem::has_value())
	{
		Locator::weatherSystem::value().SetBackgroundWeather(weather);
	}
}

entt::entity RealWeatherLand::AddStorm(const script_weather::LandStorm& storm)
{
	return Locator::weatherSystem::has_value() ? Locator::weatherSystem::value().AddStandingStorm(storm) : entt::null;
}

bool RealWeatherLand::StormLives(entt::entity storm) const
{
	return Locator::weatherSystem::has_value() && Locator::weatherSystem::value().StormLives(storm);
}

void RealWeatherLand::EndStorm(entt::entity storm)
{
	if (Locator::weatherSystem::has_value() && StormLives(storm))
	{
		Locator::weatherSystem::value().EndStorm(storm);
	}
}

glm::vec3 RealWeatherLand::IslandCentre() const
{
	try
	{
		// Half way between the first and the last block of land, on the ground plane
		const auto blocks = Locator::terrainSystem::value().GetIndexExtent();
		const auto centre = zoom_to::IslandCentre(blocks.minimum, blocks.maximum);
		return {centre.x, 0.0f, centre.y};
	}
	catch (const std::runtime_error&)
	{
		// No land is loaded
		return glm::vec3 {0.0f};
	}
}

bool RealWeatherLand::IsMultiplayer() const
{
	return Locator::time::has_value() && Locator::time::value().IsNetworkGame();
}

} // namespace openblack::ecs
