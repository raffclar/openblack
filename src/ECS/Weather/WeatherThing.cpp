/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WeatherThing.h"

#include <algorithm>
#include <list>

#include <glm/mat3x3.hpp>

#include "Climate.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Weather/WeatherState.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "WeatherLand.h"

using namespace openblack;
using namespace openblack::weather;
using openblack::ecs::components::Transform;
using openblack::ecs::components::WeatherThing;

namespace
{
/// The weather's state (Locator::weatherSystem)
openblack::weather::State& WeatherState()
{
	return openblack::Locator::weatherSystem::value().GetState();
}

WeatherThing* Get(entt::entity thing)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(thing) ? registry.TryGet<WeatherThing>(thing) : nullptr;
}

/// The storm drifts by ComputeWeather's wind at it x `factor`
void Drift(storms::Storm& storm, float factor)
{
	const auto weather = climate::ComputeWeather(storm.descriptor.position, false);
	storm.descriptor.position.x += static_cast<float>(weather.windX) * factor;
	storm.descriptor.position.z += static_cast<float>(weather.windZ) * factor;
}

/// One weather thing's turn; true while it has a storm
bool Process(entt::entity entity, WeatherThing& thing)
{
	auto* storm = storms::Find(thing.storm);
	if (storm == nullptr)
	{
		thing.storm = storms::k_NoStorm;
		// not in a script -> deleted: openblack does not count the scripts' references, the thing stays
		return false;
	}
	if (thing.affectedByWind)
	{
		Drift(*storm, 0.01f);
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* transform = registry.TryGet<Transform>(entity))
	{
		// from the storm's x, z; y 0
		transform->position = glm::vec3(storm->descriptor.position.x, 0.0f, storm->descriptor.position.z);
	}
	// the storm also takes a drawing flag from the thing's flags: not ported
	thing.descriptor = storm->descriptor;
	return true;
}
} // namespace

entt::entity weather_thing::Create(const glm::vec3& position, uint32_t weatherInfo)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, glm::vec3(position.x, 0.0f, position.z), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& thing = registry.Assign<WeatherThing>(entity);
	WeatherState().things.push_front(entity);

	// at the position (land + y), inner 100, outer 300, life 100, the info
	storms::StormDescriptor d {
	    .position = glm::vec3(position.x, LandHeightAt(position.x, position.z) + position.y, position.z),
	    .innerRadius = 100.0f,
	    .outerRadius = 300.0f,
	    .lifeTime = 100.0f,
	    .elevation = 500.0f,
	};
	if (Locator::infoConstants::has_value())
	{
		const auto& infos = Locator::infoConstants::value().weather;
		// (openblack) an out-of-range subtype is clamped to the last row; the original indexes the weather infos directly
		const auto& info = infos[std::min<size_t>(weatherInfo, infos.size() - 1)];
		d.weather.temperature = static_cast<int8_t>(info.temperature);
		d.weather.rain = static_cast<int8_t>(info.wetness);
		d.weather.snow = static_cast<int8_t>(info.snowFall);
		d.weather.overcast = static_cast<int8_t>(info.overCast);
		d.weather.windX = static_cast<int8_t>(info.wind.x);
		d.weather.windZ = static_cast<int8_t>(info.wind.y);
	}
	// the storm, made at once; the thing copies its descriptor back
	thing.storm = storms::Create(d);
	if (auto* storm = storms::Find(thing.storm))
	{
		storm->drawClouds = false;
		thing.descriptor = storm->descriptor;
	}
	return entity;
}

void weather_thing::ProcessWeatherThings()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	WeatherState().things.remove_if(
	    [&registry](entt::entity e) { return !ecs::IsAvailable(e) || !registry.AllOf<WeatherThing>(e); });
	for (const auto entity : WeatherState().things)
	{
		Process(entity, registry.Get<WeatherThing>(entity));
	}
	// (the original records whether some thing has a storm; its reader is not ported)
}

void weather_thing::UpdateStats(entt::entity entity)
{
	auto* thing = Get(entity);
	if (thing == nullptr)
	{
		return;
	}
	storms::Storm* storm = storms::Find(thing->storm);
	if (storm != nullptr)
	{
		storm->descriptor = thing->descriptor;
	}
	else
	{
		thing->storm = storms::Create(thing->descriptor);
		storm = storms::Find(thing->storm);
	}
	if (storm == nullptr)
	{
		return;
	}
	const auto& d = storm->descriptor;
	storm->forkTimer = GameFloatRand(d.forkMax - d.forkMin) + d.forkMin;
	storm->sheetTimer = GameFloatRand(d.sheetMax - d.sheetMin) + d.sheetMin;
}

void weather_thing::Reset()
{
	WeatherState().things.clear();
}

void weather_thing::SetWeatherProperties(entt::entity entity, float temperature, float rainfall, float snowfall, float overcast,
                                         float fallSpeed)
{
	if (auto* thing = Get(entity))
	{
		thing->descriptor.weather.temperature = static_cast<int8_t>(static_cast<int32_t>(temperature));
		thing->descriptor.weather.rain = static_cast<int8_t>(static_cast<int32_t>(rainfall * 100.0f));
		thing->descriptor.weather.snow = static_cast<int8_t>(static_cast<int32_t>(snowfall * 100.0f));
		thing->descriptor.weather.overcast = static_cast<int8_t>(static_cast<int32_t>(overcast * 100.0f));
		thing->descriptor.fallSpeed = fallSpeed;
		UpdateStats(entity);
	}
}

void weather_thing::SetTimeFadeProperties(entt::entity entity, float duration, float fadeTime)
{
	if (auto* thing = Get(entity))
	{
		thing->descriptor.lifeTime = duration;
		thing->descriptor.fadeInTime = fadeTime;
		UpdateStats(entity);
	}
}

void weather_thing::SetCloudProperties(entt::entity entity, float blackness, int32_t numClouds, float elevation)
{
	if (auto* thing = Get(entity))
	{
		thing->descriptor.blackness = blackness;
		thing->descriptor.numClouds = numClouds;
		thing->descriptor.elevation = elevation;
		UpdateStats(entity);
	}
}

void weather_thing::SetLightningProperties(entt::entity entity, float sheetMin, float sheetMax, float forkMin, float forkMax)
{
	if (auto* thing = Get(entity))
	{
		thing->descriptor.sheetMin = sheetMin;
		thing->descriptor.sheetMax = sheetMax;
		thing->descriptor.forkMin = forkMin;
		thing->descriptor.forkMax = forkMax;
		UpdateStats(entity);
	}
}

void weather_thing::SetMovement(entt::entity entity, const glm::vec3& movement)
{
	if (auto* thing = Get(entity))
	{
		thing->descriptor.weather.windX = static_cast<int8_t>(std::clamp(static_cast<int32_t>(movement.x * 8.0f), -127, 127));
		thing->descriptor.weather.windZ = static_cast<int8_t>(std::clamp(static_cast<int32_t>(movement.z * 8.0f), -127, 127));
		UpdateStats(entity);
	}
}

void weather_thing::SetTarget(entt::entity entity, const glm::vec3& target)
{
	if (auto* thing = Get(entity))
	{
		if (auto* storm = storms::Find(thing->storm))
		{
			storm->target = target;
		}
	}
}

void weather_thing::SetSpeed(entt::entity entity, float speed)
{
	if (auto* thing = Get(entity))
	{
		if (auto* storm = storms::Find(thing->storm))
		{
			storm->speed = speed;
		}
	}
}

void weather_thing::SetAffectedByWind(entt::entity entity, bool on)
{
	if (auto* thing = Get(entity))
	{
		thing->affectedByWind = on;
	}
}

bool weather_thing::IsWeather(entt::entity entity)
{
	return Get(entity) != nullptr;
}
