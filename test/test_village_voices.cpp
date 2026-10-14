/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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
