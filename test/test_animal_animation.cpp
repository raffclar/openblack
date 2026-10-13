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
#include <limits>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Animals/AnimalAnimation.h"

using namespace openblack::animals;

namespace
{
// A looping flap of ten keyframes over 1066 ms that carries its bird 2.77 of its units each play, and a held pose of
// eleven keyframes over 1100 ms
constexpr ClipTiming k_Flap {.playTime = 1066, .frameCount = 10, .looping = true, .playedByTime = false, .stride = 2.77f};
constexpr ClipTiming k_Held {.playTime = 1100, .frameCount = 11, .looping = false, .playedByTime = false, .stride = 0.0f};
// A looping walk of twelve keyframes over 1000 ms that carries its animal 1.2 of its units each play, and a clip that
// plays by the clock
constexpr ClipTiming k_Walk {.playTime = 1000, .frameCount = 12, .looping = true, .playedByTime = false, .stride = 1.2f};
constexpr ClipTiming k_Graze {.playTime = 2000, .frameCount = 20, .looping = true, .playedByTime = true, .stride = 1.2f};

// The milliseconds of clip an animal walking at about a metre a second (655 of 655.35), at a scale of 0.8, plays over a second
// of frames of so many milliseconds each, its place in the clip moving on as the game's frames move it
struct WalkedSecond
{
	int64_t played {0};
	uint32_t place {0};
};
WalkedSecond WalkASecond(uint32_t frameMilliseconds)
{
	constexpr auto k_MetreASecond = static_cast<uint16_t>(655);
	WalkedSecond walked;
	float carry = 0.0f;
	for (uint32_t time = 0; time < 1000; time += frameMilliseconds)
	{
		const auto step = FramePlay(k_Walk, true, k_MetreASecond, frameMilliseconds, 0.8f, carry);
		walked.played += step.played;
		walked.place = AdvanceClip(k_Walk, walked.place, step.played);
		carry = step.carry;
	}
	return walked;
}
} // namespace

TEST(AnimalAnimation, ALoopingClipComesRoundAndAHeldOneStopsAtItsEnd)
{
	EXPECT_EQ(AdvanceClip(k_Flap, 1000, 50), 1050u);
	EXPECT_EQ(AdvanceClip(k_Flap, 1000, 100), 34u);
	EXPECT_EQ(AdvanceClip(k_Flap, 1000, 66), 0u);
	EXPECT_EQ(AdvanceClip(k_Held, 1000, 50), 1050u);
	EXPECT_EQ(AdvanceClip(k_Held, 1000, 500), 1100u);
	EXPECT_EQ(AdvanceClip(k_Held, 1100, 16), 1100u);
}

TEST(AnimalAnimation, AMovingAnimalPlaysItsClipByTheGroundItCovers)
{
	// 10 metres a second for a tenth of a second is a metre; shrunk by a scale of 2, half a metre, of a stride of 2.77
	const auto speed = static_cast<uint16_t>(6554);
	EXPECT_EQ(MovingPlay(k_Flap, speed, 100, 2.0f), 192);
	// Twice the scale, half the play: a bigger bird flaps slower
	EXPECT_EQ(MovingPlay(k_Flap, speed, 100, 4.0f), 96);
	// A frame too short to cover enough ground plays nothing of the clip
	EXPECT_EQ(MovingPlay(k_Flap, speed, 1, 4.0f), 0);
	// A clip with no stride doesn't play by the ground
	EXPECT_EQ(MovingPlay(k_Held, speed, 100, 1.0f), 0);
}

TEST(AnimalAnimation, ACarriedPlayKeepsPaceWithTheGroundAtAnyFrameRate)
{
	// 10 metres a second, shrunk by a scale of 2, over a stride of 2.77: the clip plays 1.925 ms of each real ms
	const auto speed = static_cast<uint16_t>(6554);
	const auto oneSecond = [&](uint32_t frameMilliseconds) {
		int64_t played = 0;
		float carry = 0.0f;
		for (uint32_t time = 0; time < 1000; time += frameMilliseconds)
		{
			const auto step = MovingPlayCarried(k_Flap, speed, frameMilliseconds, 2.0f, carry);
			EXPECT_GE(step.carry, 0.0f);
			EXPECT_LT(step.carry, 1.0f);
			played += step.played;
			carry = step.carry;
		}
		return played;
	};
	const auto truncated = [&](uint32_t frameMilliseconds) {
		int64_t played = 0;
		for (uint32_t time = 0; time < 1000; time += frameMilliseconds)
		{
			played += MovingPlay(k_Flap, speed, frameMilliseconds, 2.0f);
		}
		return played;
	};
	// A second of the clip comes to the same at 1000, 250, 125 and 25 frames a second as at 10, to a millisecond
	const auto atThirty = oneSecond(33);
	for (const uint32_t frame : {1u, 4u, 8u, 40u})
	{
		EXPECT_NEAR(static_cast<double>(oneSecond(frame)), static_cast<double>(oneSecond(100)), 1.0) << frame;
	}
	// Dropping the fraction every frame instead loses ground the faster the frames: at 1000 a second the clip
	// plays a millisecond each real one, little more than half its pace
	EXPECT_EQ(truncated(1), 1000);
	EXPECT_GT(oneSecond(1), 1900);
	// At the game's own 30 frames a second the carried play is within a millisecond a frame of the dropped one
	EXPECT_NEAR(static_cast<double>(atThirty), static_cast<double>(truncated(33)), 31.0);
	// Without a stride, nothing plays and nothing carries
	const auto held = MovingPlayCarried(k_Held, speed, 100, 1.0f, 0.5f);
	EXPECT_EQ(held.played, 0);
	EXPECT_EQ(held.carry, 0.0f);
}

TEST(AnimalAnimation, AWalkingAnimalsLegsKeepPaceWithTheGroundAtAnyFrameRate)
{
	// A metre a second at a scale of 0.8 over a stride of 1.2: 1041.11 ms of clip a second, at 1000, 250, 125, 25 and
	// 10 frames a second alike, to a millisecond
	for (const uint32_t frame : {1u, 4u, 8u, 40u, 100u})
	{
		const auto walked = WalkASecond(frame);
		EXPECT_NEAR(static_cast<double>(walked.played), 1041.11, 1.0) << frame;
		EXPECT_NEAR(static_cast<double>(walked.place), 41.11, 1.0) << frame;
	}
}

TEST(AnimalAnimation, AtThirtyFramesASecondTheCarriedPlayIsTheDroppedOnesPaceKept)
{
	// At the game's own 30 frames a second each 33 ms frame covers 34.36 ms of clip: dropping the fraction plays 34 a
	// frame, carrying it plays 34 or 35 so that the clip keeps up with the ground
	constexpr auto k_MetreASecond = static_cast<uint16_t>(655);
	int64_t carried = 0;
	int64_t dropped = 0;
	float carry = 0.0f;
	for (int frame = 0; frame < 30; ++frame)
	{
		const auto step = FramePlay(k_Walk, true, k_MetreASecond, 33, 0.8f, carry);
		EXPECT_GE(step.played, 34);
		EXPECT_LE(step.played, 35);
		carried += step.played;
		carry = step.carry;
		dropped += MovingPlay(k_Walk, k_MetreASecond, 33, 0.8f);
	}
	EXPECT_EQ(dropped, 30 * 34);
	EXPECT_NEAR(static_cast<double>(carried), 30.0 * 34.36, 1.0);
}

TEST(AnimalAnimation, AnAnimalStandingOrOnAClockClipPlaysTheFramesMilliseconds)
{
	constexpr auto k_MetreASecond = static_cast<uint16_t>(655);
	// Standing still, its walk plays by the clock and the leftover waits for it to move again
	auto step = FramePlay(k_Walk, false, k_MetreASecond, 16, 0.8f, 0.25f);
	EXPECT_EQ(step.played, 16);
	EXPECT_EQ(step.carry, 0.25f);
	// Moving with a clip that plays by the clock, the same
	step = FramePlay(k_Graze, true, k_MetreASecond, 16, 0.8f, 0.25f);
	EXPECT_EQ(step.played, 16);
	EXPECT_EQ(step.carry, 0.25f);
	// Moving with its walk, the leftover is taken on: 16.66 ms of clip and a half make 17 with 0.16 left
	step = FramePlay(k_Walk, true, k_MetreASecond, 16, 0.8f, 0.5f);
	EXPECT_EQ(step.played, 17);
	EXPECT_NEAR(step.carry, 0.16f, 0.01f);
}

TEST(AnimalAnimation, TheKeyframesAreSpreadEvenlyOverThePlay)
{
	// Looping: ten keyframes over the whole play, the last blending into the first
	auto span = SpanAt(k_Flap, 0);
	EXPECT_EQ(span.from, 0u);
	EXPECT_EQ(span.to, 1u);
	EXPECT_FLOAT_EQ(span.t, 0.0f);
	span = SpanAt(k_Flap, 160);
	EXPECT_EQ(span.from, 1u);
	EXPECT_NEAR(span.t, 0.5f, 0.01f);
	span = SpanAt(k_Flap, 1065);
	EXPECT_EQ(span.from, 9u);
	EXPECT_EQ(span.to, 0u);
	// Held: from the first keyframe at the start to the last at the end
	span = SpanAt(k_Held, 1100);
	EXPECT_EQ(span.from, 10u);
	EXPECT_NEAR(span.t, 0.0f, 0.001f);
	span = SpanAt(k_Held, 55);
	EXPECT_EQ(span.from, 0u);
	EXPECT_NEAR(span.t, 0.5f, 0.001f);
}

TEST(AnimalAnimation, TheDrawClockCountsMillisecondsIntoTheTurn)
{
	EXPECT_EQ(DrawTime(10, 0.0f), 1000u);
	EXPECT_EQ(DrawTime(10, 0.456f), 1045u);
	EXPECT_EQ(DrawTime(10, 1.0f), 1099u);
}

TEST(AnimalAnimation, APoseBlendsEachBoneThenPlacesItByItsParent)
{
	constexpr auto k_Root = std::numeric_limits<uint32_t>::max();
	const std::array<uint32_t, 3> parents {k_Root, 0, 1};
	const std::array from {glm::translate(glm::mat4(1.0f), {0.0f, 1.0f, 0.0f}),
	                       glm::translate(glm::mat4(1.0f), {2.0f, 0.0f, 0.0f}), glm::mat4(1.0f)};
	const std::array to {glm::translate(glm::mat4(1.0f), {0.0f, 3.0f, 0.0f}),
	                     glm::rotate(glm::mat4(1.0f), 1.0f, glm::vec3(0.0f, 1.0f, 0.0f)),
	                     glm::scale(glm::mat4(1.0f), glm::vec3(3.0f))};
	std::array<glm::mat4, 3> bones {};
	PoseBetween(from, to, 0.25f, parents, bones);

	// Each bone's own matrix is blended element by element, not turned between its keyframes
	const auto own = [&from, &to](size_t i) { return from.at(i) + ((to.at(i) - from.at(i)) * 0.25f); };
	EXPECT_EQ(bones[0], own(0));
	EXPECT_EQ(bones[1], own(0) * own(1));
	EXPECT_EQ(bones[2], own(0) * own(1) * own(2));
}
