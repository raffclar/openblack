/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Test hooks of the fire (documented in docs/bw1-notes/openblack-internals.md):
//   OPENBLACK_TEST_FIRE="x,z,T[,kind[,turn]]"  FireEffect::SetTemperature of the nearest object with a fire info to
//                                              (x, z) (kind: any, tree, abode, villager, field), at that game turn
//                                              (default: the first one with the land); T <= 1: SetOnFire(T) instead
//   OPENBLACK_FIRE_TRACE=1                     the fire's log: new fires, ignitions, deaths, reactions, villagers
// Each runs once per land.

#include "FireDebugHooks.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <array>
#include <limits>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "FireEffect.h"
#include "FireObjectTraits.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct FireDebugHooksState
{
	bool done {false};
	uint32_t firstTurn {0};
	bool haveFirstTurn {false};
};

FireDebugHooksState& FireDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<FireDebugHooksState>();
}

bool KindMatches(entt::entity entity, const char* kind)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (kind[0] == '\0' || std::strcmp(kind, "any") == 0)
	{
		return true;
	}
	if (std::strcmp(kind, "tree") == 0)
	{
		return registry.AnyOf<Tree, DeadTree>(entity);
	}
	if (std::strcmp(kind, "abode") == 0)
	{
		return registry.AllOf<Abode>(entity) && !registry.AllOf<Field>(entity);
	}
	if (std::strcmp(kind, "villager") == 0)
	{
		return registry.AllOf<Villager>(entity);
	}
	if (std::strcmp(kind, "field") == 0)
	{
		return registry.AllOf<Field>(entity);
	}
	return true;
}

/// Nearest object in x, z that the fire can take
entt::entity Nearest(float x, float z, const char* kind)
{
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity best = entt::null;
	float bestDistance = std::numeric_limits<float>::max();
	registry.Each<const Transform>([&](entt::entity entity, const Transform& transform) {
		if (fire::traits::CombustionTemperature(entity) == 0.0f || !KindMatches(entity, kind))
		{
			return;
		}
		const float d = glm::length(glm::vec2(transform.position.x - x, transform.position.z - z));
		if (d < bestDistance)
		{
			bestDistance = d;
			best = entity;
		}
	});
	return best;
}
} // namespace

void fire::ResetDebugHooks()
{
	FireDebugHooksData().done = false;
	FireDebugHooksData().haveFirstTurn = false;
}

void fire::RunDebugHooks(uint32_t turn)
{
	if (FireDebugHooksData().done || !Locator::terrainSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	const char* value = std::getenv("OPENBLACK_TEST_FIRE");
	if (value == nullptr)
	{
		FireDebugHooksData().done = true;
		return;
	}
	if (!FireDebugHooksData().haveFirstTurn)
	{
		FireDebugHooksData().haveFirstTurn = true;
		FireDebugHooksData().firstTurn = turn;
	}
	float x = 0.0f;
	float z = 0.0f;
	float temperature = 0.0f;
	std::array<char, 32> kind = {};
	int atTurn = -1;
	const int read = std::sscanf(value, "%f,%f,%f,%31[^,],%d", &x, &z, &temperature, kind.data(), &atTurn);
	if (read < 3)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Fire test: OPENBLACK_TEST_FIRE=\"{}\" not understood", value);
		FireDebugHooksData().done = true;
		return;
	}
	if (atTurn >= 0 && turn - FireDebugHooksData().firstTurn < static_cast<uint32_t>(atTurn))
	{
		return;
	}
	FireDebugHooksData().done = true;
	const auto object = Nearest(x, z, kind.data());
	if (object == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Fire test: no object to burn near ({:.1f}, {:.1f})", x, z);
		return;
	}
	const auto& transform = Locator::entitiesRegistry::value().Get<const Transform>(object);
	if (temperature <= 1.0f)
	{
		SetOnFire(object, temperature);
	}
	else
	{
		SetTemperature(object, temperature, entt::null);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Fire test: object {} at ({:.1f}, {:.1f}) Tc {:.0f} cap {:.0f} -> T {:.1f} (turn {})",
	                   static_cast<int>(object), transform.position.x, transform.position.z,
	                   traits::CombustionTemperature(object), traits::HeatCapacity(object), GetTemperature(object), turn);
}
