/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Player.h"
#include "ECS/ComputerHand.h"
#include "ECS/ComputerMind.h"
#include "ECS/Registry.h"
#include "Locator.h"

// The test makes its own locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/ComputerPlayerSystem.h"

using namespace openblack;
using openblack::computer_hand::StepKind;
using openblack::ecs::components::ComputerPlayer;
using openblack::ecs::components::Player;

namespace
{

/// The script commands for the hand and the mind of a computer god, on a registry of the test's own
class ComputerPlayerScript: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto& registry = Locator::entitiesRegistry::value();
		_god = registry.Create();
		registry.Assign<Player>(_god, Player {.name = k_God});
		// The land script switches the mind on, which makes it
		_minds.SwitchOn(k_God, true);
	}
	void TearDown() override { Locator::entitiesRegistry::reset(); }

	[[nodiscard]] ComputerPlayer& Mind() const { return Locator::entitiesRegistry::value().Get<ComputerPlayer>(_god); }

	static constexpr PlayerNames k_God = PlayerNames::PLAYER_TWO;
	static constexpr glm::vec3 k_Place {1200.0f, 40.0f, 900.0f};

	ecs::systems::ComputerPlayerSystem _minds;
	entt::entity _god {entt::null};
};

TEST_F(ComputerPlayerScript, AMoveTakesTheHandFromTheMindAtTheScriptsSpeed)
{
	auto& mind = Mind();
	mind.speed = 90.0f;
	mind.switchedOn = false;
	mind.mind.paused = true;
	computer_hand::Add(mind.hand, StepKind::Wait, {});

	_minds.MoveHand(k_God, k_Place, 350.0f, false);

	// The move switches the mind on again and ends its pause
	EXPECT_TRUE(mind.switchedOn);
	EXPECT_FALSE(mind.mind.paused);
	// The move takes the place of the action that the hand did
	ASSERT_EQ(mind.hand.steps.size(), 1u);
	EXPECT_EQ(mind.hand.steps.front().kind, StepKind::MoveHandToPosDirectYInterpolation);
	EXPECT_EQ(mind.hand.steps.front().arguments.front().place, k_Place);
	EXPECT_EQ(mind.hand.speed, 350.0f);
	EXPECT_EQ(mind.speed, 90.0f);
	EXPECT_TRUE(mind.mind.scriptHand);
	EXPECT_TRUE(mind.mind.scriptHold);
	EXPECT_FALSE(_minds.Ready(k_God));
}

TEST_F(ComputerPlayerScript, AMoveAtAFixedHeightKeepsTheHandOverTheLand)
{
	_minds.MoveHand(k_God, k_Place, 160.0f, true);
	ASSERT_EQ(Mind().hand.steps.size(), 1u);
	EXPECT_EQ(Mind().hand.steps.front().kind, StepKind::MoveHandToPos);
}

TEST_F(ComputerPlayerScript, ASetPutsTheHandAtTheGodsOwnSpeedAndLeavesTheMindSwitchedOff)
{
	auto& mind = Mind();
	mind.speed = 150.0f;
	mind.switchedOn = false;
	_minds.SetHand(k_God, k_Place, false);
	ASSERT_EQ(mind.hand.steps.size(), 1u);
	EXPECT_EQ(mind.hand.steps.front().kind, StepKind::SetHandToPosDirectY);
	EXPECT_EQ(mind.hand.speed, 150.0f);
	EXPECT_FALSE(mind.switchedOn);
	EXPECT_TRUE(mind.mind.scriptHold);

	_minds.SetHand(k_God, k_Place, true);
	ASSERT_EQ(mind.hand.steps.size(), 1u);
	EXPECT_EQ(mind.hand.steps.front().kind, StepKind::SetHandToPos);
}

TEST_F(ComputerPlayerScript, TheHandPositionIsTheMapPositionOfTheHandInMetres)
{
	Mind().hand.coords = map_coords::MapCoords {.x = 0x00010000, .z = 0x00020000, .altitude = 5.0f};
	const auto position = _minds.HandPosition(k_God);
	ASSERT_TRUE(position.has_value());
	// With no land, the height is the height of the hand over the land
	EXPECT_EQ(*position, glm::vec3(10.0f, 5.0f, 20.0f));
	EXPECT_FALSE(_minds.HandPosition(PlayerNames::PLAYER_THREE).has_value());
}

TEST_F(ComputerPlayerScript, TheGodIsReadyWhenTheHandHasNoStepsLeft)
{
	EXPECT_TRUE(_minds.Ready(k_God));
	computer_hand::Add(Mind().hand, StepKind::Wait, {});
	EXPECT_FALSE(_minds.Ready(k_God));
}

TEST_F(ComputerPlayerScript, AReleaseEndsThePauseAndTheHoldAndGivesTheHandTheGodsSpeed)
{
	auto& mind = Mind();
	mind.speed = 200.0f;
	_minds.SetPaused(k_God, true);
	EXPECT_TRUE(mind.mind.paused);
	EXPECT_TRUE(mind.mind.scriptHold);
	_minds.MoveHand(k_God, k_Place, 350.0f, false);
	_minds.SetPaused(k_God, false);
	EXPECT_FALSE(mind.mind.paused);
	EXPECT_FALSE(mind.mind.scriptHold);
	// A move that the script started continues at the speed of the god
	EXPECT_EQ(mind.hand.speed, 200.0f);
	EXPECT_EQ(mind.hand.steps.size(), 1u);
}

TEST_F(ComputerPlayerScript, ASwitchOffStopsTheMindUntilTheNextMove)
{
	_minds.SetActive(k_God, false);
	EXPECT_FALSE(Mind().switchedOn);
	// The god stays a computer god
	EXPECT_TRUE(Mind().marked);
	_minds.MoveHand(k_God, k_Place, 400.0f, false);
	EXPECT_TRUE(Mind().switchedOn);
}

TEST_F(ComputerPlayerScript, TheSpeedIsTheGodsAndTheHandsSpeed)
{
	_minds.SetSpeed(k_God, 160.0f);
	EXPECT_EQ(Mind().speed, 160.0f);
	EXPECT_EQ(Mind().hand.speed, 160.0f);
}

TEST_F(ComputerPlayerScript, AClearStopsTheActionAndEmptiesTheQueue)
{
	computer_hand::Add(Mind().hand, StepKind::Wait, {});
	_minds.QueueAction(k_God, "DestroyTown", entt::null, entt::null);
	ASSERT_EQ(Mind().mind.queue.size(), 1u);
	EXPECT_EQ(Mind().mind.queue.front().name, "DestroyTown");
	_minds.ClearActions(k_God);
	EXPECT_TRUE(Mind().hand.steps.empty());
	EXPECT_TRUE(Mind().mind.queue.empty());
}

TEST_F(ComputerPlayerScript, ThePersonalityWeightGoesToEachNodeOfTheName)
{
	_minds.SetPersonality(k_God, "ExpandInfluence", 0.2f);
	const auto& nodes = Mind().mind.nodes;
	const auto changed = std::ranges::count_if(nodes, [](const auto& node) { return node.weight == 0.2f; });
	EXPECT_GE(changed, 1);
	EXPECT_LT(static_cast<size_t>(changed), nodes.size());
}

TEST_F(ComputerPlayerScript, TheAttitudeIsKeptDoubledAndReadHalved)
{
	_minds.SetAttitude(k_God, PlayerNames::PLAYER_THREE, 0.25f);
	EXPECT_EQ(_minds.Attitude(k_God, PlayerNames::PLAYER_THREE), 0.25f);
	// The other players keep the start value
	EXPECT_EQ(_minds.Attitude(k_God, PlayerNames::PLAYER_ONE), 0.5f);
}

} // namespace
