/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HealDebugHooks.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/SpecularColour.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "MagicTables.h"
#include "Script/CHLSpells.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// the hurt villagers, with the life and glow (specular red) last logged
struct HurtVillager
{
	entt::entity entity;
	float life;
	int glow;
};
/// the parsed hook; its defaults (and the 30 m / radius 10 cast below) are test harness values, not from the original
struct Test
{
	float x {0.0f};
	float z {0.0f};
	float radius {10.0f};
	float life {0.3f};
	int poisoned {0};
	int turn {1};
	int heal {0};
	int repeat {0};
};
/// What these hooks keep between calls, in the debug hooks' store (Locator::debugHooks)
struct HealDebugHooksState
{
	bool done {false};
	int turns {0};
	std::vector<HurtVillager> hurt {};
	Test test {};
	bool active {false};
};

HealDebugHooksState& HealDebugHooksData()
{
	return openblack::Locator::debugHooks::value().Get<HealDebugHooksState>();
}

void Hurt(const Test& test)
{
	auto& registry = Locator::entitiesRegistry::value();
	int count = 0;
	registry.Each<const Villager, const Transform>([&](entt::entity entity, const Villager&, const Transform& transform) {
		if (glm::length(glm::vec2(transform.position.x - test.x, transform.position.z - test.z)) > test.radius)
		{
			return;
		}
		ecs::life::SetLife(entity, test.life);
		if (test.poisoned != 0 && !registry.AllOf<Poisoned>(entity))
		{
			registry.Assign<Poisoned>(entity);
		}
		++count;
		const auto known = std::find_if(HealDebugHooksData().hurt.begin(), HealDebugHooksData().hurt.end(),
		                                [entity](const auto& h) { return h.entity == entity; });
		if (known == HealDebugHooksData().hurt.end())
		{
			HealDebugHooksData().hurt.push_back({entity, test.life, -1});
		}
		else
		{
			known->life = test.life;
		}
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Heal test: turn +{} villager {} at ({:.1f}, {:.1f}) hurt to life {:.3f}{}",
		                   HealDebugHooksData().turns, static_cast<uint32_t>(entity), transform.position.x,
		                   transform.position.z, test.life, test.poisoned != 0 ? ", poisoned" : "");
	});
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Heal test: turn +{} {} villagers hurt within {:.1f} m of ({:.1f}, {:.1f})",
	                   HealDebugHooksData().turns, count, test.radius, test.x, test.z);
	if (test.heal != 0)
	{
		// as the script's SPELL_AT_POS (the neutral creator), from 30 m above, with the player's timer
		const auto type = test.heal == 2 ? MagicType::HealPowerUpOne : MagicType::Heal;
		const float ground = Locator::terrainSystem::value().GetHeightAt(glm::vec2(test.x, test.z));
		const glm::vec3 target(test.x, ground, test.z);
		const float duration = magic::GetTimerWhenPlayerCasting(Locator::infoConstants::value(), type);
		const auto spell = magic::script::CastSpellAtPos(target, type, target + glm::vec3(0.0f, 30.0f, 0.0f), {}, false, 10.0f,
		                                                 duration, 0.0f, glm::vec3(0.0f));
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Heal test: turn +{} {} cast at ({:.1f}, {:.1f}) -> spell {}",
		                   HealDebugHooksData().turns, test.heal == 2 ? "HEAL_PU_ONE" : "HEAL", test.x, test.z,
		                   spell == entt::null ? -1 : static_cast<int>(spell));
	}
}

void Report()
{
	auto& registry = Locator::entitiesRegistry::value();
	for (auto& hurt : HealDebugHooksData().hurt)
	{
		if (!registry.Valid(hurt.entity))
		{
			continue;
		}
		const float life = ecs::life::LifeOf(hurt.entity);
		const auto* specular = registry.TryGet<const SpecularColour>(hurt.entity);
		// -1 = no chakra on it (no SpecularColour component)
		const int glow = specular != nullptr ? static_cast<int>(specular->colour.r) : -1;
		// the life at once, the glow (the chakra's specular) every 50 of red, and when the chakra starts or ends
		if (life != hurt.life || glow / 50 != hurt.glow / 50 || (glow < 0) != (hurt.glow < 0))
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Heal test: turn +{} villager {} life {:.3f} -> {:.3f}, poisoned {}, glow (specular) {}",
			                   HealDebugHooksData().turns, static_cast<uint32_t>(hurt.entity), hurt.life, life,
			                   registry.AllOf<Poisoned>(hurt.entity),
			                   specular != nullptr ? fmt::format("{},{},{}", int {specular->colour.r}, int {specular->colour.g},
			                                                     int {specular->colour.b})
			                                       : std::string("none"));
			hurt.life = life;
			hurt.glow = glow;
		}
	}
}
} // namespace

void magic::heal_debug::RunDebugHooks()
{
	if (!Locator::terrainSystem::has_value() || !Locator::entitiesRegistry::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	if (!HealDebugHooksData().done)
	{
		HealDebugHooksData().done = true;
		if (const char* value = std::getenv("OPENBLACK_TEST_HURT_VILLAGERS"); value != nullptr)
		{
			Test test;
			if (std::sscanf(value, "%f,%f,%f,%f,%d,%d,%d,%d", &test.x, &test.z, &test.radius, &test.life, &test.poisoned,
			                &test.turn, &test.heal, &test.repeat) < 4)
			{
				SPDLOG_LOGGER_WARN(spdlog::get("game"), "Heal test: OPENBLACK_TEST_HURT_VILLAGERS=\"{}\" not understood",
				                   value);
				return;
			}
			HealDebugHooksData().test = test;
			HealDebugHooksData().active = true;
		}
	}
	if (!HealDebugHooksData().active)
	{
		return;
	}
	++HealDebugHooksData().turns;
	if (HealDebugHooksData().turns == HealDebugHooksData().test.turn ||
	    (HealDebugHooksData().test.repeat > 0 && HealDebugHooksData().turns > HealDebugHooksData().test.turn &&
	     (HealDebugHooksData().turns - HealDebugHooksData().test.turn) % HealDebugHooksData().test.repeat == 0))
	{
		Hurt(HealDebugHooksData().test);
	}
	Report();
}

void magic::heal_debug::ResetDebugHooks()
{
	HealDebugHooksData().done = false;
	HealDebugHooksData().active = false;
	HealDebugHooksData().turns = 0;
	HealDebugHooksData().hurt.clear();
}
