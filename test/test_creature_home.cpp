/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The home of a creature with the temple pen that the caller found gives the same home as the search for the pen

#include <optional>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureMode.h"
#include "Creature/TemplePen.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/CreatureHome.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace home = openblack::ecs::creature_home;

namespace
{
entt::entity MakeCreature(ecs::Registry& registry, std::optional<glm::vec3> leashHome)
{
	const auto creature = registry.Create();
	registry.Assign<Creature>(creature, Creature {.owner = PlayerNames::PLAYER_ONE});
	if (leashHome.has_value())
	{
		auto& leash = registry.Assign<CreatureLeash>(creature);
		leash.home = leashHome;
	}
	return creature;
}
} // namespace

TEST(CreatureHome, AHomeWithNoTempleIsTheSameAsTheSearchGives)
{
	ecs::Registry registry;
	const auto none = MakeCreature(registry, std::nullopt);
	const auto leashed = MakeCreature(registry, glm::vec3 {120.0f, 3.0f, 450.5f});
	const auto notCreature = registry.Create();
	// The player has no temple, thus the search finds no pen
	for (const auto thing : {none, leashed, notCreature})
	{
		EXPECT_EQ(home::HomeOf(registry, thing, std::nullopt), home::HomeOf(registry, thing));
		EXPECT_EQ(home::HomeOnMapOf(registry, thing, std::nullopt), home::HomeOnMapOf(registry, thing));
	}
	EXPECT_FALSE(home::HomeOf(registry, none, std::nullopt).has_value());
	ASSERT_TRUE(home::HomeOf(registry, leashed, std::nullopt).has_value());
	EXPECT_EQ(*home::HomeOf(registry, leashed, std::nullopt), (glm::vec3 {120.0f, 3.0f, 450.5f}));
}

TEST(CreatureHome, AHomeWithATemplePenIsThePen)
{
	ecs::Registry registry;
	const glm::vec3 pen {800.0f, 10.0f, 900.0f};
	const auto none = MakeCreature(registry, std::nullopt);
	const auto leashed = MakeCreature(registry, glm::vec3 {120.0f, 3.0f, 450.5f});
	for (const auto creature : {none, leashed})
	{
		EXPECT_EQ(home::HomeOf(registry, creature, pen), creature_mode::PenOf(std::nullopt, pen, glm::vec3 {}));
		EXPECT_EQ(home::HomeOnMapOf(registry, creature, pen), temple_pen::MapPosition(pen));
	}
	// A thing that is not a creature has no home
	EXPECT_FALSE(home::HomeOf(registry, registry.Create(), pen).has_value());
	EXPECT_FALSE(home::HomeOnMapOf(registry, registry.Create(), pen).has_value());
}
