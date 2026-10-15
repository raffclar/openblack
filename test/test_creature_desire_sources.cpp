/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <deque>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureDesireSources.h"
#include "Creature/CreatureDesires.h"

using namespace openblack;
using namespace openblack::creature_desire_sources;

namespace
{
/// Random numbers given in order as shares of the most asked for, with the mosts kept
struct ScriptedRandom
{
	std::deque<float> shares;
	std::vector<float> mosts;

	[[nodiscard]] UpTo Function()
	{
		return [this](float most) {
			mosts.push_back(most);
			const auto share = shares.empty() ? 0.5f : shares.front();
			if (!shares.empty())
			{
				shares.pop_front();
			}
			return share * most;
		};
	}
};

State StateWith(const InnateTraits* innate, const Attention* attention)
{
	return {.innate = innate, .attention = attention, .turnsPerSecond = 10};
}
} // namespace

TEST(CreatureDesireSources, ATraitIsDrawnAgainForEachTestOfItsLimits)
{
	// The first draw is above 0, the second below 1, the third above 0: a fourth draw is kept, here below 0
	ScriptedRandom random {.shares = {1.0f, 0.5f, 1.0f, 0.0f}};
	EXPECT_NEAR(SpreadTrait(0.01f, k_TraitSpread, random.Function()), -0.09f, 1e-6f);
	EXPECT_EQ(random.mosts.size(), 4u);
	EXPECT_FLOAT_EQ(random.mosts.front(), 0.2f);
}

TEST(CreatureDesireSources, ATraitAtOneOrMoreOnTheSecondDrawIsOne)
{
	ScriptedRandom random {.shares = {0.5f, 1.0f}};
	EXPECT_FLOAT_EQ(SpreadTrait(0.95f, k_TraitSpread, random.Function()), 1.0f);
	EXPECT_EQ(random.mosts.size(), 2u);
}

TEST(CreatureDesireSources, ATraitAtNothingOrLessOnTheThirdDrawIsNothing)
{
	// The first draw is not above 0, so the upper limit is not tested: the next draw is the test of the lower limit
	ScriptedRandom random {.shares = {0.0f, 0.0f}};
	EXPECT_FLOAT_EQ(SpreadTrait(0.05f, k_TraitSpread, random.Function()), 0.0f);
	EXPECT_EQ(random.mosts.size(), 2u);
}

TEST(CreatureDesireSources, LookWeightsSpreadFurtherThanTraits)
{
	ScriptedRandom random;
	const InnateTraits species {.kindness = 0.2f,
	                            .aggression = 0.2f,
	                            .lethargy = 0.2f,
	                            .friendliness = 0.2f,
	                            .communicativeness = 0.2f,
	                            .sixth = 0.2f,
	                            .objectsLookWeight = 0.5f,
	                            .mountainsLookWeight = 0.5f,
	                            .waterLookWeight = 0.5f};
	const auto traits = SpreadTraits(species, random.Function());
	// A middle draw keeps each value; every trait between 0 and 1 is drawn four times
	EXPECT_FLOAT_EQ(traits.kindness, 0.2f);
	EXPECT_FLOAT_EQ(traits.objectsLookWeight, 0.5f);
	ASSERT_EQ(random.mosts.size(), 9u * 4u);
	EXPECT_FLOAT_EQ(random.mosts.at(0), 2.0f * k_TraitSpread);
	EXPECT_FLOAT_EQ(random.mosts.at(6 * 4), 2.0f * k_LookWeightSpread);
}

TEST(CreatureDesireSources, SourcesAndGrowingTimesAreSpread)
{
	ScriptedRandom up {.shares = {1.0f}};
	EXPECT_FLOAT_EQ(SpreadSource(0.5f, 0.1f, up.Function()), 0.6f);
	ScriptedRandom over {.shares = {1.0f}};
	EXPECT_FLOAT_EQ(SpreadSource(0.95f, 0.1f, over.Function()), 1.0f);
	ScriptedRandom down {.shares = {0.0f}};
	EXPECT_FLOAT_EQ(SpreadIncreaseSeconds(100.0f, down.Function()), 80.0f);
	ScriptedRandom most {.shares = {1.0f}};
	EXPECT_FLOAT_EQ(SpreadIncreaseSeconds(100.0f, most.Function()), 120.0f);
}

TEST(CreatureDesireSources, AttentionCountsTurnsWatchedIgnoredAndSinceTheLastInteraction)
{
	Attention attention;
	CountTurn(attention, false);
	CountTurn(attention, false);
	EXPECT_EQ(attention.turnsIgnored, 2u);
	EXPECT_EQ(attention.turnsWatched, 0u);
	CountTurn(attention, true);
	EXPECT_EQ(attention.turnsIgnored, 0u);
	EXPECT_EQ(attention.turnsWatched, 1u);
	// Being watched is not an interaction
	EXPECT_EQ(attention.turnsSinceInteraction, 3u);
}

TEST(CreatureDesireSources, LonelinessAndLackOfInteractionCountWholeSeconds)
{
	EXPECT_FLOAT_EQ(CountSource(599, k_LonelySeconds, 10), 59.0f / 60.0f);
	EXPECT_FLOAT_EQ(CountSource(600, k_LonelySeconds, 10), 1.0f);
	EXPECT_FLOAT_EQ(CountSource(5000, k_LonelySeconds, 10), 1.0f);
	EXPECT_FLOAT_EQ(CountSource(600, k_UninterestedSeconds, 10), 0.5f);
	EXPECT_EQ(TurnsForSource(1.0f, k_LonelySeconds, 10), 600u);
	EXPECT_EQ(TurnsForSource(0.5f, k_UninterestedSeconds, 10), 600u);

	Attention attention {.turnsIgnored = 300, .turnsSinceInteraction = 1200};
	const auto state = StateWith(nullptr, &attention);
	EXPECT_FLOAT_EQ(*Read(types::k_Loneliness, state), 0.5f);
	EXPECT_FLOAT_EQ(*Read(types::k_LackOfInteraction, state), 1.0f);
}

TEST(CreatureDesireSources, TheHandsMovementAndWhatItHoldsInterestTheCreature)
{
	PlayerInterest interest;
	// The first turn only notes where the hand is
	EXPECT_FLOAT_EQ(InterestNow(interest, glm::vec3 {0.0f}, std::nullopt), 0.0f);
	EXPECT_FLOAT_EQ(InterestNow(interest, glm::vec3 {30.0f, 0.0f, 0.0f}, std::nullopt), 30.0f * 0.98f / 60.0f);
	// A still hand's movement fades
	EXPECT_FLOAT_EQ(InterestNow(interest, glm::vec3 {30.0f, 0.0f, 0.0f}, std::nullopt), 30.0f * 0.98f * 0.98f / 60.0f);
	// The movement counts up to sixty
	EXPECT_FLOAT_EQ(InterestNow(interest, glm::vec3 {1000.0f, 0.0f, 0.0f}, std::nullopt), 0.98f);
	// What it holds counts up to a radius of thirty
	PlayerInterest held {.lastHand = glm::vec3 {0.0f}};
	EXPECT_FLOAT_EQ(InterestNow(held, glm::vec3 {0.0f}, 15.0f), 0.5f);
	EXPECT_FLOAT_EQ(InterestNow(held, glm::vec3 {0.0f}, 100.0f), 1.0f);
	// Without a player there is nothing of interest
	EXPECT_FLOAT_EQ(InterestNow(held, std::nullopt, 15.0f), 0.0f);
}

TEST(CreatureDesireSources, InterestRisesAtOnceAndHalvesTowardsLess)
{
	PlayerInterest interest;
	EXPECT_FLOAT_EQ(interest.value, 1.0f);
	UpdateInterest(interest, 0.0f);
	EXPECT_FLOAT_EQ(interest.value, 0.5f);
	UpdateInterest(interest, 0.0f);
	EXPECT_FLOAT_EQ(interest.value, 0.25f);
	UpdateInterest(interest, 0.8f);
	EXPECT_FLOAT_EQ(interest.value, 0.8f);
}

TEST(CreatureDesireSources, AYoungCreatureWantsToIdleWithItsPlayerMore)
{
	State state {.interest = 0.2f, .age = 5};
	EXPECT_FLOAT_EQ(*Read(types::k_IdleWithPlayer, state), 0.8f);
	state.age = 10;
	EXPECT_FLOAT_EQ(*Read(types::k_IdleWithPlayer, state), 0.4f);
	state.age = 40;
	EXPECT_FLOAT_EQ(*Read(types::k_IdleWithPlayer, state), 0.2f);
	state.interest = 1.0f;
	state.age = 0;
	EXPECT_FLOAT_EQ(*Read(types::k_IdleWithPlayer, state), 1.0f);
	// A computer player's creature never does
	state.computerPlayer = true;
	EXPECT_FLOAT_EQ(*Read(types::k_IdleWithPlayer, state), 0.0f);
}

TEST(CreatureDesireSources, HomeSourcesFollowTheDistanceFromHome)
{
	State state {.homeDistance = 50.0f, .goHomeDistance = 200.0f};
	EXPECT_FLOAT_EQ(*Read(types::k_GoHome, state), 0.25f);
	EXPECT_FLOAT_EQ(*Read(types::k_HangAroundAtHome, state), 0.5f);
	state.homeDistance = 500.0f;
	EXPECT_FLOAT_EQ(*Read(types::k_GoHome, state), 1.0f);
	EXPECT_FLOAT_EQ(*Read(types::k_HangAroundAtHome, state), 0.0f);
	// No distance for the stage: away from home it wants to go home fully, at home not at all
	state.goHomeDistance = 0.0f;
	EXPECT_FLOAT_EQ(*Read(types::k_GoHome, state), 1.0f);
	state.homeDistance = 0.0f;
	EXPECT_FLOAT_EQ(*Read(types::k_GoHome, state), 0.0f);
	// Without a home the sources keep their own values
	state.homeDistance.reset();
	EXPECT_FALSE(Read(types::k_GoHome, state).has_value());
	EXPECT_FALSE(Read(types::k_HangAroundAtHome, state).has_value());
}

TEST(CreatureDesireSources, GettingHighCountsWholeHundredsOfMagicMushrooms)
{
	State state {.mushroomsEaten = 0};
	EXPECT_FLOAT_EQ(*Read(types::k_GetHigh, state), 0.0f);
	state.mushroomsEaten = 99;
	EXPECT_FLOAT_EQ(*Read(types::k_GetHigh, state), 0.0f);
	EXPECT_FLOAT_EQ(*Read(types::k_MentalIllness, state), 0.0f);
	state.mushroomsEaten = 250;
	EXPECT_FLOAT_EQ(*Read(types::k_GetHigh, state), 2.0f);
	EXPECT_FLOAT_EQ(*Read(types::k_MentalIllness, state), 2.0f);
}

TEST(CreatureDesireSources, InnateSourcesReadThePersonality)
{
	const InnateTraits traits {
	    .kindness = 0.1f, .aggression = -0.05f, .lethargy = 0.3f, .friendliness = 0.4f, .communicativeness = 0.5f};
	const auto state = StateWith(&traits, nullptr);
	EXPECT_FLOAT_EQ(*Read(types::k_InnateKindness, state), 0.1f);
	// A trait outside 0 to 1 is read as it is
	EXPECT_FLOAT_EQ(*Read(types::k_InnateAggression, state), -0.05f);
	EXPECT_FLOAT_EQ(*Read(types::k_InnateLethargy, state), 0.3f);
	EXPECT_FLOAT_EQ(*Read(types::k_InnateFriendliness, state), 0.4f);
	EXPECT_FLOAT_EQ(*Read(types::k_InnateCommunicativeness, state), 0.5f);
	EXPECT_FALSE(Read(20, state).has_value());
}

TEST(CreatureDesireSources, ImpressFromSeeingVillagesGoesUpByHalves)
{
	EXPECT_FLOAT_EQ(ImpressAfterSighting(0.0f), 0.5f);
	EXPECT_FLOAT_EQ(ImpressAfterSighting(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(ImpressAfterSighting(1.0f), 1.0f);
	const State state {.impressFromSeeing = 0.5f};
	EXPECT_FLOAT_EQ(*Read(types::k_ImpressFromSeeingThings, state), 0.5f);
}

TEST(CreatureDesireSources, ASourceSetByAScriptChangesWhatItFollows)
{
	InnateTraits traits;
	Attention attention;
	PlayerInterest interest {.value = 0.7f};
	uint32_t mushrooms = 3;
	const Written written {
	    .innate = &traits, .attention = &attention, .interest = &interest, .mushroomsEaten = &mushrooms, .turnsPerSecond = 10};
	Write(types::k_InnateFriendliness, 1.0f, written);
	EXPECT_FLOAT_EQ(traits.friendliness, 1.0f);
	Write(types::k_Loneliness, 1.0f, written);
	EXPECT_EQ(attention.turnsIgnored, 600u);
	Write(types::k_LackOfInteraction, 1.0f, written);
	EXPECT_EQ(attention.turnsSinceInteraction, 1200u);
	// Interest and mushrooms are set to fixed values, whatever the value
	Write(types::k_IdleWithPlayer, 1.0f, written);
	EXPECT_FLOAT_EQ(interest.value, 0.0f);
	Write(types::k_GetHigh, 1.0f, written);
	EXPECT_EQ(mushrooms, 100u);
}

TEST(CreatureDesireSources, NewDesiresSpreadTheirSourcesAndGrowingTimes)
{
	std::array<creature_desires::DesireSetup, creature_desires::k_DesireCount> setup {};
	auto& hunger = setup.at(static_cast<size_t>(creature_desires::Desire::Hunger));
	hunger.increaseSeconds = 100.0f;
	hunger.sources.push_back({.type = 14, .value = 0.5f, .threshold = 0.5f});
	hunger.spreads.push_back({.value = 0.1f, .threshold = 0.2f});
	// The most of each range
	const auto most = [](float low, float high) { return high > low ? high : low; };
	const auto desires = creature_desires::Create(setup, most);
	const auto& made = desires[creature_desires::Desire::Hunger];
	EXPECT_FLOAT_EQ(made.increaseSeconds, 120.0f);
	ASSERT_EQ(made.sources.size(), 1u);
	EXPECT_FLOAT_EQ(made.sources.front().value, 0.6f);
	EXPECT_FLOAT_EQ(made.sources.front().threshold, 0.7f);
	// The middle of each range keeps the species' values
	const auto middle = [](float low, float high) { return 0.5f * (low + high); };
	const auto kept = creature_desires::Create(setup, middle);
	EXPECT_FLOAT_EQ(kept[creature_desires::Desire::Hunger].increaseSeconds, 100.0f);
	EXPECT_FLOAT_EQ(kept[creature_desires::Desire::Hunger].sources.front().value, 0.5f);
}

TEST(CreatureDesireSources, SettingASourceIsNotedForTheMind)
{
	creature_desires::Desires desires;
	desires[creature_desires::Desire::AttractAttention].sources.push_back({.type = types::k_Loneliness});
	creature_desires::MakeSourcesFull(desires, creature_desires::Desire::AttractAttention);
	ASSERT_EQ(desires.written.size(), 1u);
	EXPECT_EQ(desires.written.front().first, types::k_Loneliness);
	EXPECT_FLOAT_EQ(desires.written.front().second, 1.0f);
}

TEST(CreatureDesireSources, SeeingAThingAgainStirsSourcesOnlyAfter200Turns)
{
	EXPECT_FALSE(SightingStirsSources(200, 0));
	EXPECT_TRUE(SightingStirsSources(201, 0));
	EXPECT_FALSE(SightingStirsSources(500, 300));
	EXPECT_TRUE(SightingStirsSources(501, 300));
}

TEST(CreatureDesireSources, ASourceBelongsToTheFirstDesireThatGrowsFromIt)
{
	creature_desires::Desires desires;
	desires[creature_desires::Desire::Compassion].sources = {{.type = 3}};
	desires[creature_desires::Desire::Anger].sources = {{.type = 7}, {.type = 3}};
	EXPECT_EQ(creature_desires::DesireOfSource(desires, 3), creature_desires::Desire::Compassion);
	EXPECT_EQ(creature_desires::DesireOfSource(desires, 7), creature_desires::Desire::Anger);
	EXPECT_FALSE(creature_desires::DesireOfSource(desires, 60).has_value());
}
