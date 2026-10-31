/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <bit>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/ComputerMind.h"

using namespace openblack;
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
	[[nodiscard]] std::optional<glm::vec3> MarkerPlace(entt::entity thing) const override
	{
		return thing == marker ? std::optional<glm::vec3>(markerPlace) : std::nullopt;
	}
	[[nodiscard]] bool HeldByHelp() const override { return heldByHelp; }

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
	entt::entity marker {entt::null};
	glm::vec3 markerPlace {0.0f};
	bool heldByHelp {false};
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
	// The game keeps a timer on the creature impress action too, which only the top layer would read
	EXPECT_EQ(InfoOf(NodeKind::ObjectUseCreatureToImpressTown).timer, 3300u);
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

TEST(ComputerMind, NameSearchFindsANodeBeforeItsChildrenAndTheFirstChildFirst)
{
	const auto mind = MakeMind();
	const auto kindsOf = [&mind](const std::vector<uint16_t>& found) {
		std::vector<NodeKind> kinds;
		std::ranges::transform(found, std::back_inserter(kinds), [&mind](uint16_t node) { return mind.nodes[node].kind; });
		return kinds;
	};
	// The need above the action has a shorter name that starts the name of the action, so the search finds the need first
	const auto found = FindNodes(mind, "ReactToPlayerThrowingStuff");
	const std::vector<NodeKind> expected {NodeKind::NeedReactToPlayer, NodeKind::ActionReactToPlayerThrowingStuff,
	                                      NodeKind::TopReactToPlayer, NodeKind::NeedReactToPlayer,
	                                      NodeKind::ActionReactToPlayerThrowingStuff};
	EXPECT_EQ(kindsOf(found), expected);
	// The search goes in the order of the tree: the root first, then each child and all of its branch in turn
	EXPECT_TRUE(std::ranges::is_sorted(found));
	// A script that forces the creature defence of a town starts the first node found: the town defence above it
	const auto defence = FindNodes(mind, "DefendTownWithCreature");
	ASSERT_FALSE(defence.empty());
	EXPECT_EQ(defence.front(), NodeOf(mind, NodeKind::ActionDefendTown));
	// A name that is shorter than the name of a node does not find the node
	EXPECT_TRUE(FindNodes(mind, "ReactToPlay").empty());
	// A weight for the miracle attack on a creature also goes to the creature attack above it, but not to its other child
	auto weights = MakeMind();
	EXPECT_EQ(FindNodes(weights, "AttackCreatureWithSpells").size(), 4u);
	EXPECT_TRUE(SetPersonality(weights, "AttackCreatureWithSpells", 0.5f));
	EXPECT_FLOAT_EQ(weights.nodes[NodeOf(weights, NodeKind::ActionAttackCreature)].weight, 0.5f);
	EXPECT_FLOAT_EQ(weights.nodes[NodeOf(weights, NodeKind::ObjectAttackCreatureWithSpells)].weight, 0.5f);
	EXPECT_NE(weights.nodes[NodeOf(weights, NodeKind::ObjectAttackCreatureWithCreature)].weight, 0.5f);
}

TEST(ComputerMind, KindsThatTheirOwnNameDoesNotFindFirst)
{
	const auto mind = MakeMind();
	const auto shortName = [](NodeKind kind) {
		const auto name = NameOf(FirstNodeNames(), kind);
		return name.substr(0, name.rfind(':'));
	};
	std::vector<NodeKind> hidden;
	for (size_t i = 0; i < static_cast<size_t>(NodeKind::_Count); ++i)
	{
		const auto kind = static_cast<NodeKind>(i);
		// Some rows of the table are under no node of the tree
		if (std::ranges::none_of(mind.nodes, [kind](const Node& node) { return node.kind == kind; }))
		{
			continue;
		}
		const auto name = shortName(kind);
		const auto found = FindNodes(mind, name);
		ASSERT_FALSE(found.empty());
		// Some kinds have the same name as other kinds: a node of the same name found first does not hide the kind
		if (shortName(mind.nodes[found.front()].kind) != name)
		{
			hidden.push_back(kind);
		}
	}
	// A node whose name starts with the name of a node that comes before it in the tree is never the first node found
	std::vector<NodeKind> expected {NodeKind::ActionGiveToPlayer,
	                                NodeKind::ActionTeachCreatureNormalAction,
	                                NodeKind::ActionTeachCreatureAggressiveMagic,
	                                NodeKind::ActionTeachCreatureCompassionateMagic,
	                                NodeKind::ActionTeachCreatureImpressiveMagic,
	                                NodeKind::ObjectTeachCreatureAggressiveMagic,
	                                NodeKind::ObjectTeachCreatureCompassionateMagic,
	                                NodeKind::ObjectTeachCreatureImpressiveMagic,
	                                NodeKind::ObjectGetCreaturetoHelpBuild,
	                                NodeKind::ObjectCastFoodSpellOnWorshipSite,
	                                NodeKind::ObjectAttackCreatureWithSpells,
	                                NodeKind::ObjectAttackCreatureWithCreature,
	                                NodeKind::ActionReactToPlayerCastingSpells,
	                                NodeKind::ActionReactToPlayerThrowingStuff,
	                                NodeKind::ObjectDefendTownWithCreature,
	                                NodeKind::ActionDefendTownAgainstBeliefAttack};
	std::ranges::sort(hidden);
	std::ranges::sort(expected);
	EXPECT_EQ(hidden, expected);
}

TEST(ComputerMindScript, DirectActionUsesBothThingsAndHoldsTheHelp)
{
	auto mind = MakeMind();
	FakeWorld world;
	constexpr auto k_Thing = static_cast<entt::entity>(3);
	constexpr auto k_Marker = static_cast<entt::entity>(4);
	EXPECT_TRUE(ForceAction(mind, world, "DIRECTPickupAndDropObject", k_Thing, k_Marker, 1.0f, 10));
	const auto node = NodeOf(mind, NodeKind::ObjectDIRECTPickupAndDropObject);
	ASSERT_EQ(world.performed.size(), 1u);
	EXPECT_EQ(world.performed[0], NodeKind::ObjectDIRECTPickupAndDropObject);
	EXPECT_EQ(mind.nodes[node].object, k_Thing);
	EXPECT_EQ(mind.nodes[node].argument, k_Marker);
	EXPECT_TRUE(mind.scriptHand);
	// The node gets the suppression after its action, which is 0 for this node
	EXPECT_FLOAT_EQ(mind.nodes[node].suppression, 0.0f);
	// A name that no node has does nothing
	EXPECT_FALSE(ForceAction(mind, world, "NoSuchNode", k_Thing, k_Marker, 1.0f, 10));
}

TEST(ComputerMindScript, DirectActionThatCannotStartGivesTheThingsToItsParent)
{
	auto mind = MakeMind();
	FakeWorld world;
	world.performWorks = false;
	constexpr auto k_Thing = static_cast<entt::entity>(3);
	constexpr auto k_Other = static_cast<entt::entity>(4);
	EXPECT_FALSE(ForceAction(mind, world, "DIRECTPickupAndDropObject", k_Thing, k_Other, 1.0f, 10));
	const auto node = NodeOf(mind, NodeKind::ObjectDIRECTPickupAndDropObject);
	// The node has an action and no children: the node above keeps the first thing, the node keeps the second
	EXPECT_EQ(mind.nodes[*mind.nodes[node].parent].object, k_Thing);
	EXPECT_EQ(mind.nodes[node].object, k_Other);
	EXPECT_FLOAT_EQ(mind.nodes[node].suppression, k_FailedSuppression);
	EXPECT_FALSE(mind.scriptHand);
}

TEST(ComputerMindScript, ActionWithChildrenGivesTheSecondThingOrTheMarkerPlaceToItsFirstChild)
{
	auto mind = MakeMind();
	FakeWorld world;
	constexpr auto k_Town = static_cast<entt::entity>(3);
	constexpr auto k_Field = static_cast<entt::entity>(4);
	const auto node = NodeOf(mind, NodeKind::ActionTownWithHighestFoodDesire);
	const auto child = mind.nodes[node].children[0];
	EXPECT_TRUE(ForceAction(mind, world, "TownWithHighestFoodDesire", k_Town, k_Field, 2.0f, 10));
	EXPECT_EQ(mind.nodes[node].object, k_Town);
	EXPECT_FLOAT_EQ(mind.nodes[node].value, 2.0f);
	EXPECT_EQ(mind.nodes[child].object, k_Field);
	EXPECT_EQ(mind.nodes[node].bestLeaf, child);
	EXPECT_TRUE(mind.scriptHand);
	// A script marker as the second thing gives its place to the node and to its first child
	world.marker = static_cast<entt::entity>(5);
	world.markerPlace = {100.0f, 20.0f, 300.0f};
	mind.scriptHand = false;
	EXPECT_TRUE(ForceAction(mind, world, "TownWithHighestFoodDesire", k_Town, world.marker, 1.0f, 11));
	EXPECT_FLOAT_EQ(mind.nodes[node].place.z, 300.0f);
	EXPECT_FLOAT_EQ(mind.nodes[child].place.x, 100.0f);
	EXPECT_TRUE(mind.scriptHand);
}

TEST(ComputerMindScript, QueuedActionsStartInOrderWhenTheHandIsFree)
{
	auto mind = MakeMind();
	mind.wasBusy = false;
	FakeWorld world;
	world.actionTurns = 2;
	QueueAction(mind, "DIRECTPickupAndDropObject", static_cast<entt::entity>(3), entt::null, 5);
	QueueAction(mind, "ChargeUpSpell", entt::null, entt::null, 6);
	ASSERT_EQ(mind.queue.size(), 2u);
	EXPECT_EQ(mind.queue[1].turn, 6u);
	ProcessTurn(mind, world, 100);
	ASSERT_EQ(world.performed.size(), 1u);
	EXPECT_EQ(world.performed[0], NodeKind::ObjectDIRECTPickupAndDropObject);
	ASSERT_EQ(mind.queue.size(), 1u);
	// The hand does the action; the script action ends with its last step
	ProcessTurn(mind, world, 101);
	EXPECT_TRUE(mind.scriptHand);
	ProcessTurn(mind, world, 102);
	EXPECT_FALSE(mind.scriptHand);
	// A long name keeps its first 260 letters
	QueueAction(mind, std::string(300, 'a'), entt::null, entt::null, 7);
	EXPECT_EQ(mind.queue.back().name.size(), k_QueuedNameLength);
}

TEST(ComputerMindScript, HelpHoldsTheMind)
{
	auto mind = MakeMind();
	mind.wasBusy = false;
	FakeWorld world;
	world.heldByHelp = true;
	world.intensities[NodeKind::ActionChargeUpSpell] = 1.0f;
	ProcessTurn(mind, world, 100);
	EXPECT_TRUE(world.performed.empty());
	EXPECT_TRUE(world.asked.empty());
}

namespace
{

/// A map with things in some cells. Things 1 to 9 are trees of forest 100, 10 and above are seeds, and 50 is gone.
class FakeSeenWorld final: public SeenWorld
{
public:
	[[nodiscard]] std::vector<entt::entity> ThingsIn(glm::ivec2 cell) const override
	{
		++looked;
		const auto found = cells.find({cell.x, cell.y});
		return found != cells.end() ? found->second : std::vector<entt::entity> {};
	}
	[[nodiscard]] entt::entity SeenAs(entt::entity thing) const override
	{
		const auto value = static_cast<uint32_t>(thing);
		if (value < 10)
		{
			return static_cast<entt::entity>(100);
		}
		return value < 200 ? thing : entt::null;
	}
	[[nodiscard]] bool Available(entt::entity thing) const override
	{
		return thing != entt::null && thing != static_cast<entt::entity>(50);
	}

	std::map<std::pair<int, int>, std::vector<entt::entity>> cells;
	mutable int looked {0};
};

} // namespace

TEST(ComputerMindSeen, GodSeesForestsAndSeedsNearItsHandOnce)
{
	FakeSeenWorld world;
	// The hand is in the middle of cell (100, 100)
	const map_coords::MapCoords hand {.x = map_coords::ToFixed(1005.0f), .z = map_coords::ToFixed(1005.0f), .altitude = 0.0f};
	world.cells[{100, 100}] = {static_cast<entt::entity>(1), static_cast<entt::entity>(2), static_cast<entt::entity>(50)};
	world.cells[{101, 100}] = {static_cast<entt::entity>(11), static_cast<entt::entity>(300)};
	world.cells[{102, 100}] = {static_cast<entt::entity>(12)};
	// Cell (106, 100) is 60 metres away, past the radius and the margin
	world.cells[{106, 100}] = {static_cast<entt::entity>(13)};
	std::vector<SeenThing> seen;
	SeeNearHand(seen, hand, 7, world);
	// The two trees give their forest once; the seeds come after, the newest first
	ASSERT_EQ(seen.size(), 3u);
	EXPECT_EQ(seen[0].thing, static_cast<entt::entity>(12));
	EXPECT_EQ(seen[1].thing, static_cast<entt::entity>(11));
	EXPECT_EQ(seen[2].thing, static_cast<entt::entity>(100));
	EXPECT_EQ(seen[2].turn, 7u);
	// The search stops at the first cell of the spiral that is too far: a square of 9 cells across has its corners
	// further than 40 metres
	EXPECT_LT(world.looked, 81);
	// Things already in the list are not added again
	SeeNearHand(seen, hand, 8, world);
	EXPECT_EQ(seen.size(), 3u);
}

TEST(ComputerMindSeen, LongListForgetsOldThingsEveryFiveHundredTurns)
{
	FakeSeenWorld world;
	const map_coords::MapCoords hand {.x = map_coords::ToFixed(1005.0f), .z = map_coords::ToFixed(1005.0f), .altitude = 0.0f};
	std::vector<SeenThing> seen;
	for (uint32_t i = 0; i < 51; ++i)
	{
		seen.push_back({.thing = static_cast<entt::entity>(10 + i), .turn = i < 10 ? 0u : 9000u});
	}
	SeeNearHand(seen, hand, 9001, world);
	EXPECT_EQ(seen.size(), 51u);
	SeeNearHand(seen, hand, 9000, world);
	EXPECT_EQ(seen.size(), 41u);
}

namespace
{

/// A world in which the even miracles are gone and the things from 50 up are not available
class FakeGoneWorld final: public GoneWorld
{
public:
	[[nodiscard]] bool CastStillThere(entt::entity spell) const override { return static_cast<uint32_t>(spell) % 2 != 0; }
	[[nodiscard]] bool Available(entt::entity thing) const override { return static_cast<uint32_t>(thing) < 50; }
};

entt::entity Thing(uint32_t value)
{
	return static_cast<entt::entity>(value);
}

} // namespace

TEST(ComputerMindGone, GodForgetsGoneMiraclesSeenThingsAndQueuedActionsInOrder)
{
	auto mind = MakeMind();
	std::vector<entt::entity> casts {Thing(5), Thing(4), Thing(3), Thing(2), Thing(1)};
	std::vector<SeenThing> seen {
	    {.thing = Thing(60), .turn = 3}, {.thing = Thing(20), .turn = 2}, {.thing = Thing(10), .turn = 1}};
	QueueAction(mind, "First", Thing(10), entt::null, 1);
	QueueAction(mind, "Second", Thing(10), Thing(70), 2);
	QueueAction(mind, "Third", entt::null, Thing(20), 3);
	ForgetGone(casts, seen, mind, FakeGoneWorld {});
	EXPECT_EQ(casts, (std::vector<entt::entity> {Thing(5), Thing(3), Thing(1)}));
	ASSERT_EQ(seen.size(), 2u);
	EXPECT_EQ(seen[0].thing, Thing(20));
	EXPECT_EQ(seen[1].thing, Thing(10));
	ASSERT_EQ(mind.queue.size(), 2u);
	EXPECT_EQ(mind.queue[0].name, "First");
	EXPECT_EQ(mind.queue[1].name, "Third");
}

TEST(ComputerMind, NodeIsWorkedOutAgainInTheSameTurn)
{
	// The game has no check that keeps the value of a node that it already worked out in the turn
	auto mind = MakeMind();
	FakeWorld world;
	world.intensities[NodeKind::ActionTownWithHighestFoodDesire] = 0.5f;
	world.intensities[NodeKind::ObjectGetFoodFromField] = 0.5f;
	const auto node = NodeOf(mind, NodeKind::ActionTownWithHighestFoodDesire);
	EXPECT_FLOAT_EQ(WorkOut(mind, world, node, 9), 0.25f);
	world.intensities[NodeKind::ObjectGetFoodFromField] = 1.0f;
	EXPECT_FLOAT_EQ(WorkOut(mind, world, node, 9), 0.5f);
	EXPECT_EQ(world.asked[NodeKind::ActionTownWithHighestFoodDesire], 2);
	EXPECT_EQ(world.asked[NodeKind::ObjectGetFoodFromField], 2);
	EXPECT_EQ(mind.nodes[node].lastWorkedOut, 9u);
}
