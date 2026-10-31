/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <tuple>

#include <gtest/gtest.h>

#include "Magic/TownBelief.h"

using namespace openblack;
using namespace openblack::magic::town_belief;

namespace
{
constexpr size_t k_LandPlayers = openblack::player_slots::k_LandSlots;
} // namespace

TEST(TownBelief, PendingBeliefIsBelievedAtTheTownsTurnByItsScale)
{
	Belief town;
	town.scale = 2.0f;
	Add(town, PlayerNames::PLAYER_ONE, 0.25f);
	Add(town, PlayerNames::PLAYER_ONE, 0.25f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 0.0f);
	const auto gained = Turn(town, 0.997f);
	ASSERT_EQ(gained.size(), 1u);
	EXPECT_EQ(gained[0].player, PlayerNames::PLAYER_ONE);
	EXPECT_FLOAT_EQ(gained[0].amount, 1.0f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 1.0f);
	EXPECT_FLOAT_EQ(town.pending.at(0), 0.0f);
	EXPECT_FLOAT_EQ(town.recent.at(0), 0.5f * 0.997f);
	EXPECT_TRUE(Turn(town, 0.997f).empty());
}

// The belief that a town gains adds up until the tenth turn takes it to show it
TEST(TownBelief, TheGainSinceItWasShownAddsUpUntilItIsTaken)
{
	Belief town;
	town.scale = 2.0f;
	Add(town, PlayerNames::PLAYER_ONE, 0.25f);
	std::ignore = Turn(town, 0.997f);
	Add(town, PlayerNames::PLAYER_ONE, 0.125f);
	Add(town, PlayerNames::PLAYER_TWO, 0.5f);
	std::ignore = Turn(town, 0.997f);
	EXPECT_FLOAT_EQ(town.gainedSinceShown.at(0), 0.75f);
	EXPECT_FLOAT_EQ(town.gainedSinceShown.at(1), 1.0f);

	// A loss below nothing counts as no gain, and taking the gains clears every player's count
	town.gainedSinceShown.at(2) = -0.5f;
	const auto taken = TakeGainedSinceShown(town);
	EXPECT_FLOAT_EQ(taken.at(0), 0.75f);
	EXPECT_FLOAT_EQ(taken.at(1), 1.0f);
	EXPECT_FLOAT_EQ(taken.at(2), 0.0f);
	EXPECT_TRUE(std::ranges::all_of(town.gainedSinceShown, [](float gained) { return gained == 0.0f; }));
}

// Only a gain of more than a thousandth shows, in thousandths
TEST(TownBelief, AGainShowsInThousandthsOnlyAboveTheLeastGain)
{
	EXPECT_FALSE(GainShown(0.0f).has_value());
	EXPECT_FALSE(GainShown(0.001f).has_value());
	ASSERT_TRUE(GainShown(0.0011f).has_value());
	EXPECT_FLOAT_EQ(*GainShown(0.0011f), 1.1f);
	EXPECT_FLOAT_EQ(*GainShown(0.25f), 250.0f);
	EXPECT_EQ(k_GainShownEvery, 10u);
}

TEST(TownBelief, CappedAboveOnly)
{
	Belief town;
	Add(town, PlayerNames::PLAYER_TWO, 15.0f);
	(void)Turn(town, 1.0f);
	EXPECT_FLOAT_EQ(town.belief.at(1), k_DefaultCap);
	Add(town, PlayerNames::PLAYER_TWO, -20.0f);
	(void)Turn(town, 1.0f);
	EXPECT_FLOAT_EQ(town.belief.at(1), -10.0f);
}

TEST(TownBelief, NeutralTownIsTakenOnceAPlayerBelievesAsMuchAsItDoesInTheNeutralPlayer)
{
	Belief town;
	SetInPlayer(town, PlayerNames::NEUTRAL, 0.3f);
	EXPECT_FALSE(Ownership(town, PlayerNames::NEUTRAL, 2.0f, k_LandPlayers).has_value());
	EXPECT_FLOAT_EQ(town.belief.at(7), 0.3f);
	Add(town, PlayerNames::PLAYER_ONE, 0.29f);
	(void)Turn(town, 1.0f);
	EXPECT_FALSE(Ownership(town, PlayerNames::NEUTRAL, 2.0f, k_LandPlayers).has_value());
	Add(town, PlayerNames::PLAYER_ONE, 0.02f);
	(void)Turn(town, 1.0f);
	const auto taker = Ownership(town, PlayerNames::NEUTRAL, 2.0f, k_LandPlayers);
	ASSERT_TRUE(taker.has_value());
	EXPECT_EQ(*taker, PlayerNames::PLAYER_ONE);
	// The new owner's belief is multiplied by the claimed-town multiplier
	EXPECT_FLOAT_EQ(town.belief.at(0), 0.62f);
	EXPECT_FALSE(Ownership(town, PlayerNames::PLAYER_ONE, 2.0f, k_LandPlayers).has_value());
}

TEST(TownBelief, TiesGoToTheLaterPlayerTheNeutralPlayerLast)
{
	Belief town;
	SetInPlayer(town, PlayerNames::NEUTRAL, 0.5f);
	Set(town, PlayerNames::PLAYER_ONE, 0.5f);
	// Player one holds it, but the neutral player believed in as much comes later and takes it
	const auto taker = Ownership(town, PlayerNames::PLAYER_ONE, 1.0f, k_LandPlayers);
	ASSERT_TRUE(taker.has_value());
	EXPECT_EQ(*taker, PlayerNames::NEUTRAL);
	Set(town, PlayerNames::PLAYER_TWO, 0.5f);
	Set(town, PlayerNames::PLAYER_THREE, 0.5f);
	SetInPlayer(town, PlayerNames::NEUTRAL, 0.0f);
	EXPECT_EQ(Ownership(town, PlayerNames::PLAYER_ONE, 1.0f, k_LandPlayers), PlayerNames::PLAYER_THREE);
}

TEST(TownBelief, OnlyThePlayersOfTheGameTakeATie)
{
	Belief town;
	for (size_t p = 0; p < openblack::player_slots::k_LandGods; ++p)
	{
		Set(town, static_cast<PlayerNames>(p), -1.0f);
	}
	SetInPlayer(town, PlayerNames::NEUTRAL, -1.0f);
	// The places after the players of a land believe nothing, more than the players do, but a land has only 8 players:
	// the neutral player, the last of them, takes the tie
	EXPECT_EQ(Ownership(town, PlayerNames::PLAYER_ONE, 1.0f, k_LandPlayers), PlayerNames::NEUTRAL);
	SetInPlayer(town, PlayerNames::NEUTRAL, -1.0f);
	Set(town, PlayerNames::PLAYER_ONE, -1.0f);
	// A network game with a ninth god: that god comes after the neutral player and takes the town
	EXPECT_EQ(Ownership(town, PlayerNames::PLAYER_ONE, 1.0f, k_LandPlayers + 1), static_cast<PlayerNames>(8));
}

TEST(TownBelief, NeutralBeliefIsSetBackEveryTurn)
{
	Belief town;
	SetInPlayer(town, PlayerNames::NEUTRAL, 0.5f);
	Set(town, PlayerNames::NEUTRAL, 3.0f);
	(void)Ownership(town, PlayerNames::NEUTRAL, 1.0f, k_LandPlayers);
	EXPECT_FLOAT_EQ(town.belief.at(7), 0.5f);
}

TEST(TownBelief, ClaimedBeliefIsCapped)
{
	Belief town;
	Set(town, PlayerNames::PLAYER_TWO, 8.0f);
	EXPECT_EQ(Ownership(town, PlayerNames::NEUTRAL, 2.0f, k_LandPlayers), PlayerNames::PLAYER_TWO);
	EXPECT_FLOAT_EQ(town.belief.at(1), k_DefaultCap);
}

TEST(TownBelief, LosingATownLowersBeliefByTheMultiplierAndTheLandsScale)
{
	Belief town;
	Set(town, PlayerNames::PLAYER_ONE, 4.0f);
	LostTown(town, PlayerNames::PLAYER_ONE, 0.5f, 0.75f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 1.5f);
}

TEST(TownBelief, ScriptSetsBeliefAsAShareOfBeliefInTheOwner)
{
	Belief town;
	Set(town, PlayerNames::PLAYER_TWO, 2.0f);
	Set(town, PlayerNames::PLAYER_ONE, 1.0f);
	SetRelativeToOwner(town, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, 0.25f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 0.5f);
	SetRelativeToOwner(town, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE, 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 0.0f);
}

TEST(TownBelief, TakingATownHalvesTheMostBelievedAndGivesTheTakerMore)
{
	constexpr std::array k_Players {PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_TWO, PlayerNames::NEUTRAL};
	Belief town;
	Set(town, PlayerNames::NEUTRAL, 0.5f);
	Set(town, PlayerNames::PLAYER_TWO, 2.0f);
	TakeTown(town, PlayerNames::PLAYER_ONE, 1.0f, k_Players);
	EXPECT_FLOAT_EQ(town.belief.at(1), 1.0f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 3.0f);
	EXPECT_FLOAT_EQ(town.belief.at(7), 0.5f);
	TakeTown(town, PlayerNames::NEUTRAL, 1.0f, k_Players);
	EXPECT_FLOAT_EQ(town.belief.at(0), 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(1), 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(7), 0.0f);
}

namespace
{
std::array<DesireDecay, k_DesireCount> MadeUpDesireTable()
{
	std::array<DesireDecay, k_DesireCount> table {};
	table.fill({.toBelief = 0.0f, .levelAfterGain = 0.5f, .levelFall = 0.0f});
	table.at(0) = {.toBelief = 0.1f, .levelAfterGain = 0.5f, .levelFall = 0.25f};
	return table;
}
} // namespace

TEST(TownBelief, AnOwnedTownInWantLosesBeliefInItsOwnerFasterAsItsLevelFalls)
{
	Belief town;
	town.belief.at(0) = 1.0f;
	const auto table = MadeUpDesireTable();
	std::array<float, k_DesireCount> desire {};
	desire.at(0) = 0.75f;
	// The levels start where the table puts them: 0.25 over the level, at 0.1 a unit
	LoseBeliefToDesires(town, PlayerNames::PLAYER_ONE, desire, table, 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 1.0f - 0.025f);
	ASSERT_TRUE(town.desireLevel.has_value());
	EXPECT_FLOAT_EQ(town.desireLevel->at(0), 0.25f);
	// The level fell, so the same want now costs more
	LoseBeliefToDesires(town, PlayerNames::PLAYER_ONE, desire, table, 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 1.0f - 0.025f - 0.05f);
	EXPECT_FLOAT_EQ(town.desireLevel->at(0), 0.0f);
	// At the least level it falls no more
	LoseBeliefToDesires(town, PlayerNames::PLAYER_ONE, desire, table, 0.0f);
	EXPECT_FLOAT_EQ(town.desireLevel->at(0), 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(0), 1.0f - 0.025f - 0.05f - 0.075f);
}

TEST(TownBelief, GainingBeliefPutsTheDesireLevelsBack)
{
	Belief town;
	town.belief.at(0) = 1.0f;
	const auto table = MadeUpDesireTable();
	const std::array<float, k_DesireCount> desire {};
	LoseBeliefToDesires(town, PlayerNames::PLAYER_ONE, desire, table, 0.0f);
	LoseBeliefToDesires(town, PlayerNames::PLAYER_ONE, desire, table, 0.0f);
	EXPECT_FLOAT_EQ(town.desireLevel->at(0), 0.0f);
	// Wanting nothing more than its level costs nothing
	EXPECT_FLOAT_EQ(town.belief.at(0), 1.0f);
	ResetDesireLevels(town, table);
	EXPECT_FLOAT_EQ(town.desireLevel->at(0), 0.5f);
}

TEST(TownBelief, ANeutralTownLosesNothingToItsWantsAndKeepsItsLevels)
{
	Belief town;
	town.belief.at(7) = 0.3f;
	const auto table = MadeUpDesireTable();
	std::array<float, k_DesireCount> desire {};
	desire.fill(1.0f);
	LoseBeliefToDesires(town, PlayerNames::NEUTRAL, desire, table, 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(7), 0.3f);
	EXPECT_FALSE(town.desireLevel.has_value());
}

TEST(TownBelief, WantsCanTakeBeliefBelowNothing)
{
	Belief town;
	auto table = MadeUpDesireTable();
	table.at(0).toBelief = 2.0f;
	std::array<float, k_DesireCount> desire {};
	desire.at(0) = 1.0f;
	LoseBeliefToDesires(town, PlayerNames::PLAYER_TWO, desire, table, 0.0f);
	EXPECT_FLOAT_EQ(town.belief.at(1), -1.0f);
}

TEST(TownBelief, TheLossToADesireRoundsTheDifferenceAndTheProductToAFloat)
{
	Belief town;
	town.belief.at(0) = 2.0f;
	std::array<DesireDecay, k_DesireCount> table {};
	table.at(0) = {.toBelief = 0.3f, .levelAfterGain = 1.17f, .levelFall = 0.0f};
	std::array<float, k_DesireCount> desire {};
	desire.at(0) = 3.3f;
	LoseBeliefToDesires(town, PlayerNames::PLAYER_ONE, desire, table, 0.0f);
	// The difference rounded to a float before the product gives one bit less than a wider difference does
	EXPECT_EQ(std::bit_cast<uint32_t>(town.belief.at(0)), 0x3FAE353Fu);
}

TEST(TownBelief, LandScriptCapIsTheAskedCapOutsideTheThreeGodNetworkMap)
{
	EXPECT_EQ(CapFromLandScript(0.75f, false, "Land2", 3, PlayerNames::PLAYER_TWO), 0.75f);
	EXPECT_EQ(CapFromLandScript(1.0f, false, k_ThreeGodMapName, 3, PlayerNames::PLAYER_ONE), 1.0f);
	EXPECT_EQ(CapFromLandScript(1.0f, true, "mpm_4p_1", 3, PlayerNames::PLAYER_ONE), 1.0f);
}

TEST(TownBelief, ThreeGodNetworkMapRaisesEachGodsCapInTownThreeToTwo)
{
	EXPECT_EQ(CapFromLandScript(1.0f, true, k_ThreeGodMapName, 3, PlayerNames::PLAYER_ONE), 2.0f);
	EXPECT_EQ(CapFromLandScript(1.0f, true, k_ThreeGodMapName, 3, PlayerNames::PLAYER_THREE), 2.0f);
	// Other towns, the neutral player and a name in another case keep the asked cap
	EXPECT_EQ(CapFromLandScript(1.0f, true, k_ThreeGodMapName, 4, PlayerNames::PLAYER_ONE), 1.0f);
	EXPECT_EQ(CapFromLandScript(1.0f, true, k_ThreeGodMapName, 3, PlayerNames::NEUTRAL), 1.0f);
	EXPECT_EQ(CapFromLandScript(1.0f, true, "MPM_3P_1", 3, PlayerNames::PLAYER_ONE), 1.0f);
}
