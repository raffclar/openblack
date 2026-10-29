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

TEST(PlayerTowns, AllTheTownsComeInTheListOfEachPlayerInTurnAndTheNeutralListLast)
{
	Registry registry;
	const auto neutral = MakeTown(registry, 1, PlayerNames::NEUTRAL, 0);
	const auto twoWon = MakeTown(registry, 2, PlayerNames::PLAYER_TWO, 5);
	const auto oneWon = MakeTown(registry, 3, PlayerNames::PLAYER_ONE, 4);
	const auto twoFirst = MakeTown(registry, 4, PlayerNames::PLAYER_TWO, 1);
	const auto oneFirst = MakeTown(registry, 5, PlayerNames::PLAYER_ONE, 2);
	EXPECT_EQ(openblack::ecs::player_towns::AllInOrder(registry),
	          (std::vector<entt::entity> {oneFirst, oneWon, twoFirst, twoWon, neutral}));
}

TEST(PlayerTowns, ATownWonLaterComesAfterATownMadeLater)
{
	// The list order is the order of gain, not the order the land made the towns
	Registry registry;
	const auto won = MakeTown(registry, 1, PlayerNames::PLAYER_ONE, 3);
	const auto made = MakeTown(registry, 2, PlayerNames::PLAYER_ONE, 1);
	EXPECT_EQ(openblack::ecs::player_towns::AllInOrder(registry), (std::vector<entt::entity> {made, won}));
}
