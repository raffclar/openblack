/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Help/VillageVoices.h"
#include "Magic/SpellRules.h"

using namespace openblack;
using namespace openblack::guidance;

namespace
{
magic::EffectValues Values(float burn, float crush, float hit, float fly)
{
	magic::EffectValues values;
	values[magic::EffectKind::Burn] = burn;
	values[magic::EffectKind::Crush] = crush;
	values[magic::EffectKind::Hit] = hit;
	values[magic::EffectKind::FlyAway] = fly;
	return values;
}
} // namespace

TEST(VillageVoices, NoHarmGivesNoCry)
{
	EXPECT_FALSE(FireLeads(Values(0.0f, 0.0f, 0.0f, 0.0f)).has_value());
	// Healing, alignment and belief do not count
	auto values = Values(0.0f, 0.0f, 0.0f, 0.0f);
	values[magic::EffectKind::Heal] = 5.0f;
	values[magic::EffectKind::Belief] = 5.0f;
	EXPECT_FALSE(FireLeads(values).has_value());
}

TEST(VillageVoices, TheBurnCountsAThousandthAndAtMostOne)
{
	// 500 of burn counts 0.5, less than a hit of 0.6
	EXPECT_EQ(FireLeads(Values(500.0f, 0.0f, 0.6f, 0.0f)), std::optional(false));
	// 10000 of burn counts only 1, less than a crush of 2
	EXPECT_EQ(FireLeads(Values(10000.0f, 2.0f, 0.0f, 0.0f)), std::optional(false));
	EXPECT_EQ(FireLeads(Values(10000.0f, 0.9f, 0.0f, 0.0f)), std::optional(true));
	// An equal value later in the order does not win
	EXPECT_EQ(FireLeads(Values(1000.0f, 1.0f, 1.0f, 1.0f)), std::optional(true));
}

TEST(VillageVoices, TheCriesOfTheList)
{
	const auto plain = AttackCries(false, 0);
	ASSERT_EQ(plain.size(), 10u);
	EXPECT_EQ(plain.front(), 130u);
	EXPECT_EQ(plain.back(), 139u);
	const auto fire = AttackCries(true, 220);
	ASSERT_EQ(fire.size(), 30u);
	EXPECT_EQ(fire.at(10), 177u);
	EXPECT_EQ(fire.at(19), 186u);
	EXPECT_EQ(fire.at(20), 220u);
	EXPECT_EQ(fire.back(), 220u);
}

TEST(VillageVoices, ADrawCountsFromTheLastCry)
{
	const auto cries = AttackCries(false, 0);
	EXPECT_EQ(PickCry(cries, 0), 139u);
	EXPECT_EQ(PickCry(cries, 9), 130u);
	EXPECT_EQ(PickCry(cries, 10), 0u);
	// The attacker's cry fills the first ten draws
	EXPECT_EQ(PickCry(AttackCries(false, 195), 9), 195u);
	EXPECT_EQ(PickCry(AttackCries(false, 195), 10), 139u);
}

TEST(VillageVoices, TheAttackersCries)
{
	EXPECT_EQ(AttackerCry(Attacker::Rock, 0), 218u);
	EXPECT_EQ(AttackerCry(Attacker::Rock, 9), 227u);
	EXPECT_EQ(AttackerCry(Attacker::Lightning, 0), 190u);
	EXPECT_EQ(AttackerCry(Attacker::Creature, 7), 207u);
	// The ninth creature cry has no sample in the game
	EXPECT_EQ(AttackerCry(Attacker::Creature, 8), 0u);
	EXPECT_EQ(AttackerCry(Attacker::Creature, 9), 208u);
	EXPECT_EQ(AttackerCry(Attacker::Other, 3), 0u);
}

TEST(VillageVoices, TheCryCarriesFurtherForAProtectedTown)
{
	EXPECT_FLOAT_EQ(AttackCryReach(0.0f), 200.0f);
	EXPECT_FLOAT_EQ(AttackCryReach(2.5f), 300.0f);
	EXPECT_FLOAT_EQ(AttackCryReach(50.0f), 400.0f);
}

TEST(VillageVoices, ATownAnswersAGiftByItsDesire)
{
	EXPECT_EQ(GiftAnswer(Gift::Food, 0.5f, 0), std::optional(209u));
	EXPECT_EQ(GiftAnswer(Gift::Food, 0.1f, 2), std::optional(167u));
	EXPECT_FALSE(GiftAnswer(Gift::Food, 0.3f, 0).has_value());
	// Wood and rain answer pleased for a small desire too
	EXPECT_EQ(GiftAnswer(Gift::Wood, 0.1f, 1), std::optional(216u));
	EXPECT_EQ(GiftAnswer(Gift::Rain, 0.9f, 0), std::optional(212u));
	EXPECT_FALSE(GiftAnswer(Gift::Wood, 0.25f, 0).has_value());
}

TEST(VillageVoices, ATownAnswersOnlyAMuchOrALittleWantedGift)
{
	EXPECT_TRUE(GiftAnswered(0.5f));
	EXPECT_TRUE(GiftAnswered(0.249f));
	EXPECT_FALSE(GiftAnswered(0.25f));
	EXPECT_FALSE(GiftAnswered(0.499f));
	// The game draws one of three answers
	EXPECT_EQ(k_GiftAnswers, 3u);
	EXPECT_FALSE(GiftAnswer(Gift::Food, 0.9f, k_GiftAnswers).has_value());
}

TEST(VillageVoices, ATownDesirePicksItsVoiceByItsValue)
{
	// Food: the first voice from 0.85, the second from 0.65, the third from 0.45
	EXPECT_EQ(DesireVoice(0, 0.9f, 0.0f), 144u);
	EXPECT_EQ(DesireVoice(0, 0.9f, 0.1f), 145u);
	EXPECT_EQ(DesireVoice(0, 0.5f, 0.0f), 146u);
	EXPECT_EQ(DesireVoice(0, 0.4f, 0.0f), 0u);
	// The build row has the same voice three times, and a desire without voices gives none
	EXPECT_EQ(DesireVoice(5, 0.5f, 0.0f), 140u);
	EXPECT_EQ(DesireVoice(2, 1.0f, 0.0f), 0u);
	EXPECT_EQ(DesireVoice(k_WorshippersRow, 1.0f, 0.0f), 19u);
	EXPECT_EQ(DesireVoice(k_WorshipFoodRow, 1.0f, 0.0f), 23u);
	EXPECT_EQ(DesireVoice(19, 1.0f, 0.0f), 0u);
	EXPECT_FLOAT_EQ(DesireVoiceDrawLimit(0.9f), 0.3f);
}

TEST(VillageVoices, TheTimeSinceTheFirstLookAtAThing)
{
	std::vector<ecs::components::GuidanceHeard::SeenThing> seen;
	const auto town = static_cast<entt::entity>(4);
	const auto site = static_cast<entt::entity>(9);
	EXPECT_EQ(TurnsSinceSeen(seen, town, 100), 0u);
	EXPECT_EQ(TurnsSinceSeen(seen, town, 160), 60u);
	// A new thing goes to the front of the list
	EXPECT_EQ(TurnsSinceSeen(seen, site, 170), 0u);
	ASSERT_EQ(seen.size(), 2u);
	EXPECT_EQ(seen.front().thing, site);
	EXPECT_EQ(TurnsSinceSeen(seen, town, 700), 600u);
	// After more than 600 turns the time starts again
	EXPECT_EQ(TurnsSinceSeen(seen, town, 701), 0u);
}

TEST(VillageVoices, AWorshipSiteVoiceReplacesTheTownVoice)
{
	// The worshippers voice wins when it has a score, even a smaller one
	const auto both = WorshipSiteVoice({.voice = 23, .score = 0.8f}, {.voice = 19, .score = 0.4f});
	ASSERT_TRUE(both.has_value());
	EXPECT_EQ(both->voice, 19u);
	const auto food = WorshipSiteVoice({.voice = 24, .score = 0.8f}, {.voice = 0, .score = 0.0f});
	ASSERT_TRUE(food.has_value());
	EXPECT_EQ(food->voice, 24u);
	EXPECT_FALSE(WorshipSiteVoice({.voice = 0, .score = 0.0f}, {.voice = 0, .score = 0.0f}).has_value());
}

TEST(VillageVoices, TheReachOfADesireVoice)
{
	EXPECT_FLOAT_EQ(DesireVoiceReachDrawLimit(1.2f), 0.6f);
	EXPECT_FLOAT_EQ(DesireVoiceReach(1.0f, 0.25f), 150.0f);
	// A score near the most and no draw carry the voice 400 metres
	EXPECT_FLOAT_EQ(DesireVoiceReach(2.0f, 0.0f), 400.0f);
}

TEST(VillageVoices, TheScoreOfADesireVoice)
{
	// The full score: a strong desire at the camera, a long time since the last voice, a thing new to the voices
	EXPECT_FLOAT_EQ(DesireVoiceScore({.turnsSinceHeard = 50, .turnsSinceSeen = 0, .distance = 0.0f, .desire = 1.0f}), 2.0f);
	// A thing looked at for 300 turns or more, or a thing at 200 metres, gives no score
	EXPECT_FLOAT_EQ(DesireVoiceScore({.turnsSinceHeard = 50, .turnsSinceSeen = 300, .distance = 0.0f, .desire = 1.0f}), 0.0f);
	EXPECT_FLOAT_EQ(DesireVoiceScore({.turnsSinceHeard = 50, .turnsSinceSeen = 0, .distance = 200.0f, .desire = 1.0f}), 0.0f);
	// Half the time since the last voice halves the score, and the same voice again takes an eighth more of it
	EXPECT_FLOAT_EQ(DesireVoiceScore({.turnsSinceHeard = 25, .turnsSinceSeen = 0, .distance = 0.0f, .desire = 1.0f}), 1.0f);
	EXPECT_FLOAT_EQ(
	    DesireVoiceScore({.turnsSinceHeard = 25, .turnsSinceSeen = 0, .distance = 0.0f, .desire = 1.0f, .sameAsLast = true}),
	    0.125f);
}

TEST(VillageVoices, AVoiceOfAweNeedsAnotherPlayerToLead)
{
	// The player leads, or the beliefs are equal: no voice
	EXPECT_FALSE(BeliefVoiceShare(std::array {0.5f, 0.2f, 0.0f}, 0).has_value());
	EXPECT_FALSE(BeliefVoiceShare(std::array {0.5f, 0.5f, 0.0f}, 1).has_value());
	// Another player leads: the share against the most belief, with the small amount added to the two
	const auto share = BeliefVoiceShare(std::array {0.0f, 0.2f, 0.8f}, 1);
	ASSERT_TRUE(share.has_value());
	EXPECT_EQ(*share, (0.2f + 0.0001f) / (0.8f + 0.0001f));
	// A town with no belief in the player still gives a small share
	EXPECT_GT(*BeliefVoiceShare(std::array {0.0f, 0.0f, 1.0f}, 0), 0.0f);
}

TEST(VillageVoices, TheDrawOffTheShareIsAThird)
{
	// The game multiplies by the float nearest a third; it does not divide by three
	EXPECT_EQ(BeliefVoiceDrawLimit(0.7f), 0.7f * 0.333333343f);
	EXPECT_EQ(BeliefVoiceDrawLimit(0.0f), 0.0f);
}

TEST(VillageVoices, TheLeftSharePicksTheVoiceOfAwe)
{
	EXPECT_EQ(BeliefVoice(GuidanceAlignment::Good, 0.7f), 187u);
	EXPECT_EQ(BeliefVoice(GuidanceAlignment::Good, 0.69f), 188u);
	EXPECT_EQ(BeliefVoice(GuidanceAlignment::Good, 0.4f), 188u);
	EXPECT_EQ(BeliefVoice(GuidanceAlignment::Good, 0.39f), 189u);
	EXPECT_EQ(BeliefVoice(GuidanceAlignment::Evil, 1.0f), 174u);
	EXPECT_EQ(BeliefVoice(GuidanceAlignment::Evil, 0.5f), 175u);
	EXPECT_EQ(BeliefVoice(GuidanceAlignment::Evil, 0.06f), 176u);
	// The third voice needs more than 0.05
	EXPECT_FALSE(BeliefVoice(GuidanceAlignment::Good, 0.05f).has_value());
	EXPECT_FALSE(BeliefVoice(GuidanceAlignment::Evil, 0.0f).has_value());
	// Only good or evil has voices
	EXPECT_FALSE(BeliefVoice(GuidanceAlignment::None, 1.0f).has_value());
}

TEST(VillageVoices, TheWantOfAVoiceOfAwe)
{
	// At the point under the cursor, 50 turns or more since the last town desire voice: wanted in full
	EXPECT_EQ(BeliefVoiceWanted(50, 0.0f, false), 1.0f);
	EXPECT_EQ(BeliefVoiceWanted(5000, 0.0f, false), 1.0f);
	// At 200 metres or more, or just after a town desire voice: not wanted
	EXPECT_EQ(BeliefVoiceWanted(50, 200.0f, false), 0.0f);
	EXPECT_EQ(BeliefVoiceWanted(50, 300.0f, false), 0.0f);
	EXPECT_EQ(BeliefVoiceWanted(0, 0.0f, false), 0.0f);
	// Half the distance takes a quarter off; half the time halves it
	EXPECT_EQ(BeliefVoiceWanted(50, 100.0f, false), 0.75f);
	EXPECT_EQ(BeliefVoiceWanted(25, 100.0f, false), 0.375f);
	// The same voice again needs the time to the fourth power
	EXPECT_EQ(BeliefVoiceWanted(25, 0.0f, true), 0.0625f);
	// The least want that plays a voice
	EXPECT_FALSE(BeliefVoiceWanted(15, 0.0f, false) > k_BeliefVoiceLeast);
	EXPECT_TRUE(BeliefVoiceWanted(16, 0.0f, false) > k_BeliefVoiceLeast);
}
