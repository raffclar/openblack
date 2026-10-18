/*******************************************************************************
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
#include "ECS/Registry.h"
#include "ECS/TownTurnOrder.h"

using namespace openblack;
using namespace openblack::ecs;
using openblack::ecs::components::Town;

namespace
{

entt::entity MakeTown(Registry& registry, uint32_t id, PlayerNames owner, uint32_t gained)
{
	const auto entity = registry.Create();
	auto& town = registry.Assign<Town>(entity, id, owner);
	town.gained = gained;
	return entity;
}

} // namespace

TEST(TownTurnOrder, APlayersTownsTakeTheirTurnsInTheOrderThePlayerGainedThem)
{
	Registry registry;
	const auto third = MakeTown(registry, 0, PlayerNames::PLAYER_ONE, 7);
	const auto first = MakeTown(registry, 1, PlayerNames::PLAYER_ONE, 2);
	const auto other = MakeTown(registry, 2, PlayerNames::PLAYER_TWO, 0);
	const auto second = MakeTown(registry, 3, PlayerNames::PLAYER_ONE, 5);
	EXPECT_EQ(town_turns::TownsInGainedOrder(registry, PlayerNames::PLAYER_ONE),
	          (std::vector<entt::entity> {first, second, third}));
	EXPECT_EQ(town_turns::TownsInGainedOrder(registry, PlayerNames::PLAYER_TWO), (std::vector<entt::entity> {other}));
	EXPECT_TRUE(town_turns::TownsInGainedOrder(registry, PlayerNames::NEUTRAL).empty());
}
