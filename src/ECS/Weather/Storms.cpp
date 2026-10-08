/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Storms.h"

#include <cmath>

#include <list>

#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Weather/WeatherState.h"
#include "LightningFlash.h"
#include "Locator.h"
#include "WeatherLand.h"

using namespace openblack::weather;
using namespace openblack::weather::storms;

namespace
{
/// Newest first (a new storm goes at the head)
/// The weather's state (Locator::weatherSystem)
openblack::weather::State& WeatherState()
{
	return openblack::Locator::weatherSystem::value().GetState();
}

/// The engine's random callback: min + GameFloatRand(max - min)
float RandomRange(float min, float max)
{
	return openblack::game_random::GameFloatRange(min, max);
}

/// One storm's update
void Update(Storm& storm, float seconds)
{
	auto& d = storm.descriptor;
	storm.age += seconds;
	if (!(storm.age < d.lifeTime))
	{
		MarkForDeletion(storm.id);
		return;
	}
	// fade in and out over fadeInTime, the inner radius with it
	if (storm.age < d.fadeInTime)
	{
		storm.fade = storm.age * d.strength / d.fadeInTime;
		storm.innerRadius = storm.age * d.innerRadius / d.fadeInTime;
	}
	else if (d.lifeTime - d.fadeInTime < storm.age)
	{
		const float left = d.lifeTime - storm.age;
		storm.fade = left * d.strength / d.fadeInTime;
		storm.innerRadius = left * d.innerRadius / d.fadeInTime;
	}
	else
	{
		storm.fade = d.strength;
		storm.innerRadius = d.innerRadius;
	}

	// towards the target at `speed` metres per second (2D; the height stays)
	storm.drawPosition = d.position;
	if (storm.speed != 0.0f)
	{
		const float step = seconds * storm.speed;
		const float dx = storm.drawPosition.x - storm.target.x;
		const float dz = storm.drawPosition.z - storm.target.z;
		const float distance = std::sqrt(dx * dx + dz * dz);
		if (distance > 0.001f)
		{
			const float f = step < distance ? step / distance : 1.0f;
			storm.drawPosition.x += (storm.target.x - storm.drawPosition.x) * f;
			storm.drawPosition.z += (storm.target.z - storm.drawPosition.z) * f;
			d.position = storm.drawPosition;
			storm.arrived = false;
		}
		else
		{
			storm.drawPosition = storm.target;
			d.position = storm.target;
			storm.arrived = true;
		}
	}

	storm.outerRadius = d.outerRadius;
	// (int)(snow x snowCoverRate x fade): the snow cover lays snow in the inner/outer radius at amount x seconds x
	// 0.03. The snow cover is not ported.

	// the lightning, once faded in (and while the engine's random callback exists, always in a game)
	if (storm.age > d.fadeInTime)
	{
		const glm::vec3 point(d.position.x, LandHeightAt(d.position.x, d.position.z) + d.elevation, d.position.z);
		if (d.forkMax != 0.0f)
		{
			storm.forkTimer -= seconds;
			if (storm.forkTimer <= 0.0f)
			{
				storm.forkTimer = RandomRange(d.forkMin, d.forkMax);
				// the light flash (0.5)
				flash::Start(storm.flash, point, d.outerRadius, 0.5f);
				if (WeatherState().forkCallback)
				{
					WeatherState().forkCallback(storm, point, d.outerRadius);
				}
			}
		}
		if (d.sheetMax != 0.0f)
		{
			storm.sheetTimer -= seconds;
			if (storm.sheetTimer <= 0.0f)
			{
				storm.sheetTimer = RandomRange(d.sheetMin, d.sheetMax);
				// the flash (1.0); then the thunder
				flash::Start(storm.flash, point, d.outerRadius, 1.0f);
				if (WeatherState().sheetCallback)
				{
					WeatherState().sheetCallback(storm, point, d.outerRadius);
				}
			}
		}
	}
	// the flash object's own step
	flash::Age(storm.flash, seconds);
}
} // namespace

StormId storms::Create(const StormDescriptor& descriptor)
{
	// the descriptor, the position as the draw position and the target, speed 1, age and timers 0
	const auto stormId = WeatherState().nextStormId++;
	Storm storm {
	    .id = stormId,
	    .descriptor = descriptor,
	    .target = descriptor.position,
	    .speed = 1.0f,
	    .arrived = true,
	    .drawPosition = descriptor.position,
	};
	WeatherState().storms.push_front(storm);
	return storm.id;
}

Storm* storms::Find(StormId id)
{
	if (id == k_NoStorm)
	{
		return nullptr;
	}
	for (auto& storm : WeatherState().storms)
	{
		if (storm.id == id)
		{
			return storm.deleteCounter == 0 ? &storm : nullptr;
		}
	}
	return nullptr;
}

void storms::MarkForDeletion(StormId id)
{
	for (auto& storm : WeatherState().storms)
	{
		if (storm.id == id && storm.deleteCounter == 0)
		{
			storm.deleteCounter = 1;
		}
	}
}

void storms::Destroy(StormId id)
{
	WeatherState().storms.remove_if([id](const Storm& storm) { return storm.id == id; });
}

void storms::KillStormsInArea(const glm::vec3& position, float radius)
{
	for (auto& storm : WeatherState().storms)
	{
		const float dx = storm.drawPosition.x - position.x;
		const float dz = storm.drawPosition.z - position.z;
		if (radius + storm.descriptor.outerRadius > std::sqrt(dx * dx + dz * dz))
		{
			MarkForDeletion(storm.id);
		}
	}
}

void storms::UpdateAll(float seconds)
{
	for (auto it = WeatherState().storms.begin(); it != WeatherState().storms.end();)
	{
		Update(*it, seconds);
		if (it->deleteCounter != 0 && ++it->deleteCounter > 2)
		{
			it = WeatherState().storms.erase(it);
			continue;
		}
		++it;
	}
}

void storms::CalcAtmos(const Storm& storm, const glm::vec3& point, WeatherInfo& weather)
{
	const float r = storm.outerRadius;
	const auto& centre = storm.drawPosition;
	if (!(centre.x - r <= point.x) || centre.x + r < point.x || !(centre.z - r <= point.z) || centre.z + r < point.z)
	{
		return;
	}
	const float dx = point.x - centre.x;
	const float dz = point.z - centre.z;
	const float d2 = dx * dx + dz * dz;
	if (d2 > r * r)
	{
		return;
	}
	const float inner = storm.innerRadius;
	const float f = d2 <= inner * inner ? 1.0f : 1.0f - (std::sqrt(d2) - inner) / (r - inner);
	const auto w = static_cast<int32_t>(f * storm.fade * 256.0f);
	if (w == 0)
	{
		return;
	}
	const auto& s = storm.descriptor.weather;
	// the temperature moves towards the storm's (a wrapping byte), the rest is added
	weather.temperature = LerpByte(weather.temperature, s.temperature, w);
	weather.rain = ClampAdd(weather.rain, (s.rain * w) >> 8);
	weather.snow = ClampAdd(weather.snow, (s.snow * w) >> 8);
	weather.overcast = ClampAdd(weather.overcast, (s.overcast * w) >> 8);
	weather.windX = ClampAdd(weather.windX, (s.windX * w) >> 8);
	weather.windZ = ClampAdd(weather.windZ, (s.windZ * w) >> 8);
}

void storms::CalcAtmosAll(const glm::vec3& point, WeatherInfo& weather)
{
	for (const auto& storm : WeatherState().storms)
	{
		if (storm.deleteCounter == 0)
		{
			CalcAtmos(storm, point, weather);
		}
	}
}

void storms::ForEach(const std::function<void(const Storm&)>& function)
{
	for (const auto& storm : WeatherState().storms)
	{
		function(storm);
	}
}

void storms::ForEachMutable(const std::function<void(Storm&)>& function)
{
	for (auto& storm : WeatherState().storms)
	{
		function(storm);
	}
}

void storms::Clear()
{
	WeatherState().storms.clear();
}

void storms::SetForkCallback(LightningCallback callback)
{
	WeatherState().forkCallback = std::move(callback);
}

void storms::SetSheetCallback(LightningCallback callback)
{
	WeatherState().sheetCallback = std::move(callback);
}
