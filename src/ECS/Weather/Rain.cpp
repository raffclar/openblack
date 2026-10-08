/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Rain.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Atmos.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/Weather/WeatherState.h"
#include "Locator.h"
#include "Storms.h"
#include "WeatherLand.h"

using namespace openblack;
using namespace openblack::weather;
using namespace openblack::weather::rain;

namespace
{
/// The weather's state (Locator::weatherSystem)
openblack::weather::State& WeatherState()
{
	return openblack::Locator::weatherSystem::value().GetState();
}
/// 7 draws of the CRT rand (game_random::crt, not GRand)
void Place(Drop& drop)
{
	drop.x = game_random::crt::Random(-160.0f, 160.0f) * 0.5f;
	drop.z = game_random::crt::Random(-160.0f, 160.0f) * 0.5f;
	drop.dx = game_random::crt::Random(-15.0f, 15.0f);
	drop.dz = game_random::crt::Random(-15.0f, 15.0f);
	drop.scroll = game_random::crt::Random(0.0f, 1.0f);
	drop.speed = game_random::crt::Random(0.1f, 0.2f);
	drop.phase = game_random::crt::Random(0.0f, 1.0f);
}

/// 4 more Random draws per reborn drop
void Step(float seconds)
{
	const float step = seconds * 2.4f;
	for (auto& drop : WeatherState().drops)
	{
		drop.scroll += WeatherState().rainFallSpeed * drop.speed * seconds;
		if (drop.scroll > 1.0f)
		{
			drop.scroll -= static_cast<float>(static_cast<int32_t>(drop.scroll));
		}
		drop.phase += step;
		if (drop.phase > 1.0f)
		{
			drop.phase -= static_cast<float>(static_cast<int32_t>(drop.phase));
			drop.dx = game_random::crt::Random(-15.0f, 15.0f);
			drop.dz = game_random::crt::Random(-15.0f, 15.0f);
			drop.x = game_random::crt::Random(-160.0f, 160.0f) * 0.5f;
			drop.z = game_random::crt::Random(-160.0f, 160.0f) * 0.5f;
		}
	}
}
} // namespace

void rain::Reset()
{
	for (auto& drop : WeatherState().drops)
	{
		Place(drop);
	}
	WeatherState().rainElevation = 160.0f;
	WeatherState().rainFallSpeed = 1.0f;
	WeatherState().rainDrawn = false;
}

void rain::Update(float seconds, const glm::vec3& camera)
{
	// the nearest storm (2D, from its descriptor's position)
	const storms::Storm* nearest = nullptr;
	float best = 0.0f;
	storms::ForEach([&](const storms::Storm& storm) {
		if (storm.deleteCounter != 0)
		{
			return;
		}
		const float dx = storm.descriptor.position.x - camera.x;
		const float dz = storm.descriptor.position.z - camera.z;
		const float d2 = dx * dx + dz * dz;
		if (nearest == nullptr || d2 < best)
		{
			best = d2;
			nearest = &storm;
		}
	});
	const float elevation = nearest != nullptr ? nearest->descriptor.elevation : 160.0f;
	const float fallSpeed = nearest != nullptr ? nearest->descriptor.fallSpeed : 1.0f;
	// (inside (inner + outer) / 2 of it the landscape's storm light follows it: not ported)
	WeatherState().rainElevation += (elevation - WeatherState().rainElevation) * 0.3f;
	WeatherState().rainFallSpeed += (fallSpeed - WeatherState().rainFallSpeed) * 0.3f;
	if (!(WeatherState().rainFallSpeed > 0.3f))
	{
		WeatherState().rainFallSpeed = 0.3f;
	}
	else if (!(WeatherState().rainFallSpeed < 5.0f))
	{
		WeatherState().rainFallSpeed = 5.0f;
	}
	if (!(WeatherState().rainElevation > 40.0f))
	{
		WeatherState().rainElevation = 40.0f;
	}
	else if (!(WeatherState().rainElevation < 640.0f))
	{
		WeatherState().rainElevation = 640.0f;
	}
	// (the lightning flash decays here: not ported)
	if (WeatherState().rainDrawn)
	{
		WeatherState().rainDrawn = false;
		Step(seconds);
	}
}

std::vector<Tile> rain::CollectTiles(const glm::vec3& camera)
{
	std::vector<Tile> tiles;
	if (!Locator::terrainSystem::has_value())
	{
		return tiles;
	}
	for (const auto& block : Locator::terrainSystem::value().GetBlocks())
	{
		if (!block.GetLndBlock())
		{
			continue;
		}
		// the block's centre (origin + 80); its distance must be under 400 + 160 (the 3D engine keeps it per block; here it
		// is the 2D distance from the camera, and the tile's own 400 m cut below is what matters)
		const glm::vec2 centre = block.GetMapPosition() + glm::vec2(80.0f);
		const float blockDistance = std::hypot(centre.x - camera.x, centre.y - camera.z);
		if (!(400.0f + 160.0f > blockDistance))
		{
			continue;
		}
		// two samples (the centre and the centre + 40), the largest of their rain and snow
		int32_t wettest = std::numeric_limits<int32_t>::min();
		for (const auto& offset : {glm::vec2(0.0f, 0.0f), glm::vec2(40.0f, 40.0f)})
		{
			const auto w = atmos::GetWeather(glm::vec3(centre.x + offset.x, 0.0f, centre.y + offset.y), true);
			wettest = std::max({wettest, static_cast<int32_t>(w.rain), static_cast<int32_t>(w.snow)});
		}
		if (wettest <= 5)
		{
			continue;
		}
		// The Z-sorter's user data packs the tile (x / 80, z / 80) and this alpha in its three low bytes, which the draw
		// unpacks for the tile (x, z, 0, 128, alpha); over 44 it also spawns the water drops on the ground (not ported)
		// 88 x max / 100 (an integer division), clamped to 255; only a negative one is dropped
		int32_t alpha = std::min(wettest * 88 / 100, 0xFF);
		if (alpha < 0)
		{
			continue;
		}
		int32_t drops = k_Drops;
		const float d = std::hypot(camera.x - centre.x, camera.z - centre.y);
		if (d > 400.0f)
		{
			continue;
		}
		if (d > 100.0f)
		{
			const float k = 1.0f - (d - 100.0f) / (400.0f - 100.0f);
			alpha = static_cast<int32_t>(static_cast<float>(alpha) * k);
			drops = static_cast<int32_t>(static_cast<float>(drops) * k);
		}
		if (drops <= 0)
		{
			continue;
		}
		const float distanceFactor = d / 400.0f * 2.0f + 1.0f;
		Tile tile {
		    .origin = glm::vec3(centre.x, LandHeightAt(centre.x, centre.y), centre.y),
		    .drops = std::min(drops, k_Drops),
		    .alpha = alpha,
		    .alphaTop = static_cast<int32_t>(static_cast<float>(alpha) / (distanceFactor * 5.0f)),
		};
		tiles.push_back(tile);
	}
	return tiles;
}

void rain::MarkDrawn()
{
	WeatherState().rainDrawn = true;
}

const std::array<Drop, k_Drops>& rain::Drops()
{
	return WeatherState().drops;
}

float rain::Elevation()
{
	return WeatherState().rainElevation;
}

float rain::FallSpeed()
{
	return WeatherState().rainFallSpeed;
}

float rain::PhaseFade(float phase)
{
	if (phase < 0.05f)
	{
		return phase * 20.0f;
	}
	if (phase > 0.95f)
	{
		return (1.0f - phase) * 20.0f;
	}
	return 1.0f;
}
