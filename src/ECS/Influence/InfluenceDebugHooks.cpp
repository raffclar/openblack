/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Test hooks of the influence (documented in docs/bw1-notes/openblack-internals.md):
//   OPENBLACK_TEST_INFLUENCE="x,z[;x,z...]"   logs what GET_INFLUENCE(player 0, raw 0, pos) gives at each point, with
//                                              the sources (citadel and town radii), on turns 2 and 100
//   OPENBLACK_TEST_INFLUENCE_RING="x,z,radius[,anti[,player]]"  an INFLUENCE_POSITION ring on turn 1
//   OPENBLACK_INFLUENCE_EVERYWHERE=1           the original's GatheringFlag (influence 1 everywhere)

#include <cstdint>
#include <cstdlib>

#include <sstream>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "Influence.h"
#include "InfluenceState.h"
#include "Locator.h"

namespace
{
/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct InfluenceDebugHooksState
{
	uint32_t runDebugHooksTurn {0};
};

InfluenceDebugHooksState& InfluenceDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<InfluenceDebugHooksState>();
}
} // namespace

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
std::vector<float> ParseNumbers(const std::string& text)
{
	std::vector<float> values;
	std::stringstream stream(text);
	std::string item;
	while (std::getline(stream, item, ','))
	{
		values.push_back(std::strtof(item.c_str(), nullptr));
	}
	return values;
}

float GroundAt(float x, float z)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	try
	{
		return Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
	}
	catch (...)
	{
		return 0.0f; // no island
	}
}

void LogSources()
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Each<const Temple, const Transform>([](entt::entity entity, const Temple& temple, const Transform& transform) {
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Influence: citadel of player {} at ({:.1f}, {:.1f}): radius {:.2f}",
		                   static_cast<int>(temple.owner), transform.position.x, transform.position.z,
		                   influence::CitadelRadius(entity));
	});
	registry.Each<const Town, const TownInfluence, const Transform>([](const Town& town, const TownInfluence& influence,
	                                                                   const Transform& transform) {
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Influence: town {} of player {} at ({:.1f}, {:.1f}): radius {:.2f}", town.id,
		                   static_cast<int>(town.owner), transform.position.x, transform.position.z, influence.radius);
	});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Influence: land {}, town x{:.2f}, player x{:.2f}, {} rings",
	                   influence::LandNumber(), influence::TownInfluenceMultiplier(), influence::PlayerInfluenceMultiplier(),
	                   influence::detail::GlobalsOrDefault().rings.size());
}

void LogPoints(const std::string& points, uint32_t turn)
{
	LogSources();
	std::stringstream stream(points);
	std::string point;
	while (std::getline(stream, point, ';'))
	{
		const auto values = ParseNumbers(point);
		if (values.size() < 2)
		{
			continue;
		}
		const glm::vec3 position(values[0], GroundAt(values[0], values[1]), values[1]);
		// the script's GET_INFLUENCE with player 0 (the local player) and raw 0 (allies on)
		const float value = influence::CalculatePlayerInfluence(PlayerNames::PLAYER_ONE, position);
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Influence (turn {}): GET_INFLUENCE(0, 0, ({:.1f}, {:.1f}, {:.1f})) = {:.4f}; raw {:.4f}; anti {}",
		                   turn, position.x, position.y, position.z, value,
		                   influence::CalculatePlayerRawInfluence(PlayerNames::PLAYER_ONE, position),
		                   influence::IsInAntiInfluence(PlayerNames::PLAYER_ONE, position));
	}
}
} // namespace

namespace openblack::influence
{
void RunDebugHooks()
{
	auto& s_turn = InfluenceDebugHooksData().runDebugHooksTurn;
	static const char* s_points = std::getenv("OPENBLACK_TEST_INFLUENCE");
	static const char* s_ring = std::getenv("OPENBLACK_TEST_INFLUENCE_RING");
	++s_turn;
	if (s_turn == 1)
	{
		if (std::getenv("OPENBLACK_INFLUENCE_EVERYWHERE") != nullptr)
		{
			SetInfluenceEverywhere(true);
		}
		if (s_ring != nullptr)
		{
			const auto values = ParseNumbers(s_ring);
			if (values.size() >= 3)
			{
				const bool anti = values.size() >= 4 && values[3] != 0.0f;
				const auto player =
				    values.size() >= 5 ? static_cast<PlayerNames>(static_cast<int>(values[4])) : PlayerNames::PLAYER_ONE;
				const glm::vec3 position(values[0], GroundAt(values[0], values[1]), values[1]);
				CreateRing(position, player, values[2], anti);
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Influence: test ring at ({:.1f}, {:.1f}) radius {} anti {} player {}",
				                   values[0], values[1], values[2], anti, static_cast<int>(player));
			}
		}
	}
	if (s_points != nullptr && (s_turn == 2 || s_turn == 100))
	{
		LogPoints(s_points, s_turn);
	}
}
} // namespace openblack::influence
