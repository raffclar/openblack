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
#include <bit>
#include <optional>
#include <span>

#include <gtest/gtest.h>

#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindDefenceRules.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/TownAggression.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;

namespace
{
constexpr auto k_Players = static_cast<size_t>(PlayerNames::_COUNT);
} // namespace

TEST(ComputerMindDefence, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionDefendTown), MindDomain::Defence);
	EXPECT_EQ(DomainOf(NodeKind::ActionReactToPlayerThrowingStuff), MindDomain::Defence);
	EXPECT_EQ(DomainOf(NodeKind::ObjectCastSpellToDefendAgainstBelief), MindDomain::Defence);
	EXPECT_EQ(DomainOf(NodeKind::NeedCheckTownForProtection), MindDomain::Defence);
	EXPECT_EQ(DomainOf(NodeKind::NeedCheckTownsForBeliefAttack), MindDomain::Defence);
}

TEST(ComputerMindDefenceRules, TempleHitIsIgnoredWhenNewThenForgotten)
{
	EXPECT_FALSE(HitReaction(1000, 0).has_value());
	EXPECT_FALSE(HitReaction(1000, 1000).has_value());
	EXPECT_FALSE(HitReaction(1020, 1000).has_value());
	ASSERT_TRUE(HitReaction(1021, 1000).has_value());
	EXPECT_FLOAT_EQ(*HitReaction(1021, 1000), 1.0f - 21.0f * k_HitMemoryShare);
	EXPECT_FLOAT_EQ(*HitReaction(1300, 1000), 0.5f);
	EXPECT_EQ(*HitReaction(1600, 1000), 0.0f);
	EXPECT_EQ(*HitReaction(9000, 1000), 0.0f);
	// A hit after the turn counts as a hit long ago
	EXPECT_EQ(*HitReaction(999, 1000), 0.0f);
}

TEST(ComputerMindDefenceRules, DefendWishFromAggressionAndTime)
{
	// Full aggression just after the attack
	EXPECT_EQ(DefendWish(500, 500, 0.5f), 1.0f);
	EXPECT_EQ(DefendWish(500, 500, 3.0f), 1.0f);
	EXPECT_EQ(DefendWish(500, 500, 0.25f), 0.5f);
	// Half of the memory: the cube of a half
	EXPECT_EQ(DefendWish(1100, 500, 1.0f), 0.125f);
	EXPECT_EQ(DefendWish(1700, 500, 1.0f), 0.0f);
	EXPECT_EQ(DefendWish(5000, 500, 1.0f), 0.0f);
}

TEST(ComputerMindDefenceRules, ChooseDefenceTakesTheFirstThenOnlyLarger)
{
	const std::array<TownAttackFacts, 4> attacks {{
	    {.town = 0, .attacker = PlayerNames::PLAYER_ONE, .lastTurn = 0, .aggression = 0.0f},
	    {.town = 0, .attacker = PlayerNames::PLAYER_TWO, .lastTurn = 0, .aggression = 1.0f},
	    {.town = 1, .attacker = PlayerNames::PLAYER_TWO, .lastTurn = 900, .aggression = 1.0f},
	    {.town = 2, .attacker = PlayerNames::PLAYER_TWO, .lastTurn = 900, .aggression = 1.0f},
	}};
	// Long ago, the first attack that counts is the choice, also with no wish
	const auto old = ChooseDefence(std::span(attacks).first(2), 5000);
	ASSERT_TRUE(old.has_value());
	EXPECT_EQ(old->index, 1u);
	EXPECT_EQ(old->value, 0.0f);
	// A larger wish wins, and a tie keeps the first
	const auto recent = ChooseDefence(attacks, 900);
	ASSERT_TRUE(recent.has_value());
	EXPECT_EQ(recent->index, 2u);
	EXPECT_EQ(recent->value, 1.0f);
	const std::array<TownAttackFacts, 1> none {{{.aggression = 0.0f}}};
	EXPECT_FALSE(ChooseDefence(none, 10).has_value());
}

TEST(ComputerMindDefenceRules, BeliefAttackFromTheRecentGainOfARival)
{
	TownBeliefFacts town;
	town.belief.at(0) = 4.0f;
	town.belief.at(1) = 1.0f;
	town.belief.at(2) = 3.0f;
	town.recent.at(1) = 0.5f;
	town.recent.at(2) = 0.25f;
	std::array<bool, k_Players> allies {};
	// Player two gave the most lately: (1 / 4) * 2, squared
	EXPECT_EQ(BeliefAttack(town, PlayerNames::PLAYER_ONE, allies), 0.25f);
	// An ally does not count, so player three is the attacker: (3 / 4) * 2, squared, is more than 1
	allies.at(1) = true;
	EXPECT_EQ(BeliefAttack(town, PlayerNames::PLAYER_ONE, allies), 1.0f);
	// A gain that is not more than the least gain does not count
	allies.at(1) = false;
	town.recent.at(1) = k_LeastBeliefAttack;
	town.recent.at(2) = 0.0f;
	EXPECT_EQ(BeliefAttack(town, PlayerNames::PLAYER_ONE, allies), 0.0f);
	// The gains of the god and of players seven and eight do not count
	town.recent.at(1) = 0.0f;
	town.recent.at(0) = 5.0f;
	town.recent.at(6) = 5.0f;
	town.recent.at(7) = 5.0f;
	EXPECT_EQ(BeliefAttack(town, PlayerNames::PLAYER_ONE, allies), 0.0f);
	// The town has no belief in the god
	town.recent.at(1) = 1.0f;
	town.belief.at(0) = 0.0f;
	EXPECT_EQ(BeliefAttack(town, PlayerNames::PLAYER_ONE, allies), 0.0f);
}

TEST(ComputerMindDefenceRules, BeliefMarginAndTheChoiceOfTown)
{
	TownBeliefFacts town;
	town.belief.at(0) = 4.0f;
	town.belief.at(3) = 1.5f;
	town.belief.at(7) = 2.5f;
	EXPECT_EQ(BeliefMargin(town, PlayerNames::PLAYER_ONE), 1.5f);
	TownBeliefFacts alone;
	alone.belief.at(0) = -1.0f;
	EXPECT_EQ(BeliefMargin(alone, PlayerNames::PLAYER_ONE), -1.0f);

	const std::array<float, 4> attacks {0.5f, 0.75f, 0.75f, 1.0f};
	const std::array<float, 4> margins {1.0f, -2.0f, 1.0f, 0.0f};
	const auto choice = ChooseBeliefDefence(attacks, margins);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->index, 1u);
	EXPECT_EQ(choice->value, 0.75f);
	const std::array<float, 1> noAttack {0.0f};
	const std::array<float, 1> someMargin {1.0f};
	EXPECT_FALSE(ChooseBeliefDefence(noAttack, someMargin).has_value());
}

TEST(ComputerMindDefenceRules, BeliefDefenceImportanceAndSuit)
{
	const std::array<float, 3> rivals {0.1f, 0.2f, -1.0f};
	EXPECT_EQ(CloseRivalShare(4.0f, rivals), 0.2f / 4.0f);
	EXPECT_EQ(CloseRivalShare(0.0f, rivals), 0.0f);
	EXPECT_EQ(BeliefDefenceImportance(true, 0.05f), 0.05f * k_CloseRivalScale);
	EXPECT_EQ(BeliefDefenceImportance(true, 0.5f), 1.0f);
	EXPECT_EQ(BeliefDefenceImportance(false, 0.0f), 1.0f);

	EXPECT_EQ(BeliefDefenceSuit(MagicType::Shield, false), 1.0f);
	EXPECT_EQ(BeliefDefenceSuit(MagicType::PhysicalShield, false), k_PhysicalShieldSuit);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_PhysicalShieldSuit), 0x3F666666u);
	EXPECT_EQ(BeliefDefenceSuit(MagicType::Shield, true), 0.0f);
	EXPECT_EQ(BeliefDefenceSuit(MagicType::Fireball, false), 0.0f);
}

TEST(ComputerMindDefenceRules, CreatureThreatNearTheTempleAndTheTowns)
{
	EXPECT_EQ(CreatureThreat(1000.0f, 10.0f, {}), gutils::GetDistanceModifier(1000.0f, 11.0f));
	const float nearTemple = CreatureThreat(5.0f, 10.0f, {});
	EXPECT_EQ(nearTemple, gutils::GetDistanceModifier(5.0f, 11.0f));
	// Inside a town adds 1, then the nearness against the reach of the influence of the town
	const std::array<TownNearCreature, 2> towns {{
	    {.distance = 10.0f, .radius = 9.0f, .influence = 100.0f},
	    {.distance = 50.0f, .radius = 40.0f, .influence = 40.0f},
	}};
	float sum = nearTemple + 1.0f;
	sum = gutils::GetDistanceModifier(10.0f, 100.0f) + sum;
	sum = gutils::GetDistanceModifier(50.0f, 40.0f) + sum;
	EXPECT_EQ(CreatureThreat(5.0f, 10.0f, towns), sum);
}

TEST(ComputerMindDefenceRules, CreatureHelpReach)
{
	EXPECT_EQ(CreatureHelpReach(1.0f), 230.0f);
	EXPECT_EQ(CreatureHelpReach(0.0f), 200.0f);
}

TEST(TownAggression, KeepsTheHarmOfTheLastAttackOfEachPlayer)
{
	ecs::town_aggression::Record record;
	ecs::town_aggression::Attacked(record, PlayerNames::PLAYER_TWO, false, 1.0f, 0.0f, 10,
	                               ecs::town_aggression::Harm {.burn = 3.0f, .crush = 1.0f, .hit = 0.5f});
	EXPECT_EQ(record.lastHarms.at(1).burn, 3.0f);
	EXPECT_EQ(record.lastHarms.at(1).crush, 1.0f);
	EXPECT_EQ(record.lastHarms.at(1).hit, 0.5f);
	// An attack with no harm, as a thing that strikes a shield, keeps no harm
	ecs::town_aggression::Attacked(record, PlayerNames::PLAYER_TWO, false, 0.0f, 0.0f, 20);
	EXPECT_EQ(record.lastHarms.at(1).burn, 0.0f);
	EXPECT_EQ(record.lastTurns.at(1), 20u);
}
