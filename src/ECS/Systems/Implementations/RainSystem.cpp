/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RainSystem.h"

#include <algorithm>
#include <array>
#include <functional>
#include <optional>
#include <utility>

#include <glm/geometric.hpp>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/LandLightFrame.h"
#include "3D/WeatherBlocks.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Weather.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/DetailLevel.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// A land block's side, and the distance from its centre to the middle of its middle cells
constexpr float k_BlockSize = 160.0f;
constexpr float k_MiddleCells = 40.0f;

rain::Random GameRandom()
{
	return [](float a, float b) { return Locator::gameRandom::value().CrtRandom(a, b); };
}
} // namespace

RainSystem::RainSystem()
{
	Reset();
}

void RainSystem::Reset()
{
	const auto random = GameRandom();
	for (auto& streak : _streaks)
	{
		streak = rain::Place(random);
	}
	_fall = {};
	_drawn = false;
	_splashPace = {};
	_splashesDue = false;
	_splashCount = 0;
}

void RainSystem::Update(float seconds, std::chrono::milliseconds frameRealTime, const glm::vec3& camera)
{
	// The game made its splashes on each frame that it drew
	_splashesDue = frame_pace::Due(_splashPace, frameRealTime) || _splashesDue;
	// The storm nearest the camera across the land sets how high and fast the rain falls
	std::optional<rain::Fall> nearest;
	float nearestDistance = 0.0f;
	Locator::entitiesRegistry::value().Each<const components::Storm>([&](const components::Storm& storm) {
		const glm::vec2 offset {storm.position.x - camera.x, storm.position.z - camera.z};
		const float distance = glm::dot(offset, offset);
		if (!nearest || distance < nearestDistance)
		{
			nearestDistance = distance;
			nearest = rain::Fall {.height = storm.cloudHeight, .speed = storm.rainSpeed};
		}
	});
	_fall = rain::Follow(_fall, nearest);
	if (_drawn)
	{
		_drawn = false;
		rain::Step(_streaks, seconds, _fall.speed, GameRandom());
	}
}

std::vector<rain::Tile> RainSystem::TakeTiles(const rain::View& view)
{
	std::vector<rain::Tile> tiles;
	// Rain is part of the weather setting
	const auto& settings = Locator::config::value().detail;
	if (!settings.weather || !Locator::terrainSystem::has_value() || !Locator::weatherSystem::has_value())
	{
		return tiles;
	}
	auto& weather = Locator::weatherSystem::value();
	const auto& island = Locator::terrainSystem::value();
	const glm::vec2 eye {view.eye.x, view.eye.z};

	// The blocks in view and near the eye where it rains
	struct Rained
	{
		glm::vec2 centre;
		int32_t wettest;
		float ground;
		float key;
	};
	std::vector<Rained> rained;
	for (const auto& corner : weather_blocks::InView(island, view, settings.landReflection))
	{
		const auto centre = corner + glm::vec2(k_BlockSize * 0.5f);
		// The most rain over the block's four middle cells
		int32_t wettest = 0;
		for (const auto& offset :
		     std::array {glm::vec2(-1.0f, -1.0f), glm::vec2(-1.0f, 1.0f), glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 1.0f)})
		{
			const auto point = centre + (offset * k_MiddleCells);
			wettest = std::max<int32_t>(wettest, weather.GetWeather(glm::vec3(point.x, 0.0f, point.y)).rain);
		}
		if (wettest <= rain::k_LeastRain)
		{
			continue;
		}
		const float ground = island.GetHeightAt(centre);
		rained.push_back(
		    {.centre = centre, .wettest = wettest, .ground = ground, .key = rain::DrawKey(centre, ground, view.eye)});
	}
	// The game draws the rain from the far blocks to the near blocks, and makes the splashes in that order too
	std::ranges::stable_sort(rained, std::ranges::greater {}, &Rained::key);

	const bool splash = std::exchange(_splashesDue, false) && settings.rainSplashes > 0 &&
	                    Locator::waterRingSystem::has_value() && !rained.empty();
	const uint32_t light = splash ? FrameLandLight(rain::k_SplashLight) & 0xFFFFFFu : 0u;
	const auto random = GameRandom();
	const rain::HeightAt heightAt = [&island](glm::vec2 point) { return island.GetHeightAt(point); };
	for (const auto& block : rained)
	{
		if (auto tile = rain::TileOf(block.centre, block.wettest, eye))
		{
			tile->ground = block.ground;
			tiles.push_back(*tile);
		}
		const auto alpha = splash ? rain::SplashAlpha(rain::Strength(block.wettest)) : std::nullopt;
		if (!alpha)
		{
			continue;
		}
		const uint32_t argb = (static_cast<uint32_t>(*alpha) << 24u) | light;
		for (uint8_t i = 0; i < settings.rainSplashes; ++i)
		{
			// The game takes the random numbers also when the rings are full
			const auto ring = rain::Splash(block.centre, argb, random, heightAt);
			if (Locator::waterRingSystem::value().Add(ring))
			{
				++_splashCount;
			}
		}
	}
	_drawn = _drawn || !tiles.empty();
	return tiles;
}
