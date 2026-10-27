/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <entt/entity/entity.hpp>
#include <gtest/gtest.h>

#include "Commands/PlayerCommands.h"
#include "ECS/Systems/Implementations/MagicSystem.h"
#include "Enums.h"

using namespace openblack;
using openblack::ecs::systems::MagicSystem;

// A cast or a let go that comes at the turn with no miracle in the hand changes nothing
TEST(HandCastCommands, ACastWithAnEmptyHandDoesNothing)
{
	MagicSystem magic;
	magic.DoCast(PlayerNames::PLAYER_ONE, player_commands::HandCast {.point = {1.0f, 2.0f, 3.0f}, .size = 4.0f});
	magic.DoCastUnlock(PlayerNames::PLAYER_ONE, static_cast<entt::entity>(5));
	magic.InterfaceTurn();
	EXPECT_FALSE(magic.GetHeldSeed().has_value());
	EXPECT_EQ(magic.GetLastHandResult(), MagicSystem::HandResult::None);
}

// Another player whose hand holds nothing: the cast and the let go of that player change nothing
TEST(HandCastCommands, ACastOfAnotherPlayerWithAnEmptyHandDoesNothing)
{
	MagicSystem magic;
	magic.DoCast(PlayerNames::PLAYER_TWO, player_commands::HandCast {.object = static_cast<entt::entity>(7)});
	magic.DoCastUnlock(PlayerNames::PLAYER_TWO, static_cast<entt::entity>(7));
	EXPECT_FALSE(magic.GetHeldSeed().has_value());
	EXPECT_EQ(magic.GetLastHandResult(), MagicSystem::HandResult::None);
}
