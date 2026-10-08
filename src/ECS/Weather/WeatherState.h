/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <list>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>

#include "ECS/Weather/Atmos.h"
#include "ECS/Weather/Climate.h"
#include "ECS/Weather/Rain.h"
#include "ECS/Weather/StormClouds.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/WeatherInfo.h"

namespace openblack::weather
{
/// What the weather keeps between turns (ecs::systems::WeatherSystemInterface owns it); the same initial values the
/// game starts with
struct State
{
	// the atmosphere grid and the ambient weather
	std::array<WeatherInfo, atmos::k_GridSize * atmos::k_GridSize> atmosGrid {};
	WeatherInfo ambient {};
	uint8_t atmosFrame {1};

	// the climates, newest first, and the world's one
	std::list<climate::Climate> climates;
	climate::Climate* worldClimate {nullptr};
	bool climateSystem {true};
	bool stormCreation {true};
	int32_t lastDay {0};
	uint32_t lastSeason {0};
	uint32_t season {0};
	int32_t nextClimateId {1};

	// the rain
	std::array<rain::Drop, rain::k_Drops> drops {};
	float rainElevation {160.0f};
	float rainFallSpeed {1.0f};
	bool rainDrawn {false};

	// the storms, newest first (a new storm goes at the head), their clouds and the lightning callbacks
	std::list<storms::Storm> storms;
	storms::StormId nextStormId {1};
	storms::LightningCallback forkCallback;
	storms::LightningCallback sheetCallback;
	std::unordered_map<storms::StormId, std::vector<storm_clouds::Puff>> puffs;

	// the weather things (entities with components::WeatherThing), newest first
	std::list<entt::entity> things;
};
} // namespace openblack::weather
