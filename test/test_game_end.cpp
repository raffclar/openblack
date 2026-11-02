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
#include <span>
#include <tuple>
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
	bool mind {false};
	float influence {0.0f};
	float power {0.0f};
	uint32_t godsDefeated {0};
	GameStanding standing;
};

class FakeWorld final: public World
{
public:
	std::map<PlayerNames, FakeGod> gods;
	/// Each god whose creature file the game kept, while the creature was on the land
	std::vector<PlayerNames> kept;

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
	[[nodiscard]] float InfluencePower(PlayerNames player) const override { return gods.at(player).power; }
	void KeepCreatureFile(PlayerNames player) override
	{
		if (gods.at(player).creature)
		{
			kept.push_back(player);
		}
	}
	void RemoveCreature(PlayerNames player) override { gods.at(player).creature = false; }
	void CountGodDefeated(PlayerNames player) override { ++gods.at(player).godsDefeated; }
	void StopMind(PlayerNames player) override { gods.at(player).mind = false; }
};

constexpr auto k_One = PlayerNames::PLAYER_ONE;
constexpr auto k_Two = PlayerNames::PLAYER_TWO;
constexpr auto k_Three = PlayerNames::PLAYER_THREE;
constexpr auto k_Five = PlayerNames::PLAYER_FIVE;
// The gods after the neutral player have the numbers after it: the ninth god of a network game has the number 9
constexpr auto k_God9 = static_cast<PlayerNames>(9);
constexpr auto k_God32 = static_cast<PlayerNames>(32);

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

TEST(GameEnd, AToolRunOnTheTestbedNeedsNoTemple)
{
	auto world = TwoHumans();
	world.gods[k_One].temple = false;
	world.gods[k_Two].temple = false;
	State state;
	Facts facts = k_Network;
	facts.templesNeeded = false;
	EXPECT_TRUE(ProcessTurn(world, facts, state).empty());
	EXPECT_FALSE(state.decided);
	EXPECT_FALSE(world.gods[k_One].standing.out);
	EXPECT_FALSE(world.gods[k_Two].standing.out);
	EXPECT_TRUE(world.gods[k_One].creature);
}

TEST(GameEnd, AGodThatLeftIsOutEvenWhenNoTempleIsNeeded)
{
	auto world = TwoHumans();
	world.gods[k_Two].standing.out = true;
	world.gods[k_Two].standing.reason = OutReason::Left;
	State state;
	Facts facts = k_Network;
	facts.templesNeeded = false;
	const auto events = ProcessTurn(world, facts, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOutAndDecided);
	EXPECT_EQ(events[0].god, k_Two);
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
	world.gods[k_Three] = {.computer = true, .mind = true};
	world.gods[k_Three].temple = false;
	State state;
	const auto events = ProcessTurn(world, k_Skirmish, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOut);
	EXPECT_FALSE(state.decided);
	// A computer god has no place. In a skirmish the human gods count it defeated.
	EXPECT_FALSE(world.gods[k_Three].standing.place.has_value());
	// The god out stops thinking
	EXPECT_FALSE(world.gods[k_Three].mind);
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

TEST(GameEnd, InASkirmishPlayersFiveToSevenNeverLoseByTheTempleRule)
{
	auto world = TwoHumans();
	world.gods[k_Five] = {.computer = true, .mind = true};
	world.gods[k_Five].temple = false;
	State state;
	EXPECT_TRUE(ProcessTurn(world, k_Skirmish, state).empty());
	EXPECT_FALSE(world.gods[k_Five].standing.out);
	EXPECT_TRUE(world.gods[k_Five].creature);
	EXPECT_TRUE(world.gods[k_Five].mind);
}

TEST(GameEnd, OnlyTheFirstFourPlayersTakePartInASkirmish)
{
	EXPECT_TRUE(CanLose(k_One, false));
	EXPECT_TRUE(CanLose(PlayerNames::PLAYER_FOUR, false));
	EXPECT_FALSE(CanLose(k_Five, false));
	EXPECT_FALSE(CanLose(PlayerNames::PLAYER_SEVEN, false));
	EXPECT_TRUE(CanLose(k_Five, true));
	EXPECT_TRUE(CanLose(k_God32, true));
}

// In a network game, a god after the fourth with no temple is out, as each god before it
TEST(GameEnd, AGodAfterTheFourthOfANetworkGameIsOutWithNoTemple)
{
	auto world = TwoHumans();
	world.gods[k_Five] = {.human = true};
	world.gods[k_Five].temple = false;
	State state;
	const auto events = ProcessTurn(world, k_Network, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOut);
	EXPECT_EQ(events[0].god, k_Five);
	EXPECT_FALSE(state.decided);
	EXPECT_TRUE(world.gods[k_Five].standing.out);
	EXPECT_FALSE(world.gods[k_Five].creature);
	EXPECT_EQ(world.gods[k_Five].standing.place, 1);
	EXPECT_EQ(world.gods[k_One].godsDefeated, 1u);
	EXPECT_EQ(world.gods[k_Two].godsDefeated, 1u);
}

// A network game of many gods is decided when one god is left, also when that god is after the fourth
TEST(GameEnd, TheLastGodOfANetworkGameOfManyGodsDecidesTheGame)
{
	FakeWorld world;
	const std::array gods {k_One, k_Two, k_Three, PlayerNames::PLAYER_FOUR, k_Five, k_God9, k_God32};
	for (const auto god : gods)
	{
		world.gods[god] = {.human = true};
	}
	// Each god but the last loses its temple: first two gods after the fourth, then three gods, then one god
	world.gods[k_Five].temple = false;
	world.gods[k_God9].temple = false;
	State state;
	auto events = ProcessTurn(world, k_Network, state);
	ASSERT_EQ(events.size(), 2u);
	EXPECT_EQ(events[1].kind, Event::Kind::GodOut);
	EXPECT_FALSE(state.decided);
	for (const auto god : {k_One, k_Two, k_Three})
	{
		world.gods[god].temple = false;
	}
	events = ProcessTurn(world, k_Network, state);
	ASSERT_EQ(events.size(), 3u);
	EXPECT_EQ(events[2].kind, Event::Kind::GodOut);
	EXPECT_FALSE(state.decided);
	world.gods[PlayerNames::PLAYER_FOUR].temple = false;
	events = ProcessTurn(world, k_Network, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOutAndDecided);
	EXPECT_TRUE(state.decided);
	EXPECT_FALSE(world.gods[k_God32].standing.out);
	// The god that is left takes the place after the six gods out
	EXPECT_EQ(world.gods[k_God32].standing.place, 7);
	EXPECT_EQ(world.gods[k_God32].godsDefeated, 6u);
}

// In a network game, a god after the fourth gets its share of the conditions in its score history, and can win
TEST(GameEnd, AGodAfterTheFourthOfANetworkGameGetsItsShareOfTheConditions)
{
	auto world = TwoHumans();
	world.gods[k_God9] = {.human = true};
	world.gods[k_One].standing.conditions[0] = WinCondition {.current = 1, .target = 4, .completed = false};
	world.gods[k_Two].standing.conditions[0] = WinCondition {.current = 2, .target = 4, .completed = false};
	world.gods[k_God9].standing.conditions[0] = WinCondition {.current = 3, .target = 4, .completed = false};
	State state;
	Facts facts = k_Network;
	facts.turn = 30;
	facts.conditionCount = 1;
	EXPECT_TRUE(ProcessTurn(world, facts, state).empty());
	EXPECT_EQ(world.gods[k_God9].standing.share, 0.75f);
	EXPECT_EQ(world.gods[k_God9].standing.scoreHistoryIndex, 1);
	EXPECT_EQ(world.gods[k_God9].standing.scoreHistory[1], 0.75f);
	// The god completes its condition and wins
	world.gods[k_God9].standing.conditions[0] = WinCondition {.current = 4, .target = 4, .completed = true};
	facts.turn = 31;
	const auto events = ProcessTurn(world, facts, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::Won);
	EXPECT_EQ(events[0].god, k_God9);
	EXPECT_TRUE(world.gods[k_One].standing.out);
	EXPECT_TRUE(world.gods[k_Two].standing.out);
}

TEST(GameEnd, TheTempleRuleRunsAtTheFirstTurnWithNoWaitForATemple)
{
	// A god that never had a temple is out at the first turn of a network game
	auto world = TwoHumans();
	world.gods[k_One].temple = false;
	world.gods[k_Two].temple = false;
	State state;
	const auto events = ProcessTurn(world, {.networkGame = true, .skirmish = false, .turn = 1}, state);
	ASSERT_EQ(events.size(), 2u);
	EXPECT_TRUE(state.decided);
}

TEST(GameEnd, ATestRunWithNoTempleRuleKeepsGodsWithNoTempleIn)
{
	auto world = TwoHumans();
	world.gods[k_One].temple = false;
	world.gods[k_Two].temple = false;
	State state;
	const Facts noTempleRule {.networkGame = true, .skirmish = false, .turn = 1, .templesNeeded = false};
	EXPECT_TRUE(ProcessTurn(world, noTempleRule, state).empty());
	EXPECT_FALSE(state.decided);
	EXPECT_FALSE(world.gods[k_One].standing.out);
	EXPECT_TRUE(world.gods[k_Two].creature);
	// A god that left is still counted out
	world.gods[k_Two].standing.out = true;
	world.gods[k_Two].standing.reason = OutReason::Left;
	const auto events = ProcessTurn(world, noTempleRule, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::GodOutAndDecided);
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
	world.gods[k_Three] = {.computer = true, .mind = true};
	world.gods[k_Three].standing.conditions[0] = WinCondition {.current = 0, .target = 3, .completed = false};
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
	// The decided game stops every computer mind
	EXPECT_FALSE(world.gods[k_Three].mind);
	// The worst god is out first. The computer god takes no place, but as every god but the winner is marked out, its
	// count gives the winner a place at once. Then the human god takes the next place, and the winner the last.
	EXPECT_EQ(world.gods[k_Two].standing.place, 2);
	EXPECT_EQ(world.gods[k_One].standing.place, 3);
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

// At the time limit, the gods take their places by their share: the worst first, the winner last
TEST(GameEnd, TheTimeLimitRanksTheGodsByTheirShare)
{
	auto world = TwoHumans();
	world.gods[k_Three] = {.human = true};
	world.gods[k_One].standing.conditions[0] = WinCondition {.current = 1, .target = 5, .completed = false};
	world.gods[k_Two].standing.conditions[0] = WinCondition {.current = 3, .target = 5, .completed = false};
	world.gods[k_Three].standing.conditions[0] = WinCondition {.current = 2, .target = 5, .completed = false};
	State state;
	Facts facts = k_Network;
	facts.conditionCount = 1;
	facts.timeLimitTurn = facts.turn;
	const auto events = ProcessTurn(world, facts, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].kind, Event::Kind::Won);
	EXPECT_EQ(events[0].god, k_Two);
	EXPECT_TRUE(state.decided);
	EXPECT_FALSE(world.gods[k_Two].standing.out);
	EXPECT_TRUE(world.gods[k_One].standing.out);
	EXPECT_TRUE(world.gods[k_Three].standing.out);
	// The worst god takes the first place. Each count of a god out when one god is left also gives the winner the next
	// place, so the winner has the last place.
	EXPECT_EQ(world.gods[k_One].standing.place, 1);
	EXPECT_EQ(world.gods[k_Three].standing.place, 3);
	EXPECT_EQ(world.gods[k_Two].standing.place, 4);
	// The winner saw the two other gods defeated
	EXPECT_EQ(world.gods[k_Two].godsDefeated, 2u);
}

// Two gods with the same share and the same share of the influence: the first god in the order of the players is better
TEST(GameEnd, AFullTieGoesToTheFirstGod)
{
	auto world = TwoHumans();
	world.gods[k_One].influence = 0.5f;
	world.gods[k_Two].influence = 0.5f;
	State state;
	Facts facts = k_Network;
	facts.timeLimitTurn = facts.turn;
	const auto events = ProcessTurn(world, facts, state);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_EQ(events[0].god, k_One);
	// The larger share of the influence wins a tie of the share, also when it is the second god
	auto again = TwoHumans();
	again.gods[k_One].influence = 0.5f;
	again.gods[k_Two].influence = 0.50001f;
	State second;
	const auto won = ProcessTurn(again, facts, second);
	ASSERT_EQ(won.size(), 1u);
	EXPECT_EQ(won[0].god, k_Two);
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

// Each tenth turn of a game with no conditions, each god keeps its share of the influence of the gods
TEST(GameEnd, TheScoreHistoryKeepsTheShareOfTheInfluenceEachTenTurns)
{
	auto world = TwoHumans();
	world.gods[k_One].power = 300.0f;
	world.gods[k_Two].power = 100.0f;
	State state;
	std::ignore = ProcessTurn(world, {.networkGame = true, .skirmish = false, .turn = 11}, state);
	EXPECT_EQ(world.gods[k_One].standing.scoreHistoryIndex, 0);
	std::ignore = ProcessTurn(world, {.networkGame = true, .skirmish = false, .turn = 20}, state);
	const auto& one = world.gods[k_One].standing;
	EXPECT_EQ(one.scoreHistoryIndex, 1);
	EXPECT_EQ(one.scoreHistory[0], 0.0f);
	EXPECT_EQ(one.scoreHistory[1], 0.75f);
	EXPECT_EQ(world.gods[k_Two].standing.scoreHistory[1], 0.25f);
	// The value that came last is not shown
	EXPECT_EQ(ShownScores(one).size(), 1u);
	EXPECT_EQ(ShownScores(one)[0], 0.0f);
}

// With conditions, each god keeps its share of the conditions, and no share of the influence while each god has a temple
TEST(GameEnd, TheScoreHistoryKeepsTheShareOfTheConditions)
{
	auto world = TwoHumans();
	world.gods[k_One].power = 300.0f;
	world.gods[k_One].standing.conditions[0] = WinCondition {.current = 1, .target = 4, .completed = false};
	world.gods[k_Two].standing.conditions[0] = WinCondition {.current = 3, .target = 4, .completed = false};
	State state;
	std::ignore = ProcessTurn(world, {.networkGame = true, .skirmish = false, .turn = 30, .conditionCount = 1}, state);
	EXPECT_EQ(world.gods[k_One].standing.scoreHistoryIndex, 1);
	EXPECT_EQ(world.gods[k_One].standing.scoreHistory[1], 0.25f);
	EXPECT_EQ(world.gods[k_Two].standing.scoreHistory[1], 0.75f);
}

// A full history drops its oldest value for each new one
TEST(GameEnd, AFullScoreHistoryDropsItsOldestValue)
{
	GameStanding standing;
	for (int i = 1; i <= 499; ++i)
	{
		AddScore(standing, static_cast<float>(i));
	}
	EXPECT_EQ(standing.scoreHistoryIndex, 499);
	EXPECT_EQ(standing.scoreHistory[0], 0.0f);
	AddScore(standing, 500.0f);
	EXPECT_EQ(standing.scoreHistoryIndex, 499);
	EXPECT_EQ(standing.scoreHistory[0], 1.0f);
	EXPECT_EQ(standing.scoreHistory[499], 500.0f);
	EXPECT_EQ(ShownScores(standing).size(), 499u);
}

// A skirmish has no conditions, so each tenth turn each god keeps its share of the influence, the computer gods too
TEST(GameEnd, ASkirmishKeepsTheShareOfTheInfluenceEachTenTurns)
{
	FakeWorld world;
	world.gods[k_One] = {.human = true, .power = 100.0f};
	world.gods[k_Two] = {.computer = true, .mind = true, .power = 300.0f};
	State state;
	std::ignore = ProcessTurn(world, {.networkGame = false, .skirmish = true, .turn = 20}, state);
	EXPECT_EQ(world.gods[k_One].standing.scoreHistoryIndex, 1);
	EXPECT_EQ(world.gods[k_One].standing.scoreHistory[1], 0.25f);
	EXPECT_EQ(world.gods[k_Two].standing.scoreHistory[1], 0.75f);
	std::ignore = ProcessTurn(world, {.networkGame = false, .skirmish = true, .turn = 25}, state);
	EXPECT_EQ(world.gods[k_One].standing.scoreHistoryIndex, 1);
}

// The lobby hears one time for each event that decides the game: a win by the conditions, or the last god out
TEST(GameEnd, OnlyTheEventsThatDecideTheGameCount)
{
	const std::array events {
	    Event {.kind = Event::Kind::GodOut, .god = k_Three},
	    Event {.kind = Event::Kind::GodOutAndDecided, .god = k_Two},
	    Event {.kind = Event::Kind::Won, .god = k_One},
	};
	EXPECT_EQ(DecidingEvents(events), 2u);
	EXPECT_EQ(DecidingEvents(std::span(events).first(1)), 0u);
	EXPECT_EQ(DecidingEvents({}), 0u);
	// A skirmish that the last god out decides counts one time
	auto world = TwoHumans();
	world.gods[k_Two].temple = false;
	State state;
	EXPECT_EQ(DecidingEvents(ProcessTurn(world, k_Skirmish, state)), 1u);
	EXPECT_EQ(DecidingEvents(ProcessTurn(world, k_Skirmish, state)), 0u);
}

// A network game with no conditions ends at the default time limit, and the god with the most influence wins
TEST(GameEnd, ANetworkGameWithNoConditionsEndsAtItsTimeLimitByTheInfluence)
{
	auto world = TwoHumans();
	world.gods[k_One].influence = 0.75f;
	world.gods[k_Two].influence = 0.25f;
	State state;
	Facts facts = k_Network;
	facts.timeLimitTurn = 18000;
	facts.turn = 17999;
	EXPECT_TRUE(ProcessTurn(world, facts, state).empty());
	EXPECT_FALSE(state.decided);
	facts.turn = 18000;
	const auto events = ProcessTurn(world, facts, state);
	ASSERT_FALSE(events.empty());
	EXPECT_EQ(events[0].kind, Event::Kind::Won);
	EXPECT_EQ(events[0].god, k_One);
	EXPECT_TRUE(state.decided);
}

TEST(GameEnd, TheDecisionKeepsTheCreatureFileOfEachGodWithACreature)
{
	auto world = TwoHumans();
	world.gods[k_Two].creature = false;
	world.gods[k_One].standing.conditions[0] = WinCondition {.current = 3, .target = 3, .completed = true};
	world.gods[k_Two].standing.conditions[0] = WinCondition {.current = 1, .target = 3, .completed = false};
	State state;
	Facts facts = k_Network;
	facts.conditionCount = 1;
	static_cast<void>(ProcessTurn(world, facts, state));
	ASSERT_TRUE(state.decided);
	// The world keeps only the file of the god of this computer. A god with no creature gives no file.
	EXPECT_EQ(world.kept, std::vector<PlayerNames> {k_One});
}

TEST(GameEnd, AGodOutKeepsTheCreatureFileBeforeTheCreatureGoes)
{
	auto world = TwoHumans();
	world.gods[k_Three] = {.computer = true};
	world.gods[k_Three].temple = false;
	State state;
	static_cast<void>(ProcessTurn(world, k_Network, state));
	EXPECT_FALSE(world.gods[k_Three].creature);
	EXPECT_EQ(world.kept, std::vector<PlayerNames> {k_Three});
}

TEST(GameEnd, TheLastGodLeftKeepsNoCreatureFile)
{
	// The god left wins when the other god is out, but the game keeps only the file of the god that went out
	auto world = TwoHumans();
	world.gods[k_Two].temple = false;
	State state;
	static_cast<void>(ProcessTurn(world, k_Network, state));
	ASSERT_TRUE(state.decided);
	EXPECT_EQ(world.kept, std::vector<PlayerNames> {k_Two});
}
