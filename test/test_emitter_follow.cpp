/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/SoundPoint.h"
#include "Enums.h"
#include "Sound/EmitterFollow.h"

using namespace openblack;
using audio::Follows;
using audio::StepFollowingSound;

namespace
{
constexpr float k_MaxDistance = 400.0f;
const glm::vec3 k_Listener {0.0f, 0.0f, 0.0f};

/// A thing at a place in the world
entt::entity ThingAt(ecs::Registry& registry, const glm::vec3& position)
{
	const auto thing = registry.Create();
	registry.Assign<ecs::components::Transform>(thing, ecs::components::Transform {.position = position});
	return thing;
}
} // namespace

// A sound that does not follow stays and plays to its end, however far the camera is
TEST(EmitterFollow, ASoundThatDoesNotFollowStaysAndPlays)
{
	const auto step = StepFollowingSound(Follows::Nothing, std::nullopt, {1000.0f, 0.0f, 0.0f}, k_Listener, k_MaxDistance);
	EXPECT_FALSE(step.stop);
	EXPECT_FALSE(step.moveTo.has_value());
}

// A sound moves to the point of its owner
TEST(EmitterFollow, ASoundMovesToItsOwner)
{
	const auto step =
	    StepFollowingSound(Follows::Owner, glm::vec3(30.0f, 2.0f, -5.0f), {20.0f, 0.0f, 0.0f}, k_Listener, k_MaxDistance);
	EXPECT_FALSE(step.stop);
	ASSERT_TRUE(step.moveTo.has_value());
	EXPECT_EQ(*step.moveTo, glm::vec3(30.0f, 2.0f, -5.0f));
}

// A sound whose owner did not move stays where it is
TEST(EmitterFollow, ASoundWhoseOwnerStaysDoesNotMove)
{
	const auto step =
	    StepFollowingSound(Follows::Owner, glm::vec3(20.0f, 0.0f, 0.0f), {20.0f, 0.0f, 0.0f}, k_Listener, k_MaxDistance);
	EXPECT_FALSE(step.stop);
	EXPECT_FALSE(step.moveTo.has_value());
}

// A sound stops at once when its owner is gone or has no point
TEST(EmitterFollow, ASoundStopsWhenItsOwnerHasNoPoint)
{
	EXPECT_TRUE(StepFollowingSound(Follows::Owner, std::nullopt, {20.0f, 0.0f, 0.0f}, k_Listener, k_MaxDistance).stop);
}

// A sound stops when its owner takes it to or past its maximum distance from the camera
TEST(EmitterFollow, ASoundStopsAtItsMaximumDistance)
{
	EXPECT_TRUE(
	    StepFollowingSound(Follows::Owner, glm::vec3(400.0f, 0.0f, 0.0f), {20.0f, 0.0f, 0.0f}, k_Listener, k_MaxDistance).stop);
	EXPECT_FALSE(
	    StepFollowingSound(Follows::Owner, glm::vec3(399.0f, 0.0f, 0.0f), {20.0f, 0.0f, 0.0f}, k_Listener, k_MaxDistance).stop);
}

// A sound with no owner that follows goes to the listener, so it never gets too far
TEST(EmitterFollow, ASoundWithNoOwnerFollowsTheListener)
{
	const glm::vec3 listener {900.0f, 50.0f, 900.0f};
	const auto step = StepFollowingSound(Follows::Listener, std::nullopt, {0.0f, 0.0f, 0.0f}, listener, k_MaxDistance);
	EXPECT_FALSE(step.stop);
	ASSERT_TRUE(step.moveTo.has_value());
	EXPECT_EQ(*step.moveTo, listener);
}

// A sound at a place stays there, and stops when the camera goes too far
TEST(EmitterFollow, ASoundAtAPlaceStopsWhenTheCameraGoesTooFar)
{
	const auto near = StepFollowingSound(Follows::Place, std::nullopt, {100.0f, 0.0f, 0.0f}, k_Listener, k_MaxDistance);
	EXPECT_FALSE(near.stop);
	EXPECT_FALSE(near.moveTo.has_value());
	EXPECT_TRUE(StepFollowingSound(Follows::Place, std::nullopt, {500.0f, 0.0f, 0.0f}, k_Listener, k_MaxDistance).stop);
}

// The sounds of a thing come from its position
TEST(SoundPoint, AThingSoundsFromItsPosition)
{
	ecs::Registry registry;
	const auto thing = ThingAt(registry, {10.0f, 3.0f, 20.0f});
	EXPECT_EQ(ecs::SoundPointOf(registry, thing), glm::vec3(10.0f, 3.0f, 20.0f));
}

// A thing in a hand sounds from the hand
TEST(SoundPoint, AThingInAHandSoundsFromTheHand)
{
	ecs::Registry registry;
	const auto thing = ThingAt(registry, {10.0f, 3.0f, 20.0f});
	const auto hand = ThingAt(registry, {40.0f, 12.0f, 0.0f});
	registry.Assign<ecs::components::InHand>(thing, ecs::components::InHand {.hand = hand});
	EXPECT_EQ(ecs::SoundPointOf(registry, thing), glm::vec3(40.0f, 12.0f, 0.0f));
}

// A thing that is gone, or that has no position, gives no point
TEST(SoundPoint, AGoneThingOrAThingWithNoPositionGivesNoPoint)
{
	ecs::Registry registry;
	const auto gone = ThingAt(registry, {10.0f, 3.0f, 20.0f});
	registry.Destroy(gone);
	EXPECT_FALSE(ecs::SoundPointOf(registry, gone).has_value());
	EXPECT_FALSE(ecs::SoundPointOf(registry, entt::null).has_value());
	const auto player = registry.Create();
	EXPECT_FALSE(ecs::SoundPointOf(registry, player).has_value());
}

// A villager that dies is not available, so its sounds stop
TEST(SoundPoint, ADyingVillagerGivesNoPoint)
{
	ecs::Registry registry;
	const auto villager = ThingAt(registry, {10.0f, 3.0f, 20.0f});
	registry.Assign<ecs::components::Villager>(villager);
	registry.Assign<ecs::components::LivingAction>(villager, VillagerStates::DecideWhatToDo, uint16_t {0});
	EXPECT_TRUE(ecs::SoundPointOf(registry, villager).has_value());

	auto& action = registry.Get<ecs::components::LivingAction>(villager);
	action.states.at(static_cast<size_t>(ecs::components::LivingAction::Index::Final)) =
	    static_cast<uint8_t>(VillagerStates::Dying);
	EXPECT_FALSE(ecs::SoundPointOf(registry, villager).has_value());
}
