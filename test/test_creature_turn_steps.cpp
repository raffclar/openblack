/*******************************************************************************
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

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/CreatureTurnSteps.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
constexpr size_t k_Walk = 7;
constexpr uint32_t k_WalkMs = 1000;

/// A walk that loops each second, with a footstep at 50 ms and a growl at 500 ms
const std::array k_WalkEvents {
    creature_audio::SoundEvent {
        .kind = creature_audio::EventKind::Generic, .timeMs = 50, .action = audio::SoundAction::FootstepNormal, .mode = 0},
    creature_audio::SoundEvent {
        .kind = creature_audio::EventKind::Voice, .timeMs = 500, .action = audio::SoundAction::None, .mode = 0},
};

std::optional<creature_audio::AnimationInfo> WalkInfo(size_t animation)
{
	if (animation != k_Walk)
	{
		return std::nullopt;
	}
	return creature_audio::AnimationInfo {.events = k_WalkEvents, .durationMs = k_WalkMs, .looping = true};
}

/// The layers of a body that walks, at this time of the walk
creature_audio::Layers WalkAt(float timeMs)
{
	return creature_audio::Layers {.slots = {{.animation = k_Walk, .timeMs = timeMs}}, .soundingSlot = 0};
}

glm::mat4 FootAt(glm::vec3 point)
{
	return glm::translate(glm::mat4(1.0f), point);
}

creature_footprints::Feet FeetAt(glm::vec3 right, glm::vec3 left)
{
	return {.right = FootAt(right), .left = FootAt(left)};
}

creature_audio::FiredEvent Fired(audio::SoundAction action)
{
	return {.fraction = 0.5f, .event = {.kind = creature_audio::EventKind::Generic, .timeMs = 0, .action = action, .mode = 0}};
}
} // namespace

TEST(CreatureTurnSteps, AnEventThatIsNotAFootstepDoesNotScare)
{
	const std::vector fired {Fired(audio::SoundAction::None)};
	EXPECT_FALSE(creature_turn_steps::ScarePoint(fired, FeetAt({1, 0, 1}, {2, 0, 2})).has_value());
	EXPECT_FALSE(creature_turn_steps::ScarePoint({}, FeetAt({1, 0, 1}, {2, 0, 2})).has_value());
}

TEST(CreatureTurnSteps, AFootstepInTheWaterScaresAtTheLowerFoot)
{
	const std::vector fired {Fired(audio::SoundAction::FootstepLight)};
	EXPECT_EQ(creature_turn_steps::ScarePoint(fired, FeetAt({1, 0.2f, 1}, {2, 0.5f, 2})), glm::vec3(1, 0.2f, 1));
	EXPECT_EQ(creature_turn_steps::ScarePoint(fired, FeetAt({1, 0.5f, 1}, {2, 0.2f, 2})), glm::vec3(2, 0.2f, 2));
}

TEST(CreatureTurnSteps, TheRightFootTakesTheStepWhenTheFeetAreLevel)
{
	const std::vector fired {Fired(audio::SoundAction::FootStamp)};
	EXPECT_EQ(creature_turn_steps::ScarePoint(fired, FeetAt({1, 0.5f, 1}, {2, 0.5f, 2})), glm::vec3(1, 0.5f, 1));
}

TEST(CreatureTurnSteps, AFootstepOnTheLandDoesNotScare)
{
	const std::vector fired {Fired(audio::SoundAction::FootstepNormal)};
	EXPECT_FALSE(creature_turn_steps::ScarePoint(fired, FeetAt({1, 1.0f, 1}, {2, 3.0f, 2})).has_value());
	EXPECT_TRUE(creature_turn_steps::ScarePoint(fired, FeetAt({1, 0.99f, 1}, {2, 3.0f, 2})).has_value());
}

TEST(CreatureTurnSteps, TheTurnThatPassesAFootstepScaresTheFish)
{
	auto last = WalkAt(0.0f);
	const auto feet = FeetAt({10, 0, 20}, {12, 0.5f, 20});
	EXPECT_EQ(creature_turn_steps::StepScare(last, WalkAt(100.0f), 100.0f, WalkInfo, feet), glm::vec3(10, 0, 20));
	// The next turn does not pass the footstep again
	EXPECT_FALSE(creature_turn_steps::StepScare(last, WalkAt(200.0f), 100.0f, WalkInfo, feet).has_value());
	// The growl is not a footstep
	auto growl = WalkAt(450.0f);
	EXPECT_FALSE(creature_turn_steps::StepScare(growl, WalkAt(550.0f), 100.0f, WalkInfo, feet).has_value());
}

TEST(CreatureTurnSteps, AWalkThatLoopsRoundPassesItsFootstepAgain)
{
	auto last = WalkAt(950.0f);
	const auto feet = FeetAt({10, 0, 20}, {12, 0.5f, 20});
	EXPECT_TRUE(creature_turn_steps::StepScare(last, WalkAt(60.0f), 100.0f, WalkInfo, feet).has_value());
}

TEST(CreatureTurnSteps, ABodyWithNoFeetKeepsItsLayersAndDoesNotScare)
{
	auto last = WalkAt(0.0f);
	EXPECT_FALSE(creature_turn_steps::StepScare(last, WalkAt(100.0f), 100.0f, WalkInfo, std::nullopt).has_value());
	ASSERT_EQ(last.slots.size(), 1u);
	EXPECT_EQ(last.slots.front().timeMs, 100.0f);
}

TEST(CreatureTurnSteps, ATurnInWhichNoTimePassesMakesNoStep)
{
	auto last = WalkAt(0.0f);
	const auto feet = FeetAt({10, 0, 20}, {12, 0.5f, 20});
	EXPECT_FALSE(creature_turn_steps::StepScare(last, WalkAt(100.0f), 0.0f, WalkInfo, feet).has_value());
	// The layers stay as they were, so the next turn passes the footstep
	EXPECT_TRUE(creature_turn_steps::StepScare(last, WalkAt(100.0f), 100.0f, WalkInfo, feet).has_value());
}

TEST(CreatureTurnSteps, TheTurnLayersComeFromTheTurnAndNotFromTheDrawnWalk)
{
	ecs::components::CreatureAnimation animation;
	animation.slots = {{.animation = k_Walk, .timeMs = 40.0f, .weight = 1.0f, .mirrored = false}};
	animation.drawnPlace = ecs::components::CreatureAnimation::DrawnPlace {
	    .position = {},
	    .rotation = glm::mat3(1.0f),
	    .slots = {{.animation = k_Walk, .timeMs = 90.0f, .weight = 1.0f, .mirrored = false}},
	};
	const auto layers = creature_turn_steps::TurnLayersOf(animation);
	ASSERT_EQ(layers.slots.size(), 1u);
	EXPECT_EQ(layers.slots.front().timeMs, 40.0f);
	EXPECT_EQ(layers.soundingSlot, std::optional<size_t>(0));
}
