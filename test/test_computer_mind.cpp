/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <map>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/ComputerMind.h"

using namespace openblack::computer_mind;

namespace
{

/// A world that gives each kind of node the intensity that the test sets, and keeps a record of the actions
class FakeWorld final: public World
{
public:
	float Intensity(Mind& mind, uint16_t node) override
	{
		const auto kind = mind.nodes[node].kind;
		++asked[kind];
		const auto found = intensities.find(kind);
		return found != intensities.end() ? found->second : 0.0f;
	}

	bool Perform(Mind& mind, uint16_t node) override
	{
		performed.push_back(mind.nodes[node].kind);
		if (performWorks)
		{
			busyTurns = actionTurns;
		}
		return performWorks;
	}

	[[nodiscard]] bool ActionBusy() const override { return busyTurns > 0; }

	bool StepAction() override
	{
		++steps;
		if (busyTurns > 0)
		{
			--busyTurns;
		}
		return !endsBadly;
	}

	void ClearAction() override { ++clears; }
	void StartAction() override { ++starts; }
	[[nodiscard]] bool ActionLocked() const override { return locked; }
	void UnlockAction() override { locked = false; }
	bool ForceQueued() override { return false; }
	[[nodiscard]] bool HeldByHelp() const override { return false; }

	std::map<NodeKind, float> intensities;
	std::map<NodeKind, int> asked;
	std::vector<NodeKind> performed;
	bool performWorks {true};
	bool endsBadly {false};
	bool locked {false};
	int actionTurns {0};
	int busyTurns {0};
	int steps {0};
	int clears {0};
	int starts {0};
};

/// The first node of the kind in the tree
uint16_t NodeOf(const Mind& mind, NodeKind kind)
{
	for (uint16_t i = 0; i < mind.nodes.size(); ++i)
	{
		if (mind.nodes[i].kind == kind)
		{
			return i;
		}
	}
	return 0;
}

} // namespace

TEST(ComputerMind, TreeHasTheShapeOfTheGame)
{
	const auto mind = MakeMind();
	ASSERT_EQ(mind.nodes.size(), 198u);
	EXPECT_EQ(mind.nodes[0].childCount, 6);
	EXPECT_EQ(mind.nodes[mind.nodes[0].children[2]].kind, NodeKind::TopTownInteract);
	// The town desires list the wood for building sites two times, each with its own node
	const auto& town = mind.nodes[NodeOf(mind, NodeKind::NeedAttendTownDesires)];
	ASSERT_EQ(town.childCount, 5);
	EXPECT_EQ(mind.nodes[town.children[2]].kind, NodeKind::ActionGiveBuildingSiteWood);
	EXPECT_EQ(mind.nodes[town.children[4]].kind, NodeKind::ActionGiveBuildingSiteWood);
	EXPECT_NE(town.children[2], town.children[4]);
	EXPECT_FLOAT_EQ(town.weight, 0.7f);
	EXPECT_EQ(InfoOf(NodeKind::TopReactToPlayer).timer, 50u);
}

TEST(ComputerMind, SuppressionRecoversAtTheRateOfTheGame)
{
	auto mind = MakeMind();
	FakeWorld world;
	const auto node = NodeOf(mind, NodeKind::ActionChargeUpSpell);
	mind.nodes[node].suppression = 0.5f;
	mind.nodes[node].lastWorkedOut = 0;
	WorkOut(mind, world, node, 100);
	EXPECT_EQ(std::bit_cast<uint32_t>(mind.nodes[node].suppression), 0x3F0CCCF4u);
	// It never goes over 1
	mind.nodes[node].suppression = 0.99f;
	WorkOut(mind, world, node, 100000);
	EXPECT_EQ(mind.nodes[node].suppression, 1.0f);
}

TEST(ComputerMind, NodeNotWantedDoesNotLookAtItsChildren)
{
	auto mind = MakeMind();
	FakeWorld world;
	const auto node = NodeOf(mind, NodeKind::ActionTownWithHighestFoodDesire);
	EXPECT_EQ(WorkOut(mind, world, node, 1), 0.0f);
	EXPECT_EQ(world.asked[NodeKind::ObjectShallCastFoodSpell], 0);
	EXPECT_EQ(mind.nodes[node].bestLeaf, node);
}

TEST(ComputerMind, ValueIsTheNodeTimesItsBestChild)
{
	auto mind = MakeMind();
	FakeWorld world;
	world.intensities[NodeKind::ActionTownWithHighestFoodDesire] = 0.5f;
	world.intensities[NodeKind::ObjectGetFoodFromField] = 0.25f;
	world.intensities[NodeKind::ObjectShallCastFoodSpell] = 0.75f;
	const auto node = NodeOf(mind, NodeKind::ActionTownWithHighestFoodDesire);
	EXPECT_FLOAT_EQ(WorkOut(mind, world, node, 1), 0.75f * 0.5f);
	EXPECT_EQ(mind.nodes[*mind.nodes[node].bestLeaf].kind, NodeKind::ObjectShallCastFoodSpell);
}

TEST(ComputerMind, EqualChildrenKeepTheFirst)
{
	auto mind = MakeMind();
	FakeWorld world;
	world.intensities[NodeKind::ActionTownWithHighestFoodDesire] = 1.0f;
	world.intensities[NodeKind::ObjectGetFoodFromField] = 0.5f;
	world.intensities[NodeKind::ObjectGetFoodFromFishFarm] = 0.5f;
	const auto node = NodeOf(mind, NodeKind::ActionTownWithHighestFoodDesire);
	WorkOut(mind, world, node, 1);
	EXPECT_EQ(mind.nodes[*mind.nodes[node].bestLeaf].kind, NodeKind::ObjectGetFoodFromField);
}

TEST(ComputerMind, ChildIsSkippedWhenTheNodeCannotGetStronger)
{
	auto mind = MakeMind();
	FakeWorld world;
	// After a child that gives more than the weight times the suppression, no other child can give more
	world.intensities[NodeKind::ActionTownWithHighestFoodDesire] = 1.0f;
	world.intensities[NodeKind::ObjectCreateDiscipleToFulfillTownDesire] = 2.0f;
	const auto node = NodeOf(mind, NodeKind::ActionTownWithHighestFoodDesire);
	mind.nodes[node].suppression = 0.5f;
	mind.nodes[node].lastWorkedOut = 1;
	WorkOut(mind, world, node, 1);
	EXPECT_EQ(world.asked[NodeKind::ObjectCreateDiscipleToFulfillTownDesire], 1);
	EXPECT_EQ(world.asked[NodeKind::ObjectGetFoodFromField], 0);
}

TEST(ComputerMind, ObjectComesFromTheNodeAbove)
{
	auto mind = MakeMind();
	FakeWorld world;
	const auto top = NodeOf(mind, NodeKind::TopTownInteract);
	const auto need = NodeOf(mind, NodeKind::NeedAttendToWorshippersWorshippers);
	mind.nodes[top].object = static_cast<entt::entity>(7);
	// The top node keeps its object only until it is looked at again, so the need is looked at directly
	WorkOut(mind, world, need, 1);
	EXPECT_EQ(mind.nodes[need].object, static_cast<entt::entity>(7));
}

TEST(ComputerMind, PerformUsesTheFirstNodeWithAnAction)
{
	auto mind = MakeMind();
	FakeWorld world;
	world.intensities[NodeKind::ActionTownWithHighestWoodDesire] = 1.0f;
	world.intensities[NodeKind::ObjectGetWoodFromForest] = 1.0f;
	ASSERT_GT(WorkOut(mind, world, 0, 5), 0.0f);
	ASSERT_EQ(mind.nodes[*mind.nodes[0].bestLeaf].kind, NodeKind::ObjectGetWoodFromForest);
	EXPECT_TRUE(PerformDecided(mind, world, 5));
	// The forest has no action of its own, so the town's action carries the wood
	ASSERT_EQ(world.performed.size(), 1u);
	EXPECT_EQ(world.performed[0], NodeKind::ActionTownWithHighestWoodDesire);
	EXPECT_EQ(world.clears, 1);
	EXPECT_EQ(world.starts, 1);
	EXPECT_FLOAT_EQ(mind.doing, mind.nodes[0].value);
	EXPECT_EQ(mind.nodes[NodeOf(mind, NodeKind::ObjectGetWoodFromForest)].lastPerformed, 5u);
}

TEST(ComputerMind, FailedPerformSuppressesTheNode)
{
	auto mind = MakeMind();
	FakeWorld world;
	world.performWorks = false;
	world.intensities[NodeKind::ActionChargeUpSpell] = 1.0f;
	WorkOut(mind, world, 0, 5);
	EXPECT_FALSE(PerformDecided(mind, world, 5));
	EXPECT_FLOAT_EQ(mind.nodes[NodeOf(mind, NodeKind::ActionChargeUpSpell)].suppression, k_FailedSuppression);
	EXPECT_EQ(world.starts, 0);
}

TEST(ComputerMind, MindWaitsAfterItFoundNothing)
{
	auto mind = MakeMind();
	FakeWorld world;
	// The first turn only clears the flag of the last action
	ProcessTurn(mind, world, 1);
	EXPECT_FALSE(mind.wasBusy);
	EXPECT_EQ(world.asked[NodeKind::ActionChargeUpSpell], 0);
	ProcessTurn(mind, world, 2);
	EXPECT_EQ(mind.lastEmptyTurn, 2u);
	EXPECT_EQ(world.asked[NodeKind::ActionChargeUpSpell], 1);
	ProcessTurn(mind, world, 31);
	EXPECT_EQ(world.asked[NodeKind::ActionChargeUpSpell], 1);
	ProcessTurn(mind, world, 32);
	EXPECT_EQ(world.asked[NodeKind::ActionChargeUpSpell], 2);
}

TEST(ComputerMind, ActionRunsThenMindRests)
{
	auto mind = MakeMind();
	FakeWorld world;
	world.actionTurns = 2;
	world.intensities[NodeKind::ActionChargeUpSpell] = 1.0f;
	mind.wasBusy = false;
	ProcessTurn(mind, world, 10);
	ASSERT_EQ(world.performed.size(), 1u);
	EXPECT_EQ(mind.lastEmptyTurn, 0u);
	ProcessTurn(mind, world, 11);
	ProcessTurn(mind, world, 12);
	EXPECT_EQ(world.steps, 2);
	EXPECT_TRUE(mind.wasBusy);
	// One turn of rest after the action, then a new choice
	ProcessTurn(mind, world, 13);
	EXPECT_EQ(world.performed.size(), 1u);
	ProcessTurn(mind, world, 14);
	EXPECT_EQ(world.performed.size(), 2u);
}

TEST(ComputerMind, BadEndHalvesTheChosenNode)
{
	auto mind = MakeMind();
	FakeWorld world;
	world.actionTurns = 3;
	world.intensities[NodeKind::ActionChargeUpSpell] = 1.0f;
	mind.wasBusy = false;
	ProcessTurn(mind, world, 10);
	const auto node = NodeOf(mind, NodeKind::ActionChargeUpSpell);
	const float before = mind.nodes[node].suppression;
	world.endsBadly = true;
	ProcessTurn(mind, world, 11);
	EXPECT_FLOAT_EQ(mind.nodes[node].suppression, before * 0.5f);
}

TEST(ComputerMind, StrongerNeedCutsTheActionShort)
{
	auto mind = MakeMind();
	FakeWorld world;
	world.actionTurns = 100;
	world.intensities[NodeKind::ActionChargeUpSpell] = 0.1f;
	mind.wasBusy = false;
	ProcessTurn(mind, world, 1);
	ASSERT_EQ(world.performed.size(), 1u);
	// The town aim is looked at only on turns that its timer of 300 divides
	world.intensities[NodeKind::ActionChargeUpSpell] = 1.0f;
	ProcessTurn(mind, world, 299);
	EXPECT_EQ(world.performed.size(), 1u);
	ProcessTurn(mind, world, 300);
	EXPECT_EQ(world.performed.size(), 2u);
	// A locked action is not cut short
	world.locked = true;
	mind.doing = 0.01f;
	ProcessTurn(mind, world, 600);
	EXPECT_EQ(world.performed.size(), 2u);
}

TEST(ComputerMind, NamesSetEveryNodeThatStartsTheSame)
{
	auto mind = MakeMind();
	// The aim and the need of the same name both change
	EXPECT_EQ(FindNodes(mind, "DefeatPlayer").size(), 2u);
	EXPECT_TRUE(SetPersonality(mind, "DefeatPlayer", 0.3f));
	EXPECT_FLOAT_EQ(mind.nodes[NodeOf(mind, NodeKind::TopDefeatPlayer)].weight, 0.3f);
	EXPECT_FLOAT_EQ(mind.nodes[NodeOf(mind, NodeKind::NeedDefeatPlayer)].weight, 0.3f);
	// A longer name that starts with a node name finds the node; a shorter one does not. The charge is under three needs.
	EXPECT_EQ(FindNodes(mind, "ChargeUpSpellNow").size(), 3u);
	EXPECT_TRUE(FindNodes(mind, "ChargeUp").empty());
	EXPECT_FALSE(SetSuppression(mind, "chargeupspell", 0.5f));
	EXPECT_TRUE(SetSuppression(mind, "ChargeUpSpell", 0.5f));
	EXPECT_FLOAT_EQ(mind.nodes[NodeOf(mind, NodeKind::ActionChargeUpSpell)].suppression, 0.5f);
}
