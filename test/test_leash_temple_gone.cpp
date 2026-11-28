/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/LeashSystem.h"
#include "Locator.h"

namespace
{

using namespace openblack::ecs::components;
using openblack::LeashType;
using openblack::Locator;
using openblack::PlayerNames;
using openblack::ecs::systems::LeashSystem;

class LeashTempleGone: public ::testing::Test
{
protected:
	void SetUp() override
	{
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<openblack::ecs::Registry>();
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	static entt::entity MakeTemple(PlayerNames owner)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto temple = registry.Create();
		registry.Assign<Temple>(temple, Temple {.owner = owner});
		return temple;
	}

	/// A post with its glow, on the temple, or at points of its own when the temple is null
	static entt::entity MakePost(PlayerNames owner, entt::entity temple, LeashType type = LeashType::Rope)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto post = registry.Create();
		const auto glow = registry.Create();
		registry.Assign<Transform>(glow, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<LeashPost>(post, LeashPost {.type = type, .owner = owner, .glow = glow, .temple = temple});
		return post;
	}

	/// The player, with the temple that the player got last as its own temple
	static void MakePlayer(PlayerNames name, entt::entity ownTemple)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto player = registry.Create();
		registry.Assign<Player>(player, Player {.name = name, .temple = ownTemple});
	}

	/// A creature of the player on which a leash can go
	static entt::entity MakeCreature(PlayerNames owner)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto creature = registry.Create();
		registry.Assign<Transform>(creature, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Creature>(creature, Creature {.owner = owner, .leashable = true});
		registry.Assign<CreatureLeash>(creature);
		return creature;
	}

	LeashSystem _leashes;
};

TEST_F(LeashTempleGone, TheLeashesGoWithTheirTemple)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto temple = MakeTemple(PlayerNames::PLAYER_ONE);
	const auto post = MakePost(PlayerNames::PLAYER_ONE, temple);
	const auto glow = registry.Get<const LeashPost>(post).glow;
	_leashes.BeginTurn();
	EXPECT_TRUE(registry.Valid(post));
	registry.Destroy(temple);
	_leashes.BeginTurn();
	EXPECT_FALSE(registry.Valid(post));
	EXPECT_FALSE(registry.Valid(glow));
}

TEST_F(LeashTempleGone, TheLeashesOfAnotherTempleStay)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto first = MakeTemple(PlayerNames::PLAYER_ONE);
	const auto second = MakeTemple(PlayerNames::PLAYER_TWO);
	const auto gone = MakePost(PlayerNames::PLAYER_ONE, first);
	const auto stays = MakePost(PlayerNames::PLAYER_TWO, second);
	registry.Destroy(first);
	_leashes.BeginTurn();
	EXPECT_FALSE(registry.Valid(gone));
	EXPECT_TRUE(registry.Valid(stays));
}

TEST_F(LeashTempleGone, TheLeashesStayWhileTheTempleIsInItsDestruction)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto temple = MakeTemple(PlayerNames::PLAYER_ONE);
	auto& component = registry.Get<Temple>(temple);
	component.destroying = true;
	component.destructionClock = 22.0f;
	const auto post = MakePost(PlayerNames::PLAYER_ONE, temple);
	_leashes.BeginTurn();
	EXPECT_TRUE(registry.Valid(post));
}

TEST_F(LeashTempleGone, LeashesWithNoTempleStay)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto post = MakePost(PlayerNames::PLAYER_ONE, entt::null);
	_leashes.BeginTurn();
	EXPECT_TRUE(registry.Valid(post));
}

TEST_F(LeashTempleGone, EachTempleOfOnePlayerKeepsItsOwnLeashes)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto first = MakeTemple(PlayerNames::PLAYER_ONE);
	const auto second = MakeTemple(PlayerNames::PLAYER_ONE);
	const auto onFirst = MakePost(PlayerNames::PLAYER_ONE, first);
	const auto onSecond = MakePost(PlayerNames::PLAYER_ONE, second);
	_leashes.BeginTurn();
	_leashes.BeginTurn();
	EXPECT_TRUE(registry.Valid(onFirst));
	EXPECT_TRUE(registry.Valid(onSecond));
}

TEST_F(LeashTempleGone, ATapPicksTheLeashOnlyAtItsOwnTemple)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto first = MakeTemple(PlayerNames::PLAYER_ONE);
	const auto second = MakeTemple(PlayerNames::PLAYER_ONE);
	const auto onFirst = MakePost(PlayerNames::PLAYER_ONE, first);
	const auto onSecond = MakePost(PlayerNames::PLAYER_ONE, second);
	registry.Get<LeashPost>(onSecond).selected = true;
	EXPECT_TRUE(_leashes.TapPost(PlayerNames::PLAYER_ONE, onFirst));
	EXPECT_TRUE(registry.Get<const LeashPost>(onFirst).selected);
	EXPECT_TRUE(registry.Get<const LeashPost>(onSecond).selected);
}

TEST_F(LeashTempleGone, ATempleThatCannotHangLeashesMakesNone)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto temple = MakeTemple(PlayerNames::PLAYER_ONE);
	registry.Assign<Transform>(temple, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Mesh>(temple, entt::hashed_string("temple/b_first_temple_l3d"), static_cast<int8_t>(0),
	                      static_cast<int8_t>(0));
	_leashes.BeginTurn();
	EXPECT_EQ(registry.Size<LeashPost>(), 0U);
}

TEST_F(LeashTempleGone, OnlyTheOwnTempleOfThePlayerPicksTheLeashThatGoesOn)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto older = MakeTemple(PlayerNames::PLAYER_ONE);
	const auto own = MakeTemple(PlayerNames::PLAYER_ONE);
	MakePlayer(PlayerNames::PLAYER_ONE, own);
	const auto onOlder = MakePost(PlayerNames::PLAYER_ONE, older, LeashType::Evil);
	const auto onOwn = MakePost(PlayerNames::PLAYER_ONE, own, LeashType::Evil);
	const auto creature = MakeCreature(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(_leashes.ComputerGodChangeType(creature, LeashType::Evil));
	EXPECT_TRUE(registry.Get<const LeashPost>(onOwn).selected);
	// The older temple keeps its own pick
	EXPECT_FALSE(registry.Get<const LeashPost>(onOlder).selected);
}

TEST_F(LeashTempleGone, APlayerWhoseOwnTempleWentPicksNoLeash)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto older = MakeTemple(PlayerNames::PLAYER_ONE);
	const auto own = MakeTemple(PlayerNames::PLAYER_ONE);
	MakePlayer(PlayerNames::PLAYER_ONE, own);
	const auto onOlder = MakePost(PlayerNames::PLAYER_ONE, older, LeashType::Evil);
	registry.Destroy(own);
	const auto creature = MakeCreature(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(_leashes.ComputerGodChangeType(creature, LeashType::Evil));
	// The older temple does not become the own temple of the player
	EXPECT_FALSE(registry.Get<const LeashPost>(onOlder).selected);
}

TEST_F(LeashTempleGone, ALeashAtPointsOfItsOwnTakesThePick)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto post = MakePost(PlayerNames::PLAYER_ONE, entt::null, LeashType::Evil);
	const auto creature = MakeCreature(PlayerNames::PLAYER_ONE);
	EXPECT_TRUE(_leashes.ComputerGodChangeType(creature, LeashType::Evil));
	EXPECT_TRUE(registry.Get<const LeashPost>(post).selected);
}

} // namespace
