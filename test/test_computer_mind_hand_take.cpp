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

#include <algorithm>

#include <gtest/gtest.h>

#include "ComputerHandFake.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/ComputerMind.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/ComputerMindCommon.h"
#include "ECS/Systems/Implementations/ComputerMindRuleTable.h"
#include "Locator.h"

namespace
{

using namespace openblack::ecs::components;
namespace computer_world = openblack::ecs::systems::computer_world;
using openblack::computer_mind::MakeMind;
using openblack::computer_mind::Node;
using openblack::computer_mind::NodeKind;

/// The hand of a computer god and a registry for the things that it takes
class ComputerMindHandTake: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<openblack::ecs::Registry>(); }
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	[[nodiscard]] static openblack::ecs::Registry& Things() { return Locator::entitiesRegistry::value(); }

	computer_world::RuleContext Context() { return {.player = PlayerNames::PLAYER_TWO, .state = _state, .hand = _hand}; }

	/// The node of a kind in the mind of the god
	[[nodiscard]] uint16_t NodeOf(NodeKind kind) const
	{
		return static_cast<uint16_t>(std::ranges::find(_state.mind.nodes, kind, &Node::kind) - _state.mind.nodes.begin());
	}

	ComputerPlayer _state {.mind = MakeMind(), .speed = 45.0f};
	FakeWorld _fake;
	const World _hand {.moves = _fake, .objects = _fake, .casts = _fake, .creature = _fake, .totem = _fake};
};

TEST_F(ComputerMindHandTake, TheHandTakesAThingAtTheSpeedOfTheGod)
{
	const auto thing = Things().Create();
	auto context = Context();
	ASSERT_TRUE(computer_world::TakeUp(context, thing));
	ASSERT_EQ(_state.hand.steps.size(), 1U);
	EXPECT_EQ(_state.hand.steps.at(0).kind, StepKind::PickUpObject);
	EXPECT_EQ(_state.hand.steps.at(0).arguments.at(0).object, thing);
	EXPECT_FLOAT_EQ(_state.hand.steps.at(0).arguments.at(1).value, 45.0f);
}

TEST_F(ComputerMindHandTake, TheHandDoesNotTakeAThingThatAHandHolds)
{
	const auto thing = Things().Create();
	Things().Assign<InHand>(thing);
	auto context = Context();
	EXPECT_FALSE(computer_world::TakeUp(context, thing));
	EXPECT_FALSE(computer_world::TakeUp(context, entt::null));
	EXPECT_TRUE(_state.hand.steps.empty());
}

TEST_F(ComputerMindHandTake, AnActionStopsBeforeItsFirstStepWhenAHandHoldsTheThing)
{
	// The god joins its scaffold with the scaffold that the chosen leaf found
	const auto held = Things().Create();
	const auto target = Things().Create();
	Things().Assign<Scaffold>(held);
	Things().Assign<Scaffold>(target);
	const auto join = NodeOf(NodeKind::ActionCombineScaffolds);
	const auto other = NodeOf(NodeKind::ObjectFindScaffoldToCombineWith);
	_state.mind.nodes[join].object = held;
	_state.mind.nodes[join].bestLeaf = other;
	_state.mind.nodes[other].object = target;
	const auto perform = computer_world::Rules().at(static_cast<size_t>(NodeKind::ActionCombineScaffolds)).perform;
	ASSERT_NE(perform, nullptr);

	auto context = Context();
	Things().Assign<InHand>(held);
	EXPECT_FALSE(perform(context, _state.mind, join));
	EXPECT_TRUE(_state.hand.steps.empty());

	Things().Remove<InHand>(held);
	EXPECT_TRUE(perform(context, _state.mind, join));
	ASSERT_EQ(_state.hand.steps.size(), 2U);
	EXPECT_EQ(_state.hand.steps.at(0).kind, StepKind::PickUpObject);
	EXPECT_EQ(_state.hand.steps.at(1).kind, StepKind::DropObjectInHandOnObject);
}

TEST_F(ComputerMindHandTake, ADamagedPitStaysTheStorePitOfItsTown)
{
	const auto town = Things().Create();
	Things().Assign<Town>(town, Town {.id = 3});
	EXPECT_FALSE(computer_world::WorkingPitOf(town).has_value());

	const auto pit = Things().Create();
	Things().Assign<Abode>(pit, Abode {.townId = 3});
	Things().Assign<StoragePit>(pit);
	// The town names the pit as its main pit when the pit starts to work
	Things().Get<Town>(town).storagePit = pit;
	// A pit that is not built yet does not count
	Things().Assign<BuildProgress>(pit, BuildProgress {.built = 0.5f});
	EXPECT_FALSE(computer_world::WorkingPitOf(town).has_value());

	Things().Remove<BuildProgress>(pit);
	EXPECT_EQ(computer_world::WorkingPitOf(town), pit);
	// The pit keeps its place when it loses most of its life
	Things().Assign<ObjectLife>(pit, ObjectLife {.life = 0.1f});
	EXPECT_EQ(computer_world::WorkingPitOf(town), pit);
}

} // namespace
