/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The CHL weather natives (CHLApi.cpp forwards to them). The arguments are popped in the handlers' order.

#include "CHLWeather.h"

#include <LHVM.h>
#include <spdlog/spdlog.h>

#include "ECS/Weather/Climate.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/WeatherThing.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::weather;

namespace
{
glm::vec3 PopPosition(lhvm::LHVM& vm)
{
	const auto z = vm.Popf();
	const auto y = vm.Popf();
	const auto x = vm.Popf();
	return {x, y, z};
}

/// GetScriptGameThing + the IsWeather test of every CHANGE_*_PROPERTIES native ("Jonty-Weather properties on non
/// weather ...")
bool IsWeatherThing(entt::entity thing)
{
	if (weather_thing::IsWeather(thing))
	{
		return true;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "Jonty-Weather properties on non weather object");
	return false;
}
} // namespace

entt::entity script::CreateWeatherThing(uint32_t subtype, const glm::vec3& position)
{
	return weather_thing::Create(position, subtype);
}

void script::ChangeWeatherProperties()
{
	auto& vm = Locator::vm::value();
	const auto fallSpeed = vm.Popf();
	const auto overcast = vm.Popf();
	const auto snowfall = vm.Popf();
	const auto rainfall = vm.Popf();
	const auto temperature = vm.Popf();
	const auto storm = static_cast<entt::entity>(vm.Pop().uintVal);
	if (IsWeatherThing(storm))
	{
		weather_thing::SetWeatherProperties(storm, temperature, rainfall, snowfall, overcast, fallSpeed);
	}
}

void script::ChangeLightningProperties()
{
	auto& vm = Locator::vm::value();
	const auto forkMax = vm.Popf();
	const auto forkMin = vm.Popf();
	const auto sheetMax = vm.Popf();
	const auto sheetMin = vm.Popf();
	const auto storm = static_cast<entt::entity>(vm.Pop().uintVal);
	if (IsWeatherThing(storm))
	{
		weather_thing::SetLightningProperties(storm, sheetMin, sheetMax, forkMin, forkMax);
	}
}

void script::ChangeTimeFadeProperties()
{
	auto& vm = Locator::vm::value();
	const auto fadeTime = vm.Popf();
	const auto duration = vm.Popf();
	const auto storm = static_cast<entt::entity>(vm.Pop().uintVal);
	if (IsWeatherThing(storm))
	{
		weather_thing::SetTimeFadeProperties(storm, duration, fadeTime);
	}
}

void script::ChangeCloudProperties()
{
	auto& vm = Locator::vm::value();
	const auto elevation = vm.Popf();
	const auto blackness = vm.Popf();
	const auto numClouds = vm.Popf();
	const auto storm = static_cast<entt::entity>(vm.Pop().uintVal);
	if (IsWeatherThing(storm))
	{
		weather_thing::SetCloudProperties(storm, blackness, static_cast<int32_t>(numClouds), elevation);
	}
}

void script::PauseUnpauseClimateSystem()
{
	auto& vm = Locator::vm::value();
	climate::SetClimateSystemEnabled(vm.Pop().intVal != 0);
}

void script::PauseUnpauseStormCreationInClimateSystem()
{
	auto& vm = Locator::vm::value();
	climate::SetStormCreationEnabled(vm.Pop().intVal != 0);
}

void script::KillStormsInArea()
{
	auto& vm = Locator::vm::value();
	const auto radius = vm.Popf();
	const auto position = PopPosition(vm);
	storms::KillStormsInArea(position, radius);
}
