/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Town.h"
#include "ECS/PlayerTowns.h"
#include "ECS/Registry.h"

using openblack::PlayerNames;
using openblack::ecs::Registry;
using openblack::ecs::components::Town;

namespace
{

entt::entity MakeTown(Registry& registry, uint32_t id, PlayerNames owner, uint32_t gained)
{
	const auto entity = registry.Create();
	auto& town = registry.Assign<Town>(entity);
	town.id = id;
	town.owner = owner;
	town.gained = gained;
	return entity;
}

} // namespace

TEST(PlayerTowns, ThePlayersTownsComeInTheOrderThePlayerGotThem)
{
	Registry registry;
	const auto won = MakeTown(registry, 1, PlayerNames::PLAYER_ONE, 6);
	const auto first = MakeTown(registry, 2, PlayerNames::PLAYER_ONE, 0);
	MakeTown(registry, 3, PlayerNames::NEUTRAL, 1);
	const auto second = MakeTown(registry, 4, PlayerNames::PLAYER_ONE, 2);
	EXPECT_EQ(openblack::ecs::player_towns::InOrder(registry, PlayerNames::PLAYER_ONE),
	          (std::vector<entt::entity> {first, second, won}));
	EXPECT_TRUE(openblack::ecs::player_towns::InOrder(registry, PlayerNames::PLAYER_TWO).empty());
}

TEST(PlayerTowns, TheTotalCountsOnlyThePlayersOwnTowns)
{
	Registry registry;
	MakeTown(registry, 1, PlayerNames::PLAYER_ONE, 0);
	MakeTown(registry, 2, PlayerNames::PLAYER_TWO, 0);
	MakeTown(registry, 3, PlayerNames::PLAYER_ONE, 1);
	MakeTown(registry, 4, PlayerNames::NEUTRAL, 0);
	EXPECT_EQ(openblack::ecs::player_towns::Total(registry, PlayerNames::PLAYER_ONE), 2U);
	EXPECT_EQ(openblack::ecs::player_towns::Total(registry, PlayerNames::PLAYER_TWO), 1U);
	EXPECT_EQ(openblack::ecs::player_towns::Total(registry, PlayerNames::PLAYER_THREE), 0U);
}
