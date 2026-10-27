/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;

TEST(ComputerMindMiracles, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionChargeUpSpell), MindDomain::Miracles);
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastWaterSpell), MindDomain::Miracles);
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastShieldToProtect), MindDomain::Miracles);
}

TEST(ComputerMindRules, ChargeShareCountsTheSiteAndTheIcon)
{
	const IconFacts icon {
	    .site = true, .store = 1.0f, .cost = 10.0f, .battery = 2.0f, .available = 5.0f, .used = 1.0f, .capacity = 3.0f};
	EXPECT_FLOAT_EQ(ChargeShare(icon), (4.0f + 2.0f + 1.001f) / 10.001f);
	// With no dancers and too little, nothing
	auto still = icon;
	still.capacity = 0.0f;
	EXPECT_EQ(ChargeShare(still), 0.0f);
	// Without a site only the store counts
	EXPECT_FLOAT_EQ(ChargeShare({.store = 1.0f, .cost = 10.0f}), 1.001f / 10.001f);
}

TEST(ComputerMindRules, TurnsToChargeTheRest)
{
	const IconFacts icon {.site = true, .store = 1.0f, .cost = 10.0f, .available = 3.0f, .capacity = 2.0f};
	EXPECT_FLOAT_EQ(TurnsToCharge(icon), 3.0f);
	auto enough = icon;
	enough.available = 9.0f;
	EXPECT_EQ(TurnsToCharge(enough), 0.0f);
	auto still = icon;
	still.capacity = 0.0f;
	EXPECT_EQ(TurnsToCharge(still), -1.0f);
	EXPECT_EQ(TurnsToCharge({.store = 1.0f, .cost = 10.0f}), -1.0f);
}

TEST(ComputerMindRules, WishesAndChoices)
{
	EXPECT_FLOAT_EQ(RequestShare(30), 0.3f);
	EXPECT_EQ(RequestShare(500), 1.0f);
	EXPECT_EQ(SpellValue(2.0f, 0.0f, 1.0f), 1.0f);
	EXPECT_EQ(SpellValue(0.5f, 0.0f, 0.0f), 0.0f);
	const std::vector<float> desires {0.2f, 0.5f, 0.5f};
	EXPECT_EQ(MostWanting(desires), 1u);
	EXPECT_FALSE(MostWanting(std::vector<float> {0.0f, -1.0f}).has_value());
	const std::vector<ChargeCandidate> icons {{.charging = true, .requests = 90},
	                                          {.store = 1.0f, .requests = 90},
	                                          {.requests = 20},
	                                          {.requests = 40},
	                                          {.requests = 40}};
	const auto choice = ChooseCharge(icons);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->index, 3u);
	EXPECT_FLOAT_EQ(choice->value, 0.4f);
}

TEST(ComputerMindMiracles, OwnsTheNeedToCheckTheTownsForDamage)
{
	EXPECT_EQ(DomainOf(NodeKind::NeedCheckTownForDamage), MindDomain::Miracles);
	EXPECT_EQ(DomainOf(NodeKind::ActionFindBurningAbode), MindDomain::Miracles);
	EXPECT_EQ(DomainOf(NodeKind::ActionFindUnprotectedAbode), MindDomain::Miracles);
}

TEST(ComputerMindRules, KindOfMiracleSuitsItsAim)
{
	EXPECT_EQ(KindScore(SpellKind::Aggressive, CreatureDesires::Anger, 0.3f, 0.7f), 1.0f);
	EXPECT_EQ(KindScore(SpellKind::Aggressive, CreatureDesires::Compassion, 0.3f, 0.7f), 0.0f);
	EXPECT_EQ(KindScore(SpellKind::Compassionate, CreatureDesires::Compassion, 0.3f, 0.7f), 1.0f);
	EXPECT_EQ(KindScore(SpellKind::Compassionate, CreatureDesires::Anger, 0.3f, 0.7f), 0.0f);
	// An impressive miracle suits its aim as much as its balance says
	EXPECT_EQ(KindScore(SpellKind::Impressive, CreatureDesires::Anger, 0.3f, 0.7f), 0.3f);
	EXPECT_EQ(KindScore(SpellKind::Attack, CreatureDesires::Compassion, 0.3f, 0.7f), 0.7f);
	EXPECT_EQ(KindWish(0.5f, 0.3f), 0.5f * 0.3f);
}

TEST(ComputerMindRules, AngryMiracleKeepsClearOfTheOwnCreature)
{
	// Two small creatures: twice their reaches is 30 metres, which is also the least clearance
	EXPECT_TRUE(TooNearOwnCreature(29.9f, 0.5f, 0.5f));
	EXPECT_FALSE(TooNearOwnCreature(30.0f, 0.5f, 0.5f));
	// Two creatures of size 1: twice 15 and 15 metres
	EXPECT_TRUE(TooNearOwnCreature(59.0f, 1.0f, 1.0f));
	EXPECT_FALSE(TooNearOwnCreature(60.0f, 1.0f, 1.0f));
	// Tiny creatures keep the least clearance
	EXPECT_TRUE(TooNearOwnCreature(29.0f, 0.1f, 0.1f));
}

TEST(ComputerMindRules, MostDamagedBuildingAndTheNeed)
{
	const std::vector<float> shares {1.0f, 0.5f, 0.5f, 0.95f};
	const auto choice = MostDamaged(shares);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->index, 1u);
	EXPECT_EQ(choice->share, 0.5f);
	EXPECT_FALSE(MostDamaged(std::vector<float> {1.0f, 1.2f}).has_value());
	EXPECT_FALSE(MostDamaged(std::vector<float> {}).has_value());
	EXPECT_EQ(DamageNeed(0.5f), 1.0f);
	EXPECT_EQ(DamageNeed(0.95f), 1.0f - 0.95f * 0.95f);
	// The limit is the float nearest to 0.9
	EXPECT_EQ(DamageNeed(0.9f), 1.0f - 0.9f * 0.9f);
	EXPECT_EQ(DamageNeed(std::nextafter(0.9f, 0.0f)), 1.0f);
}

TEST(ComputerMindRules, TownFearsItsMostRecentAttack)
{
	std::array<TownAttack, k_TownAttackPlayers> attacks {};
	for (auto& attack : attacks)
	{
		attack.turn = 0;
	}
	attacks[1] = {.turn = 700, .miracleHarm = 5.0f, .thingHarm = 0.0f, .otherThingHarm = 0.0f};
	attacks[4] = {.turn = 900, .miracleHarm = 2.0f, .thingHarm = 1.0f, .otherThingHarm = 0.5f};
	const auto threat = ThreatOf(attacks, 1000);
	ASSERT_TRUE(threat.has_value());
	const float forgotten = 100.0f * k_AttackMemoryShare;
	EXPECT_EQ(threat->fear, 1.0f - forgotten * forgotten * forgotten);
	EXPECT_EQ(threat->miracleHarm, 2.0f);
	EXPECT_EQ(threat->thingHarm, 1.5f);
	EXPECT_EQ(ShieldAgainst(*threat), MagicType::Shield);
	EXPECT_EQ(ShieldToCheck(*threat), MagicType::Shield);
	// When the two harms are the same, the two choices differ
	const TownThreat even {.fear = 1.0f, .miracleHarm = 1.0f, .thingHarm = 1.0f};
	EXPECT_EQ(ShieldAgainst(even), MagicType::PhysicalShield);
	EXPECT_EQ(ShieldToCheck(even), MagicType::Shield);
	const TownThreat things {.fear = 1.0f, .miracleHarm = 0.0f, .thingHarm = 1.0f};
	EXPECT_EQ(ShieldAgainst(things), MagicType::PhysicalShield);
	EXPECT_EQ(ShieldToCheck(things), MagicType::PhysicalShield);
}

TEST(ComputerMindRules, TownForgetsOldAttacksAndHarmlessOnes)
{
	std::array<TownAttack, k_TownAttackPlayers> attacks {};
	// After 600 turns the town forgets the attack
	attacks[0] = {.turn = 0, .miracleHarm = 5.0f};
	EXPECT_FALSE(ThreatOf(attacks, 700).has_value());
	// An attack with no harm is no threat
	attacks[0] = {.turn = 690, .miracleHarm = 0.0f};
	EXPECT_FALSE(ThreatOf(attacks, 700).has_value());
	// A turn in the future counts as very long ago
	attacks[0] = {.turn = 800, .miracleHarm = 5.0f};
	EXPECT_FALSE(ThreatOf(attacks, 700).has_value());
}

TEST(ComputerMindRules, ImpressPlaceCountsTheVillagersNearTheHand)
{
	EXPECT_EQ(ImpressPlaceValue(0, 10.0f), 0.0f);
	EXPECT_EQ(ImpressPlaceValue(4, 0.0f), 4.0f * openblack::gutils::GetDistanceModifier(0.0f, k_ImpressReach));
	EXPECT_LT(ImpressPlaceValue(4, 500.0f), ImpressPlaceValue(4, 100.0f));
	const std::vector<float> values {1.0f, 3.0f, 3.0f, 2.0f};
	EXPECT_EQ(BestImpressPlace(values), 1u);
	EXPECT_FALSE(BestImpressPlace(std::vector<float> {}).has_value());
}

TEST(ComputerMindRules, CarryDistanceAndSameCastPlace)
{
	EXPECT_EQ(CarryDistance(0.0f, 4.0f, 1.0f), 4.0f * 1.25f + 1.0f);
	EXPECT_EQ(CarryDistance(0.25f, 4.0f, 1.0f), 4.0f * 1.5f + 1.0f);
	const map_coords::MapCoords a {.x = 0x100000, .z = 0x100000, .altitude = 0.0f};
	EXPECT_TRUE(SameCastPlace(a, {.x = 0x100000 + 0x1FFF, .z = 0x100000 - 0x1FFF, .altitude = 5.0f}));
	EXPECT_FALSE(SameCastPlace(a, {.x = 0x100000 + 0x2000, .z = 0x100000, .altitude = 0.0f}));
	EXPECT_FALSE(SameCastPlace(a, {.x = 0x100000, .z = 0x100000 - 0x2000, .altitude = 0.0f}));
}
