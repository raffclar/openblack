/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StormDebugHooks.h"

#include <cstdio>
#include <cstdlib>

#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Spell.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ScreenshotRequestSystemInterface.h"
#include "ECS/Weather/LightningFlash.h"
#include "ECS/Weather/Weather.h"
#include "Game.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Particles/PSysManager.h"
#include "Particles/Rules/Storm.h"

using namespace openblack;

namespace
{
struct Shot
{
	unsigned int turns;
	std::string path;
	bool done {false};
};

std::vector<Shot> ParseShots(const char* name)
{
	std::vector<Shot> list;
	const char* value = std::getenv(name);
	if (value == nullptr)
	{
		return list;
	}
	std::stringstream stream(value);
	std::string item;
	while (std::getline(stream, item, ';'))
	{
		const auto comma = item.find(',');
		if (comma != std::string::npos)
		{
			list.push_back({static_cast<unsigned int>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1)});
		}
	}
	return list;
}

/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct StormDebugHooksState
{
	bool haveFirst {false};
	unsigned int firstTurn {0};
	std::optional<std::vector<Shot>> strikeShots; // OPENBLACK_TEST_STORM_STRIKE_SHOT, parsed on first use
	std::optional<std::vector<Shot>> shots;       // OPENBLACK_TEST_STORM_SHOT, parsed on first use
};

StormDebugHooksState& StormDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<StormDebugHooksState>();
}

/// OPENBLACK_TEST_STORM_STRIKE_SHOT="<n>,<path>[;...]": a screenshot at the n-th strike from the clouds
std::vector<Shot>& StrikeShots()
{
	auto& shots = StormDebugHooksData().strikeShots;
	if (!shots.has_value())
	{
		shots = ParseShots("OPENBLACK_TEST_STORM_STRIKE_SHOT");
	}
	return *shots;
}

std::vector<Shot>& Shots()
{
	auto& shots = StormDebugHooksData().shots;
	if (shots.has_value())
	{
		return *shots;
	}
	shots = [] {
		std::vector<Shot> list;
		const char* value = std::getenv("OPENBLACK_TEST_STORM_SHOT");
		if (value == nullptr)
		{
			return list;
		}
		std::stringstream stream(value);
		std::string item;
		while (std::getline(stream, item, ';'))
		{
			const auto comma = item.find(',');
			if (comma != std::string::npos)
			{
				list.push_back({static_cast<unsigned int>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1)});
			}
		}
		return list;
	}();
	return *shots;
}

} // namespace

void magic::storm_debug::OnTurn(entt::entity spell)
{
	const unsigned int turn = CurrentTurn();
	if (!StormDebugHooksData().haveFirst)
	{
		StormDebugHooksData().haveFirst = true;
		StormDebugHooksData().firstTurn = turn;
		// OPENBLACK_TEST_STORM_PILE="x,z,amount[,wood]": a food (or wood) pile there when the first storm is cast, for
		// the tornado to take (a test helper: PotArchetype::Create, as a dropped pile)
		if (const char* pile = std::getenv("OPENBLACK_TEST_STORM_PILE"); pile != nullptr && Locator::terrainSystem::has_value())
		{
			float x = 0.0f;
			float z = 0.0f;
			int amount = 0;
			std::array<char, 16> kind = {};
			if (std::sscanf(pile, "%f,%f,%d,%15s", &x, &z, &amount, kind.data()) >= 3)
			{
				const glm::vec3 position(x, Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)), z);
				const auto type = std::string(kind.data()) == "wood" ? PotInfo::WoodPile_5 : PotInfo::FoodPile;
				const auto created = ecs::archetypes::PotArchetype::Create(position, 0.0f, type, amount);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Storm test: pile {} of {} at ({:.1f}, {:.1f}) -> entity {}",
				                   static_cast<int>(type), amount, x, z, static_cast<uint32_t>(created));
			}
		}
	}
	if (Locator::screenshotRequest::has_value())
	{
		for (auto& shot : Shots())
		{
			if (!shot.done && turn >= StormDebugHooksData().firstTurn + shot.turns)
			{
				shot.done = true;
				Locator::screenshotRequest::value().Request(shot.path);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Storm test: screenshot {} turns after the first storm (turn {}) -> {}",
				                   shot.turns, turn, shot.path);
				break; // one request per turn
			}
		}
	}
	if (Locator::screenshotRequest::has_value())
	{
		for (auto& shot : StrikeShots())
		{
			if (!shot.done && psys::storm::StrikeCount() >= shot.turns)
			{
				shot.done = true;
				Locator::screenshotRequest::value().Request(shot.path);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Storm test: screenshot at strike {} (turn {}) -> {}", shot.turns, turn,
				                   shot.path);
				break;
			}
		}
	}
	if (!psys::storm::TraceEnabled() || (turn - StormDebugHooksData().firstTurn) % 10 != 0)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& component = registry.Get<const ecs::components::Spell>(spell);
	const glm::vec3 centre = ToWorld(glm::vec3(component.position.x, 0.0f, component.position.z));
	const auto weather = weather::ComputeWeather(centre);
	const auto* effect = psys::manager::Find(component.psys);
	uint8_t flash = 0;
	if (Locator::camera::has_value())
	{
		flash = weather::LightningFlashAtCamera(centre);
	}
	SPDLOG_LOGGER_INFO(
	    spdlog::get("game"),
	    "Storm trace: turn {} spell {} age {:.1f} chants {:.0f} closed {} at ({:.1f}, {:.1f}): rain {} overcast {} "
	    "wind ({}, {}) temp {} | atoms {} carried {} flash {}",
	    turn, static_cast<uint32_t>(spell), component.age, component.chants, component.closedDown, component.position.x,
	    component.position.z, weather.rain, weather.overcast, weather.windX, weather.windZ, weather.temperature,
	    effect != nullptr ? effect->AtomCount() : 0, psys::storm::CarriedObjectCount(), flash);
}

void magic::storm_debug::Reset()
{
	StormDebugHooksData().haveFirst = false;
	for (auto& shot : Shots())
	{
		shot.done = false;
	}
}
