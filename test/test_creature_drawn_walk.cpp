/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureFight.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureDrawnWalk.h"

using namespace openblack;
using namespace openblack::ecs;
using components::CreatureAnimation;
using components::CreatureLocomotion;
using components::Transform;

namespace
{

constexpr float k_TurnMs = 100.0f;
/// The frame times that the frames of a turn take: a fast screen, the screen's refresh, and two slow frame rates
constexpr std::array k_FrameMs {7.0f, 16.7f, 33.0f, 50.0f};

/// A creature that walks four units ahead and turns a little in its turn, with its legs part of the way through a walk
CreatureLocomotion Walking()
{
	CreatureLocomotion walk;
	walk.started = true;
	walk.fromPosition = glm::vec3(10.0f, 2.0f, 20.0f);
	walk.toPosition = glm::vec3(10.0f, 2.5f, 16.0f);
	walk.fromHeading = GameHeading(0.5f);
	walk.toHeading = GameHeading(0.75f);
	walk.tracks.push_back({.animation = 7,
	                       .fromMs = 250.0f,
	                       .advanceMs = 80.0f,
	                       .durationMs = 800.0f,
	                       .weight = 1.0f,
	                       .looping = true,
	                       .breathing = false});
	walk.tracks.push_back({.animation = 9,
	                       .fromMs = 780.0f,
	                       .advanceMs = 80.0f,
	                       .durationMs = 800.0f,
	                       .weight = 0.5f,
	                       .looping = true,
	                       .breathing = false});
	return walk;
}

/// The part of the turn that each frame of one turn reaches, at a frame time
std::vector<float> FractionsOfOneTurn(float frameMs)
{
	std::vector<float> fractions;
	for (float ms = frameMs; ms < k_TurnMs; ms += frameMs)
	{
		fractions.push_back(ms / k_TurnMs);
	}
	return fractions;
}

} // namespace

TEST(CreatureDrawnWalk, TheDrawnWalkStartsAtTheLastTurnAndEndsAtThisTurn)
{
	const auto walk = Walking();
	const auto start = creature_drawn_walk::PlaceAt(walk, 0.0f, 0.0f);
	const auto end = creature_drawn_walk::PlaceAt(walk, 0.0f, 1.0f);
	EXPECT_EQ(start.position, walk.fromPosition);
	EXPECT_EQ(end.position, walk.toPosition);
	ASSERT_EQ(end.slots.size(), 2u);
	EXPECT_EQ(end.slots[0].timeMs, 330.0f);
	// A looping walk wraps round at the end of its animation
	EXPECT_EQ(end.slots[1].timeMs, 60.0f);
	EXPECT_EQ(end.slots[1].weight, 0.5f);
}

TEST(CreatureDrawnWalk, AnimationsThatDoNotLoopStopBeforeTheirEnd)
{
	auto walk = Walking();
	walk.tracks.resize(1);
	walk.tracks[0].looping = false;
	walk.tracks[0].fromMs = 790.0f;
	const auto end = creature_drawn_walk::PlaceAt(walk, 0.0f, 1.0f);
	EXPECT_EQ(end.slots[0].timeMs, 799.0f);
}

TEST(CreatureDrawnWalk, AnyFrameRateDrawsTheSameTurnEnd)
{
	const auto walk = Walking();
	const auto turnEnd = creature_drawn_walk::PlaceAt(walk, 0.0f, 1.0f);
	for (const auto frameMs : k_FrameMs)
	{
		std::optional<CreatureAnimation::DrawnPlace> drawn;
		for (const auto fraction : FractionsOfOneTurn(frameMs))
		{
			drawn = creature_drawn_walk::PlaceAt(walk, 0.0f, fraction);
		}
		// The frames move only the drawn place, and the turn's end is the same place at every frame rate
		ASSERT_TRUE(drawn.has_value());
		EXPECT_TRUE(creature_drawn_walk::Changed(drawn, turnEnd)) << frameMs;
		const auto end = creature_drawn_walk::PlaceAt(walk, 0.0f, 1.0f);
		EXPECT_FALSE(creature_drawn_walk::Changed(end, turnEnd)) << frameMs;
	}
}

TEST(CreatureDrawnWalk, ChangedSeesEachPartOfThePlace)
{
	const auto walk = Walking();
	const auto place = creature_drawn_walk::PlaceAt(walk, 0.0f, 0.5f);
	EXPECT_TRUE(creature_drawn_walk::Changed(std::nullopt, place));
	EXPECT_FALSE(creature_drawn_walk::Changed(place, place));
	auto moved = place;
	moved.position.x += 1.0f;
	EXPECT_TRUE(creature_drawn_walk::Changed(moved, place));
	auto legs = place;
	legs.slots[1].timeMs += 1.0f;
	EXPECT_TRUE(creature_drawn_walk::Changed(legs, place));
}

TEST(CreatureDrawnWalk, TheDrawnTransformIsTheDrawnPlaceWhenThereIsOne)
{
	const Transform transform {.position = glm::vec3(1.0f, 2.0f, 3.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(2.0f)};
	CreatureAnimation animation;
	animation.slots.push_back({.animation = 3, .timeMs = 10.0f, .weight = 1.0f, .mirrored = false});
	EXPECT_EQ(components::DrawnTransform(transform, nullptr).position, transform.position);
	EXPECT_EQ(components::DrawnTransform(transform, &animation).position, transform.position);
	EXPECT_EQ(components::DrawnSlots(animation).front().animation, 3u);

	animation.drawnPlace = creature_drawn_walk::PlaceAt(Walking(), 0.0f, 0.25f);
	const auto drawn = components::DrawnTransform(transform, &animation);
	EXPECT_EQ(drawn.position, animation.drawnPlace->position);
	EXPECT_EQ(drawn.rotation, animation.drawnPlace->rotation);
	// The scale is the creature's own
	EXPECT_EQ(drawn.scale, transform.scale);
	EXPECT_EQ(components::DrawnSlots(animation).front().animation, 7u);
	// The turn's slots stay as the turn left them
	EXPECT_EQ(animation.slots.front().animation, 3u);
}

TEST(CreatureDrawnWalk, AFightActionEndsAtTheSameTurnAtAnyFrameRate)
{
	// A fight's animation moves on only in the game turns, by the turn's whole milliseconds. The frames of a turn only
	// draw the body, so the turn at which the action ends is the same at every frame rate.
	constexpr float k_DurationMs = 1200.0f;
	const auto speed = creature_fight::PlaybackSpeed(creature_fight::animations::k_Stance, 1.0f);
	std::optional<int> referenceTurn;
	for (const auto frameMs : k_FrameMs)
	{
		float timeMs = 0.0f;
		std::optional<int> endTurn;
		for (int turn = 1; turn <= 100 && !endTurn.has_value(); ++turn)
		{
			// The frames of the turn read the time and change nothing
			for (const auto fraction : FractionsOfOneTurn(frameMs))
			{
				[[maybe_unused]] const auto drawnMs = timeMs + (fraction * k_TurnMs);
			}
			const auto advance = creature_fight::TurnAdvance(speed, static_cast<int32_t>(k_TurnMs));
			if (creature_fight::ActionEnds(timeMs, advance, k_DurationMs))
			{
				endTurn = turn;
			}
			timeMs += static_cast<float>(advance);
		}
		ASSERT_TRUE(endTurn.has_value());
		if (!referenceTurn.has_value())
		{
			referenceTurn = endTurn;
		}
		EXPECT_EQ(endTurn, referenceTurn) << frameMs;
	}
}
