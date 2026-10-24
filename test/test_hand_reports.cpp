/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <span>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Commands/PlayerCommands.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/GodHand.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandStillness.h"
#include "ECS/Components/ReportedHand.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Simulation/CommandLog.h"

// The test builds its own locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/CreatureHandSystem.h"
#include "ECS/Systems/Implementations/HandSystem.h"
#include "ECS/Systems/Implementations/PlayerCommandSystem.h"

using namespace openblack;
using namespace openblack::player_commands;

namespace
{
entt::entity Entity(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

HandReport ReportAt(glm::vec3 hand)
{
	return HandReport {.collidePoint = hand + glm::vec3(0.0f, -1.0f, 0.0f),
	                   .handPoint = hand,
	                   .camera = glm::vec3(0.0f, 100.0f, 0.0f),
	                   .focus = glm::vec3(0.0f)};
}
} // namespace

TEST(CommandLog, ALineGivesBackTheTurnAndEachCommandWithItsPlayer)
{
	const std::vector<Command> commands {
	    {.player = PlayerNames::PLAYER_ONE, .action = HandTake {.object = Entity(5)}},
	    {.player = PlayerNames::PLAYER_ONE, .action = ReportAt({1.0f, 2.0f, 3.0f})},
	    {.player = PlayerNames::PLAYER_TWO, .action = HandTap {.object = Entity(6)}},
	};
	const auto line = command_log::LineOf(42, commands);
	EXPECT_EQ(line.rfind("{\"turn\":42,\"frame\":\"", 0), 0u);
	const auto turn = command_log::ReadLine(line);
	ASSERT_TRUE(turn.has_value());
	EXPECT_EQ(turn->turn, 42u);
	ASSERT_EQ(turn->commands.size(), 3u);
	EXPECT_EQ(turn->commands[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(std::get<HandTake>(turn->commands[0].action).object, Entity(5));
	EXPECT_EQ(std::get<HandReport>(turn->commands[1].action), ReportAt({1.0f, 2.0f, 3.0f}));
	EXPECT_EQ(turn->commands[2].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(std::get<HandTap>(turn->commands[2].action).object, Entity(6));
}

TEST(CommandLog, ALineThatIsNotATurnDoesNotRead)
{
	EXPECT_FALSE(command_log::ReadLine("").has_value());
	EXPECT_FALSE(command_log::ReadLine("{\"turn\":1,\"frame\":\"0g\"}").has_value());
	EXPECT_FALSE(command_log::ReadLine("{\"turn\":1,\"frame\":\"010\"}").has_value());
	// The frame of a checksum is not a turn
	EXPECT_FALSE(command_log::ReadLine("{\"turn\":1,\"frame\":\"0203010000000200000000000000\"}").has_value());
}

class CommandsOfATurn: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	ecs::systems::PlayerCommandSystem _commands;
	std::vector<std::pair<uint32_t, std::vector<Command>>> _heard;

	void Listen()
	{
		_commands.SetTurnListener([this](uint32_t turn, std::span<const Command> commands) {
			_heard.emplace_back(turn, std::vector<Command>(commands.begin(), commands.end()));
		});
	}
};

TEST_F(CommandsOfATurn, TheListenerHearsTheCommandsThatTheTurnDoes)
{
	Listen();
	_commands.Submit(HandTap {.object = Entity(3)});
	_commands.SendOpenBlock();
	_commands.RunSent(7);
	_commands.RunSent(8);
	ASSERT_EQ(_heard.size(), 2u);
	EXPECT_EQ(_heard[0].first, 7u);
	ASSERT_EQ(_heard[0].second.size(), 1u);
	EXPECT_EQ(std::get<HandTap>(_heard[0].second[0].action).object, Entity(3));
	// A turn with no command is heard too, with none
	EXPECT_EQ(_heard[1].first, 8u);
	EXPECT_TRUE(_heard[1].second.empty());
}

TEST_F(CommandsOfATurn, TheCommandsOfAnotherComputerWaitForTheirTurnAndComeAfterTheOwn)
{
	Listen();
	_commands.Receive(5, {{.player = PlayerNames::PLAYER_TWO, .action = HandTake {.object = Entity(9)}}});
	_commands.RunSent(4);
	_commands.Submit(HandTap {.object = Entity(1)});
	_commands.SendOpenBlock();
	_commands.RunSent(5);
	ASSERT_EQ(_heard.size(), 2u);
	EXPECT_TRUE(_heard[0].second.empty());
	ASSERT_EQ(_heard[1].second.size(), 2u);
	EXPECT_EQ(_heard[1].second[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(_heard[1].second[1].player, PlayerNames::PLAYER_TWO);
	EXPECT_EQ(std::get<HandTake>(_heard[1].second[1].action).object, Entity(9));
}

class ReportedHands: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		_hand = registry.Create();
		registry.Assign<ecs::components::Hand>(_hand, false);
		registry.Assign<ecs::components::Transform>(_hand, glm::vec3(10.0f, 0.0f, 10.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<ecs::components::HandStillness>(_hand);
		registry.Assign<ecs::components::ReportedHand>(_hand);
		_hands.FindAfterLoad();
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	void DrawHandAt(glm::vec3 at) { Locator::entitiesRegistry::value().Get<ecs::components::Transform>(_hand).position = at; }

	ecs::systems::HandSystem _hands;
	entt::entity _hand {entt::null};
};

TEST_F(ReportedHands, TheWorldHasNoHandBeforeTheFirstReport)
{
	EXPECT_FALSE(_hands.GetPlayerHandPositions()[0].has_value());
	// The frames draw it all the same
	EXPECT_EQ(_hands.GetDrawnHandPosition(), glm::vec3(10.0f, 0.0f, 10.0f));
}

TEST_F(ReportedHands, TheWorldHasTheHandWhereTheLastReportPutIt)
{
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({20.0f, 1.0f, 30.0f}));
	DrawHandAt({500.0f, 0.0f, 500.0f});
	EXPECT_EQ(_hands.GetPlayerHandPositions()[0], glm::vec3(20.0f, 1.0f, 30.0f));
	const auto& reported = Locator::entitiesRegistry::value().Get<const ecs::components::ReportedHand>(_hand);
	EXPECT_EQ(reported.collidePoint, glm::vec3(20.0f, 0.0f, 30.0f));
	EXPECT_EQ(reported.camera, glm::vec3(0.0f, 100.0f, 0.0f));
}

TEST_F(ReportedHands, TheTurnCountsTheStillnessOfTheReportedHandNotOfTheDrawnHand)
{
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({20.0f, 0.0f, 20.0f}));
	_hands.ProcessTurn();
	// The drawn hand moves far each turn, the reported one stays
	for (int turn = 0; turn < 3; ++turn)
	{
		DrawHandAt({100.0f * static_cast<float>(turn), 0.0f, 0.0f});
		_hands.ProcessTurn();
	}
	EXPECT_EQ(_hands.GetHandStillTurns(), 3u);
	// A report far away is a move
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({60.0f, 0.0f, 20.0f}));
	_hands.ProcessTurn();
	EXPECT_EQ(_hands.GetHandStillTurns(), 0u);
}

TEST_F(ReportedHands, TheGodOfAnotherPlayerGetsHandsOfItsOwnAndItsReportsMoveOnlyThem)
{
	// Before the command of the game server the player has no hands, and its reports do nothing
	EXPECT_FALSE(_hands.GetHandsOf(PlayerNames::PLAYER_TWO).has_value());
	_hands.Report(PlayerNames::PLAYER_TWO, ReportAt({5.0f, 0.0f, 5.0f}));
	EXPECT_FALSE(_hands.GetPlayerHandPositions()[0].has_value());

	const auto made = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto hand : made)
	{
		ASSERT_TRUE(registry.Valid(hand));
		EXPECT_NE(hand, _hand);
		ASSERT_TRUE(registry.AllOf<ecs::components::GodHand>(hand));
		EXPECT_EQ(registry.Get<const ecs::components::GodHand>(hand).player, PlayerNames::PLAYER_TWO);
	}
	// A second command makes no more hands
	EXPECT_EQ(_hands.MakeHandsOf(PlayerNames::PLAYER_TWO), made);
	EXPECT_EQ(_hands.GetHandsOf(PlayerNames::PLAYER_ONE), _hands.GetPlayerHands());

	_hands.Report(PlayerNames::PLAYER_TWO, ReportAt({5.0f, 0.0f, 5.0f}));
	_hands.Report(PlayerNames::PLAYER_ONE, ReportAt({20.0f, 0.0f, 20.0f}));
	EXPECT_EQ(registry.Get<const ecs::components::ReportedHand>(made[0]).handPoint, glm::vec3(5.0f, 0.0f, 5.0f));
	EXPECT_EQ(_hands.GetPlayerHandPositions()[0], glm::vec3(20.0f, 0.0f, 20.0f));
	// The turn counts the stillness of each god's hand
	for (int turn = 0; turn < 3; ++turn)
	{
		_hands.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<const ecs::components::HandStillness>(made[0]).stillTurns,
	          registry.Get<const ecs::components::HandStillness>(_hand).stillTurns);
}

TEST_F(ReportedHands, ThisComputerMovesTheHandsOfItsOwnGodOnceTheyAreThere)
{
	_hands.SetOwnPlayer(PlayerNames::PLAYER_TWO);
	// Before the first turn it has the first player's hands to draw
	EXPECT_EQ(_hands.GetOwnHands(), _hands.GetPlayerHands());
	const auto made = _hands.MakeHandsOf(PlayerNames::PLAYER_TWO);
	EXPECT_EQ(_hands.GetOwnHands(), made);
	// The turns still read the first player's hands
	EXPECT_EQ(_hands.GetPlayerHands()[0], _hand);
}

TEST_F(ReportedHands, AfterALoadTheHandsOfEachGodAreFoundByTheirPlayer)
{
	const auto made = _hands.MakeHandsOf(PlayerNames::PLAYER_THREE);
	ecs::systems::HandSystem loaded;
	loaded.FindAfterLoad();
	EXPECT_EQ(loaded.GetPlayerHands()[0], _hand);
	EXPECT_EQ(loaded.GetHandsOf(PlayerNames::PLAYER_THREE), made);
	EXPECT_FALSE(loaded.GetHandsOf(PlayerNames::PLAYER_TWO).has_value());
}

TEST_F(ReportedHands, TheWorldHasNoCameraBeforeTheFirstReport)
{
	EXPECT_FALSE(_hands.GetReportedCamera().has_value());
}

TEST_F(ReportedHands, TheWorldHasTheCameraWhereTheLastReportPutIt)
{
	auto report = ReportAt({20.0f, 1.0f, 30.0f});
	report.focus = {25.0f, 0.0f, 35.0f};
	_hands.Report(PlayerNames::PLAYER_ONE, report);
	const auto camera = _hands.GetReportedCamera();
	ASSERT_TRUE(camera.has_value());
	EXPECT_EQ(camera->origin, glm::vec3(0.0f, 100.0f, 0.0f));
	EXPECT_EQ(camera->focus, glm::vec3(25.0f, 0.0f, 35.0f));
}

// The world holds the creature from the turn of the lock to the turn of its end, whatever the hand of the frames does
TEST_F(ReportedHands, TheWorldHoldsACreatureFromTheLockToItsEnd)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::handSystem::reset(&_hands, [](ecs::systems::HandSystemInterface* /*unowned*/) {});
	const auto creature = registry.Create();
	registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {.owner = PlayerNames::PLAYER_ONE});
	ecs::systems::CreatureHandSystem creatures;
	EXPECT_FALSE(creatures.GetLockedCreature().has_value());
	creatures.DoLock(PlayerNames::PLAYER_ONE, creature);
	EXPECT_EQ(creatures.GetLockedCreature(), creature);
	// The frames hold the hand to no creature
	EXPECT_FALSE(creatures.GetCreature().has_value());
	// The end of the lock of another thing leaves the hold
	creatures.DoUnlock(PlayerNames::PLAYER_ONE, registry.Create());
	EXPECT_EQ(creatures.GetLockedCreature(), creature);
	creatures.DoUnlock(PlayerNames::PLAYER_ONE, creature);
	EXPECT_FALSE(creatures.GetLockedCreature().has_value());
	Locator::handSystem::reset();
}

TEST_F(ReportedHands, TheWorldHoldsNoCreatureOfAnotherPlayer)
{
	auto& registry = Locator::entitiesRegistry::value();
	Locator::handSystem::reset(&_hands, [](ecs::systems::HandSystemInterface* /*unowned*/) {});
	const auto creature = registry.Create();
	registry.Assign<ecs::components::Creature>(creature, ecs::components::Creature {.owner = PlayerNames::PLAYER_TWO});
	// The refusal is written to the game's log
	const auto log = spdlog::get("game") != nullptr ? nullptr : spdlog::null_logger_mt("game");
	ecs::systems::CreatureHandSystem creatures;
	creatures.DoLock(PlayerNames::PLAYER_ONE, creature);
	EXPECT_FALSE(creatures.GetLockedCreature().has_value());
	Locator::handSystem::reset();
	if (log != nullptr)
	{
		spdlog::drop("game");
	}
}
