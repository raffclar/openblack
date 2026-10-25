/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <map>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/GameEndRules.h"

using namespace openblack;
using namespace openblack::game_end;
using openblack::ecs::components::GameStanding;
using openblack::ecs::components::OutReason;
using openblack::ecs::components::WinCondition;

namespace
{
struct FakeGod
{
	bool human {false};
	bool computer {false};
	bool temple {true};
	bool creature {true};
	float influence {0.0f};
	uint32_t godsDefeated {0};
	GameStanding standing;
};

class FakeWorld final: public World
{
public:
	std::map<PlayerNames, FakeGod> gods;

	[[nodiscard]] std::vector<PlayerNames> ActiveGods() const override
	{
		std::vector<PlayerNames> active;
		for (const auto& [player, god] : gods)
		{
			if (god.human || god.computer)
			{
				active.push_back(player);
			}
		}
		return active;
	}
	[[nodiscard]] bool IsHuman(PlayerNames player) const override { return gods.at(player).human; }
	[[nodiscard]] bool HasTemple(PlayerNames player) const override { return gods.at(player).temple; }
	[[nodiscard]] GameStanding& Standing(PlayerNames player) override { return gods.at(player).standing; }
	[[nodiscard]] float InfluenceShare(PlayerNames player) const override { return gods.at(player).influence; }
	void RemoveCreature(PlayerNames player) override { gods.at(player).creature = false; }
	void CountGodDefeated(PlayerNames player) override { ++gods.at(player).godsDefeated; }
};

constexpr auto k_One = PlayerNames::PLAYER_ONE;
constexpr auto k_Two = PlayerNames::PLAYER_TWO;
constexpr auto k_Three = PlayerNames::PLAYER_THREE;
constexpr auto k_Five = PlayerNames::PLAYER_FIVE;

FakeWorld TwoHumans()
{
	FakeWorld world;
	world.gods[k_One] = {.human = true};
	world.gods[k_Two] = {.human = true};
	return world;
}

constexpr Facts k_Network {.networkGame = true, .skirmish = false, .turn = 10};
constexpr Facts k_Skirmish {.networkGame = false, .skirmish = true, .turn = 10};
} // namespace

TEST(GameEnd, AConditionStaysCompletedOnceReached)
{
	WinCondition condition {.current = 0, .target = 100, .completed = false};
	Set(condition, 150);
	EXPECT_TRUE(condition.completed);
	Set(condition, 20);
	EXPECT_TRUE(condition.completed);
	EXPECT_EQ(condition.current, 20);
	WinCondition added {.current = 0, .target = 3, .completed = false};
	Add(added, 2);
	EXPECT_FALSE(added.completed);
	Add(added, 1);
	EXPECT_TRUE(added.completed);
}

TEST(GameEnd, TheShareCountsACompletedConditionWhole)
{
	GameStanding standing;
	standing.conditions[0] = WinCondition {.current = 5, .target = 10, .completed = false};
	standing.conditions[3] = WinCondition {.current = 1, .target = 1, .completed = true};
	const auto done = ShareOfConditions(standing, ShareOfEachCondition(2));
	EXPECT_FALSE(done.all);
	EXPECT_FLOAT_EQ(done.share, 0.75f);
}

TEST(GameEnd, AGodWithNoConditionsCompletedAll)
{
	const auto done = ShareOfConditions(GameStanding {}, ShareOfEachCondition(0));
	EXPECT_TRUE(done.all);
	EXPECT_FLOAT_EQ(done.share, 1.0f);
}

TEST(GameEnd, NothingEndsAStoryGame)
{
	auto world = TwoHumans();
	world.gods[k_Two].temple = false;
	State state;
	const auto events = ProcessTurn(world, {.networkGame = false, .skirmish = false, .turn = 10}, state);
	EXPECT_TRUE(events.empty());
	EXPECT_FALSE(world.gods[k_Two].standing.out);
}

TEST(GameEnd, TheLastGodWithATempleDecidesTheGame)
{
	auto world = TwoHumans();
	world.gods[k_Two].temple = false;
	State state;
	const auto events = ProcessTurn(world, k_Network, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOutAndDecided);
	EXPECT_EQ(events[0].god, k_Two);
	EXPECT_TRUE(state.decided);
	EXPECT_TRUE(world.gods[k_Two].standing.out);
	EXPECT_FALSE(world.gods[k_Two].creature);
	EXPECT_EQ(world.gods[k_One].godsDefeated, 1u);
	// The god out takes the first place, and the god left takes the next
	EXPECT_EQ(world.gods[k_Two].standing.place, 1);
	EXPECT_EQ(world.gods[k_One].standing.place, 2);
	// A decided game does nothing more
	EXPECT_TRUE(ProcessTurn(world, k_Network, state).empty());
}

TEST(GameEnd, WithThreeGodsTheGameGoesOnAfterTheFirstIsOut)
{
	auto world = TwoHumans();
	world.gods[k_Three] = {.computer = true};
	world.gods[k_Three].temple = false;
	State state;
	const auto events = ProcessTurn(world, k_Skirmish, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOut);
	EXPECT_FALSE(state.decided);
	// A computer god has no place. In a skirmish the human gods count it defeated.
	EXPECT_FALSE(world.gods[k_Three].standing.place.has_value());
	EXPECT_EQ(world.gods[k_One].godsDefeated, 1u);
	EXPECT_EQ(world.gods[k_Two].godsDefeated, 1u);
	// The god is counted one time only
	EXPECT_TRUE(ProcessTurn(world, k_Skirmish, state).empty());
}

TEST(GameEnd, AComputerGodOutOfANetworkGameIsNotCountedDefeated)
{
	auto world = TwoHumans();
	world.gods[k_Three] = {.computer = true};
	world.gods[k_Three].temple = false;
	State state;
	static_cast<void>(ProcessTurn(world, k_Network, state));
	EXPECT_EQ(world.gods[k_One].godsDefeated, 0u);
}

TEST(GameEnd, PlayersFiveToSevenNeverLoseByTheTempleRule)
{
	auto world = TwoHumans();
	world.gods[k_Five] = {.human = true};
	world.gods[k_Five].temple = false;
	State state;
	EXPECT_TRUE(ProcessTurn(world, k_Network, state).empty());
	EXPECT_FALSE(world.gods[k_Five].standing.out);
	EXPECT_TRUE(world.gods[k_Five].creature);
}

TEST(GameEnd, AGodThatLeftIsCountedWithItsReason)
{
	auto world = TwoHumans();
	world.gods[k_Two].standing.out = true;
	world.gods[k_Two].standing.reason = OutReason::Left;
	State state;
	const auto events = ProcessTurn(world, k_Network, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOutAndDecided);
	EXPECT_EQ(world.gods[k_Two].standing.reason, OutReason::Left);
	// Its temple still stands, and its creature stays
	EXPECT_TRUE(world.gods[k_Two].creature);
}

TEST(GameEnd, AGodThatCompletesEveryConditionWins)
{
	auto world = TwoHumans();
	world.gods[k_One].standing.conditions[0] = WinCondition {.current = 3, .target = 3, .completed = true};
	world.gods[k_Two].standing.conditions[0] = WinCondition {.current = 1, .target = 3, .completed = false};
	State state;
	Facts facts = k_Network;
	facts.conditionCount = 1;
	const auto events = ProcessTurn(world, facts, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::Won);
	EXPECT_EQ(events[0].god, k_One);
	EXPECT_TRUE(state.decided);
	EXPECT_FALSE(world.gods[k_One].standing.out);
	EXPECT_TRUE(world.gods[k_Two].standing.out);
	EXPECT_EQ(world.gods[k_Two].standing.place, 1);
}

TEST(GameEnd, TheTimeLimitDecidesByTheShareAndThenTheInfluence)
{
	auto world = TwoHumans();
	world.gods[k_One].standing.conditions[0] = WinCondition {.current = 1, .target = 2, .completed = false};
	world.gods[k_Two].standing.conditions[0] = WinCondition {.current = 1, .target = 2, .completed = false};
	world.gods[k_One].influence = 0.25f;
	world.gods[k_Two].influence = 0.75f;
	State state;
	Facts facts = k_Network;
	facts.conditionCount = 1;
	facts.timeLimitTurn = 11;
	EXPECT_TRUE(ProcessTurn(world, facts, state).empty());
	facts.turn = 11;
	const auto events = ProcessTurn(world, facts, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].god, k_Two);
}

TEST(GameEnd, ASkirmishHasNoTimeLimit)
{
	auto world = TwoHumans();
	State state;
	Facts facts = k_Skirmish;
	facts.timeLimitTurn = 10;
	EXPECT_TRUE(ProcessTurn(world, facts, state).empty());
	EXPECT_FALSE(state.decided);
}

TEST(GameEnd, AGameDecidedAfterAGodWasOutRanksTheGodsStillIn)
{
	auto world = TwoHumans();
	world.gods[k_Three] = {.human = true};
	world.gods[k_Three].standing.out = true;
	world.gods[k_Three].standing.finished = true;
	world.gods[k_Three].standing.place = 1;
	world.gods[k_One].standing.conditions[0] = WinCondition {.current = 1, .target = 1, .completed = true};
	world.gods[k_Two].standing.conditions[0] = WinCondition {.current = 0, .target = 1, .completed = false};
	State state {.decided = false, .lastPlace = 1};
	Facts facts = k_Network;
	facts.conditionCount = 1;
	const auto events = ProcessTurn(world, facts, state);
	ASSERT_FALSE(events.empty());
	EXPECT_EQ(events[0].god, k_One);
	EXPECT_EQ(world.gods[k_Two].standing.place, 2);
	EXPECT_EQ(world.gods[k_One].standing.place, 3);
}
