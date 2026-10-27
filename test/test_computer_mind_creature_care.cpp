/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "ECS/ComputerMind.h"
#include "ECS/ComputerMindCreatureRules.h"
#include "ECS/ComputerMindDomains.h"
#include "ECS/ComputerMindRules.h"
#include "ECS/Systems/Implementations/ComputerMindRuleTable.h"

using namespace openblack;
using namespace openblack::computer_mind;
using namespace openblack::computer_mind_rules;
namespace computer_world = openblack::ecs::systems::computer_world;

TEST(ComputerMindCreatureCare, OwnsItsKindsOfNodes)
{
	EXPECT_EQ(DomainOf(NodeKind::ActionGiveCreatureFood), MindDomain::CreatureCare);
	EXPECT_EQ(DomainOf(NodeKind::ActionReacttoCreatureIllEmergency), MindDomain::CreatureCare);
	EXPECT_EQ(DomainOf(NodeKind::ObjectRestoreMyCreatureHealth), MindDomain::CreatureCare);
}

TEST(ComputerMindCreatureCare, OwnsTheTopNodeOfTheCreature)
{
	EXPECT_EQ(DomainOf(NodeKind::TopCreatureInteract), MindDomain::CreatureCare);
}

TEST(ComputerMindCreatureCare, CreatureOffTheLeashIsFree)
{
	EXPECT_EQ(CreatureFreedom({.leashed = false, .fighting = true, .life = 0.0f}), 1.0f);
}

TEST(ComputerMindCreatureCare, CreatureIsNotFreeSoonAfterTheTieOrInAFight)
{
	EXPECT_EQ(CreatureFreedom({.leashed = true, .turnsSinceTied = 450}), 0.0f);
	EXPECT_EQ(CreatureFreedom({.leashed = true, .turnsSinceTied = 2000, .fighting = true}), 0.0f);
	EXPECT_EQ(CreatureFreedom({.leashed = true, .turnsSinceTied = 2000, .restoringHealth = true}), 0.0f);
}

TEST(ComputerMindCreatureCare, CreatureBecomesFreeWithTimeAfterTheTie)
{
	// 450 turns more give full freedom, and more than that stays at 1
	const float half = CreatureFreedom({.leashed = true, .turnsSinceTied = 675});
	EXPECT_FLOAT_EQ(half, (675.0f - 450.0f) * k_LeashFreedomPerTurn);
	EXPECT_EQ(CreatureFreedom({.leashed = true, .turnsSinceTied = 5000}), 1.0f);
}

TEST(ComputerMindCreatureCare, WeakTiredOrHungryCreatureIsLessFree)
{
	// 900 turns after the tie, a fit creature is fully free
	const CreatureFreedomFacts fit {.leashed = true, .turnsSinceTied = 900};
	auto weak = fit;
	weak.life = 0.4f;
	EXPECT_FLOAT_EQ(CreatureFreedom(weak), 0.4f);
	auto tired = fit;
	tired.exhaustion = 0.75f;
	EXPECT_FLOAT_EQ(CreatureFreedom(tired), 0.25f);
	// Less tired than half is fully rested
	tired.exhaustion = 0.49f;
	EXPECT_FLOAT_EQ(CreatureFreedom(tired), 1.0f);
	auto hungry = fit;
	hungry.energy = 0.3f;
	EXPECT_FLOAT_EQ(CreatureFreedom(hungry), 0.3f);
}

TEST(ComputerMindCreatureCare, StarvingCreatureWishAndAmount)
{
	EXPECT_FLOAT_EQ(StarvingWish(0.25f), 0.25f);
	EXPECT_FLOAT_EQ(StarvingWish(3.0f), 1.0f);
	// The amount is the wish in thousandths, made whole toward 0
	EXPECT_EQ(StarvingAmount(0.2509f), 250);
	EXPECT_EQ(StarvingAmount(1.0f), 1000);
}

TEST(ComputerMindCreatureCare, JourneySpeedIsAPartOfTheTopSpeed)
{
	EXPECT_TRUE(LongJourney({0.0f, 0.0f, 0.0f}, {60.0f, 60.0f, 60.0f}));
	EXPECT_FALSE(LongJourney({0.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}));
	EXPECT_FLOAT_EQ(JourneySpeed(true, 0.3f, 0.6f), 0.9f);
	EXPECT_EQ(JourneySpeed(true, 0.3f, 0.8f), 1.0f);
	EXPECT_FLOAT_EQ(JourneySpeed(false, 0.3f, 0.6f), 0.45f);
	EXPECT_EQ(JourneySpeed(false, -0.5f, -0.1f), 0.0f);
}

TEST(ComputerMindCreatureCare, EachKindOfThePartHasItsRules)
{
	computer_world::RuleTable table {};
	computer_world::RegisterCreatureCareRules(table);
	const auto rule = [&table](NodeKind kind) { return table.at(static_cast<size_t>(kind)); };
	// The top node and the actions that a leaf below carries out have only a wish
	for (const auto kind : {NodeKind::TopCreatureInteract, NodeKind::ActionCastFoodForMyCreature,
	                        NodeKind::ActionRestoreCreatureHealth, NodeKind::ActionReacttoCreatureDamagedEmergency})
	{
		EXPECT_NE(rule(kind).intensity, nullptr) << static_cast<int>(kind);
		EXPECT_EQ(rule(kind).perform, nullptr) << static_cast<int>(kind);
	}
	for (const auto kind :
	     {NodeKind::ActionGiveCreatureFood, NodeKind::ActionMakeCreatureDrink, NodeKind::ActionMakeCreatureSleep,
	      NodeKind::ActionSendCreatureHome, NodeKind::ActionMoveCreaturetoWarmerArea, NodeKind::ActionMoveCreaturetoCoollerArea,
	      NodeKind::ActionReacttoCreatureStarvingEmergency, NodeKind::ActionReacttoCreatureIllEmergency,
	      NodeKind::ObjectCastFoodForMyCreature, NodeKind::ObjectRestoreMyCreatureHealth,
	      NodeKind::ObjectRestoreMyCreatureHealthAgain})
	{
		EXPECT_NE(rule(kind).intensity, nullptr) << static_cast<int>(kind);
		EXPECT_NE(rule(kind).perform, nullptr) << static_cast<int>(kind);
	}
}
