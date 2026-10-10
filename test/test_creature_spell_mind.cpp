/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureSpellMind.h"

using namespace openblack;
using namespace openblack::creature_spell_mind;
using openblack::creature_desires::Desire;

namespace
{
creature_desires::Desires Some()
{
	creature_desires::Desires desires;
	for (size_t i = 0; i < creature_desires::k_DesireCount; ++i)
	{
		auto& state = desires.desires.at(i);
		state.activated = true;
		state.max = 1.0f;
		state.value = 0.2f + 0.01f * static_cast<float>(i);
	}
	return desires;
}
} // namespace

constexpr float k_TurnsPerSecond = 10.0f;
constexpr uint32_t k_HeldTurns = 200000;
constexpr float k_Floor = 0.05f;

TEST(CreatureSpellMind, ASpellsDesireIsWantedAboveAllAndMostOthersHeldDown)
{
	auto desires = Some();
	const auto cheat = SetCheatDominant(desires, Desire::Anger, {.all = true, .floor = k_Floor}, k_TurnsPerSecond);
	EXPECT_EQ(cheat.desire, Desire::Anger);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].value, desires[Desire::Anger].max);
	EXPECT_FLOAT_EQ(desires[Desire::Curiosity].value, k_Floor);
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, 0u);
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, k_HeldTurns);
	// The body's needs only because all are to be held down
	EXPECT_EQ(desires[Desire::Hunger].suppressedTurns, k_HeldTurns);
	// Never these
	for (const auto never : {Desire::IdleWithPlayer, Desire::RestoreHealth, Desire::BeFriends, Desire::ManifestState,
	                         Desire::Rest, Desire::PlayWithPlayer, Desire::HangAroundAtHome, Desire::LookAround})
	{
		EXPECT_EQ(desires[never].suppressedTurns, 0u);
	}
	EXPECT_FALSE(HeldDownByCheat(Desire::Water, false));
	EXPECT_TRUE(HeldDownByCheat(Desire::Water, true));
}

TEST(CreatureSpellMind, CompassionAlsoWantsToMakeFriendsAboveAll)
{
	auto desires = Some();
	(void)SetCheatDominant(desires, Desire::Compassion, {.all = true, .floor = k_Floor}, k_TurnsPerSecond);
	EXPECT_EQ(desires[Desire::BeFriends].suppressedTurns, 0u);
	EXPECT_TRUE(desires[Desire::BeFriends].activated);
	// Made dominant first, it is then put at the floor with the rest as compassion is made dominant
	EXPECT_FLOAT_EQ(desires[Desire::BeFriends].value, k_Floor);
	EXPECT_FLOAT_EQ(desires[Desire::Compassion].value, desires[Desire::Compassion].max);
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, k_HeldTurns);
}

TEST(CreatureSpellMind, TheCheatLastsItsTimeThenLetsEverythingGo)
{
	auto desires = Some();
	auto cheat = SetCheatDominant(desires, Desire::Anger, {.all = true, .floor = k_Floor}, k_TurnsPerSecond);
	desires[Desire::Anger].suppressedTurns = 5;
	EXPECT_TRUE(StepCheat(desires, cheat, k_TurnsPerSecond));
	// Its own desire is let go each turn
	EXPECT_EQ(desires[Desire::Anger].suppressedTurns, 0u);
	cheat.turns = static_cast<uint32_t>(k_CheatSeconds * k_TurnsPerSecond) + 10;
	EXPECT_FALSE(StepCheat(desires, cheat, k_TurnsPerSecond));
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, 0u);
}

TEST(CreatureSpellMind, WearingOffItIsWantedLeast)
{
	auto desires = Some();
	(void)SetCheatDominant(desires, Desire::Scratch, {.all = true, .floor = k_Floor}, k_TurnsPerSecond);
	MakeLeastDominant(desires, Desire::Scratch);
	// The others are all at the floor
	EXPECT_NEAR(desires[Desire::Scratch].value, k_Floor / k_LeastDominantFactor, 1e-5f);
	ClearCheatDominance(desires);
	for (const auto& state : desires.desires)
	{
		EXPECT_EQ(state.suppressedTurns, 0u);
	}
}

TEST(CreatureSpellMind, ItsSourcesAreFilledButAngerFromHurtAndHealthFromLife)
{
	auto desires = Some();
	using namespace creature_desires::sources;
	desires[Desire::Anger].sources = {{.type = k_AngerFromDamage, .value = 0.1f},
	                                  {.type = k_InnateAggression, .value = 0.1f},
	                                  {.type = k_AngerFromSadness, .value = 0.1f}};
	desires[Desire::RestoreHealth].sources = {{.type = k_RestoreHealthFromLife, .value = 0.3f}};
	// A source of the same kind that drives another desire is the same source
	desires[Desire::Play].sources = {{.type = k_AngerFromSadness, .value = 0.1f}};
	(void)SetCheatDominant(desires, Desire::Anger, {.all = true, .floor = k_Floor}, k_TurnsPerSecond);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].sources[0].value, 0.1f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].sources[1].value, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Anger].sources[2].value, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::Play].sources[0].value, 1.0f);
	EXPECT_FLOAT_EQ(desires[Desire::RestoreHealth].sources[0].value, 0.3f);
}

TEST(CreatureSpellMind, AScriptsOnlyDesireLastsTheSecondsItGives)
{
	auto desires = Some();
	auto cheat = SetCheatDominant(desires, Desire::Hunger, {.all = true, .seconds = 3.0f, .floor = k_Floor}, k_TurnsPerSecond);
	EXPECT_EQ(cheat.seconds, 3u);
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, 30u);
	// Ending once more whole seconds have passed than it lasts
	for (int turn = 0; turn < 39; ++turn)
	{
		EXPECT_TRUE(StepCheat(desires, cheat, k_TurnsPerSecond));
	}
	EXPECT_FALSE(StepCheat(desires, cheat, k_TurnsPerSecond));
}

TEST(CreatureSpellMind, TakingTheOnlyDesireAwayWantsItNoMore)
{
	auto desires = Some();
	std::optional<Cheat> cheat = SetCheatDominant(desires, Desire::Sadness, {.all = true, .floor = k_Floor}, k_TurnsPerSecond);
	ClearOnlyDesire(desires, cheat, false);
	EXPECT_FALSE(cheat.has_value());
	EXPECT_FLOAT_EQ(desires[Desire::Sadness].value, 0.0f);
	// Not let go, the others stay held down
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, k_HeldTurns);

	cheat = SetCheatDominant(desires, Desire::Sadness, {.all = true, .floor = k_Floor}, k_TurnsPerSecond);
	ClearOnlyDesire(desires, cheat, true);
	EXPECT_EQ(desires[Desire::Curiosity].suppressedTurns, 0u);

	// With none, nothing changes
	desires[Desire::Sadness].value = 0.5f;
	ClearOnlyDesire(desires, cheat, true);
	EXPECT_FLOAT_EQ(desires[Desire::Sadness].value, 0.5f);
}
