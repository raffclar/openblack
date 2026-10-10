/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <deque>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "ECS/VillagerEyes.h"

using namespace openblack::ecs::villager_eyes;

namespace
{
/// Gives the numbers it is told to, in order, and remembers every range it was asked for
struct FakeRandom
{
	std::deque<float> answers;
	std::vector<std::pair<float, float>> asked;

	float operator()(float a, float b)
	{
		asked.emplace_back(a, b);
		if (answers.empty())
		{
			return a;
		}
		const float answer = answers.front();
		answers.pop_front();
		return answer;
	}
};

/// Eyes in the middle of a blink, still turning towards where every eye turns, so that only the blink draws numbers
Eyes Blinking(int32_t into, int32_t closedFor)
{
	return Eyes {.blink = {.blinking = true, .untilNext = 1000, .into = into, .closedFor = closedFor}, .roll = 0.0f};
}

constexpr Shared k_Still {.rollTarget = 0.1f, .droopTime = 0, .droop = 0.0f};

void ExpectNear(const glm::mat4& a, const glm::mat4& b)
{
	for (int column = 0; column < 4; ++column)
	{
		for (int row = 0; row < 4; ++row)
		{
			EXPECT_NEAR(a[column][row], b[column][row], 1e-6f) << "column " << column << " row " << row;
		}
	}
}
} // namespace

TEST(VillagerEyes, TheFirstBlinkStartsOnTheFirstFrameAndPicksTheNextGap)
{
	Eyes eyes {};
	Shared shared {};
	FakeRandom random {.answers = {3200.7f, 0.2f}, .asked = {}};
	const float closed = Step(eyes, shared, 16, random);
	EXPECT_TRUE(eyes.blink.blinking);
	// The time until the next blink is cut to whole milliseconds
	EXPECT_EQ(eyes.blink.untilNext, 3200);
	// The lids start closing only on the next frame
	EXPECT_FLOAT_EQ(closed, 0.0f);
	EXPECT_EQ(eyes.blink.into, 0);
	ASSERT_EQ(random.asked.size(), 2u);
	EXPECT_EQ(random.asked[0], std::make_pair(1000.0f, 5000.0f));
	// The roll was where every eye turns (both start at 0), so it picks another way
	EXPECT_EQ(random.asked[1], std::make_pair(-0.25f, 0.25f));
	EXPECT_FLOAT_EQ(shared.rollTarget, 0.2f);
	EXPECT_FLOAT_EQ(eyes.roll, 0.0f);
}

TEST(VillagerEyes, BetweenBlinksTheGapCountsDown)
{
	Eyes eyes {.blink = {.blinking = false, .untilNext = 40, .into = 0, .closedFor = 150}, .roll = 0.0f};
	auto shared = k_Still;
	FakeRandom random {};
	EXPECT_FLOAT_EQ(Step(eyes, shared, 40, random), 0.0f);
	// Down to 0 isn't yet under it
	EXPECT_FALSE(eyes.blink.blinking);
	EXPECT_EQ(eyes.blink.untilNext, 0);
	EXPECT_TRUE(random.asked.empty());
	random.answers = {1000.0f};
	EXPECT_FLOAT_EQ(Step(eyes, shared, 1, random), 0.0f);
	EXPECT_TRUE(eyes.blink.blinking);
}

TEST(VillagerEyes, TheLidsCloseOver50MillisecondsHoldShutThenOpen)
{
	FakeRandom random {};
	// Each frame's closure is by the time into the blink before the frame is added
	const auto closedAt = [&random](int32_t into) {
		auto eyes = Blinking(into, 200);
		auto shared = k_Still;
		const float closed = Step(eyes, shared, 10, random);
		EXPECT_EQ(eyes.blink.into, into + 10);
		return closed;
	};
	EXPECT_FLOAT_EQ(closedAt(0), 0.0f);
	EXPECT_FLOAT_EQ(closedAt(25), 25.0f * 0.02f);
	EXPECT_FLOAT_EQ(closedAt(49), 49.0f * 0.02f);
	EXPECT_FLOAT_EQ(closedAt(50), 1.0f);
	EXPECT_FLOAT_EQ(closedAt(249), 1.0f);
	// Shut for its while, then opening again over 50 milliseconds
	EXPECT_FLOAT_EQ(closedAt(250), 1.0f);
	EXPECT_FLOAT_EQ(closedAt(275), 1.0f - (25.0f * 0.02f));
	EXPECT_NEAR(closedAt(300), 0.0f, 1e-6f);
	EXPECT_TRUE(random.asked.empty());
}

TEST(VillagerEyes, ABlinkEndsPastItsOpeningAndPicksTheNextHold)
{
	auto eyes = Blinking(301, 200);
	auto shared = k_Still;
	FakeRandom random {.answers = {137.9f}, .asked = {}};
	EXPECT_FLOAT_EQ(Step(eyes, shared, 16, random), 0.0f);
	EXPECT_FALSE(eyes.blink.blinking);
	EXPECT_EQ(eyes.blink.into, 0);
	EXPECT_EQ(eyes.blink.closedFor, 137);
	ASSERT_EQ(random.asked.size(), 1u);
	EXPECT_EQ(random.asked[0], std::make_pair(100.0f, 200.0f));
	// The gap to the next blink counts only from the frame after
	EXPECT_EQ(eyes.blink.untilNext, 1000);
}

TEST(VillagerEyes, TheFirstBlinkHolds200Milliseconds)
{
	EXPECT_EQ(Eyes {}.blink.closedFor, 200);
	EXPECT_EQ(Eyes {}.blink.untilNext, 0);
}

TEST(VillagerEyes, TheEyesTurnSteadilyTowardsWhereEveryEyeTurns)
{
	FakeRandom random {};
	Eyes eyes {.blink = {.blinking = false, .untilNext = 5000, .into = 0, .closedFor = 200}, .roll = 0.0f};
	Shared shared {.rollTarget = 0.25f, .droopTime = 0, .droop = 0.0f};
	// 0.7 thousandths of a radian a millisecond
	static_cast<void>(Step(eyes, shared, 100, random));
	EXPECT_NEAR(eyes.roll, 0.07f, 1e-6f);
	static_cast<void>(Step(eyes, shared, 1000, random));
	// Never past where they turn to
	EXPECT_FLOAT_EQ(eyes.roll, 0.25f);
	// Only the lids' droop was picked again, its 300 milliseconds having passed
	ASSERT_EQ(random.asked.size(), 1u);
	EXPECT_EQ(random.asked[0], std::make_pair(0.0f, 0.3f));
	random.asked.clear();
	// And back the other way
	shared.rollTarget = 0.2f;
	static_cast<void>(Step(eyes, shared, 50, random));
	EXPECT_NEAR(eyes.roll, 0.215f, 1e-6f);
	static_cast<void>(Step(eyes, shared, 50, random));
	EXPECT_FLOAT_EQ(eyes.roll, 0.2f);
	// Once there, the next frame picks another way, for every villager's eyes
	random.answers = {-0.1f};
	static_cast<void>(Step(eyes, shared, 50, random));
	EXPECT_FLOAT_EQ(eyes.roll, 0.2f);
	EXPECT_FLOAT_EQ(shared.rollTarget, -0.1f);
}

TEST(VillagerEyes, TheLidsDroopAtLeastBySharePickedEvery300Milliseconds)
{
	FakeRandom random {.answers = {0.25f}, .asked = {}};
	Eyes eyes {.blink = {.blinking = false, .untilNext = 5000, .into = 0, .closedFor = 200}, .roll = 0.0f};
	Shared shared {.rollTarget = 0.1f, .droopTime = 290, .droop = 0.05f};
	EXPECT_FLOAT_EQ(Step(eyes, shared, 10, random), 0.05f);
	EXPECT_EQ(shared.droopTime, 300u);
	EXPECT_TRUE(random.asked.empty());
	EXPECT_FLOAT_EQ(Step(eyes, shared, 1, random), 0.25f);
	EXPECT_EQ(shared.droopTime, 0u);
	ASSERT_EQ(random.asked.size(), 1u);
	EXPECT_EQ(random.asked[0], std::make_pair(0.0f, 0.3f));
	// A blink closes the lids further than the droop
	auto blinking = Blinking(100, 200);
	EXPECT_FLOAT_EQ(Step(blinking, shared, 10, random), 1.0f);
}

TEST(VillagerEyes, AnEyeFacesOutOfItsFrameMirrored)
{
	const auto eye = EyeFrame(glm::mat4(1.0f), glm::mat4(1.0f));
	// Its sideways and up axes are kept, and its front and back swap, with the float skew of a half turn
	EXPECT_NEAR(eye[0].x, 1.0f, 1e-7f);
	EXPECT_NEAR(eye[0].z, 8.742278e-8f, 1e-12f);
	EXPECT_EQ(eye[1], glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
	EXPECT_NEAR(eye[2].x, 8.742278e-8f, 1e-12f);
	EXPECT_FLOAT_EQ(eye[2].z, -1.0f);
	EXPECT_EQ(eye[3], glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	EXPECT_LT(glm::determinant(glm::mat3(eye)), 0.0f);
}

TEST(VillagerEyes, AnEyeSitsOnItsFrameOnTheBoneInTheWorld)
{
	// A bone turned a quarter about y and moved, and an eye frame further along the bone
	const auto bone = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 2.0f, 3.0f)) *
	                  glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const auto frame = glm::translate(glm::mat4(1.0f), glm::vec3(0.17f, 0.18f, -0.05f));
	const auto eye = EyeFrame(bone, frame);
	const auto expected = bone * frame;
	EXPECT_NEAR(eye[3].x, expected[3].x, 1e-5f);
	EXPECT_NEAR(eye[3].y, expected[3].y, 1e-5f);
	EXPECT_NEAR(eye[3].z, expected[3].z, 1e-5f);
	// A point on the eye's front comes out behind it
	const auto front = eye * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
	const auto behind = expected * glm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
	EXPECT_NEAR(front.x, behind.x, 1e-5f);
	EXPECT_NEAR(front.z, behind.z, 1e-5f);
}

TEST(VillagerEyes, TheEyeballTurnsAboutTheEyesUp)
{
	const auto eye = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
	ExpectNear(Rolled(eye, 0.0f), eye);
	const auto rolled = Rolled(eye, 0.25f);
	const float c = std::cos(0.25f);
	const float s = std::sin(0.25f);
	EXPECT_FLOAT_EQ(rolled[0].x, c);
	EXPECT_FLOAT_EQ(rolled[0].z, s);
	EXPECT_FLOAT_EQ(rolled[2].x, -s);
	EXPECT_FLOAT_EQ(rolled[2].z, c);
	EXPECT_EQ(rolled[1], eye[1]);
	EXPECT_EQ(rolled[3], eye[3]);
}

TEST(VillagerEyes, TheLidsTurnShutAboutTheEyesSide)
{
	const auto eye = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
	// Open, a lid sits as the eye
	ExpectNear(Lid(eye, 0.0f, true), eye);
	ExpectNear(Lid(eye, 0.0f, false), eye);
	// Shut, the upper lid turns by 0.47 radians and the lower by -0.35
	for (const auto& [upper, angle] : {std::pair {true, 0.47f}, std::pair {false, -0.35f}})
	{
		const auto lid = Lid(eye, 1.0f, upper);
		EXPECT_FLOAT_EQ(lid[1].y, std::cos(angle));
		EXPECT_FLOAT_EQ(lid[1].z, -std::sin(angle));
		EXPECT_FLOAT_EQ(lid[2].y, std::sin(angle));
		EXPECT_FLOAT_EQ(lid[2].z, std::cos(angle));
		EXPECT_EQ(lid[0], eye[0]);
		EXPECT_EQ(lid[3], eye[3]);
	}
	// Half closed, half the angle
	EXPECT_FLOAT_EQ(Lid(eye, 0.5f, true)[1].y, std::cos(0.235f));
}

TEST(VillagerEyes, AnEyeIsShadedByHowItFacesTheSun)
{
	// The man's right eye looks straight out of its frame: an unturned eye looks along -z, a third of the way to the sun
	// (rounded 147 of 255), which shades it 90 + 165 * 147 / 256
	EXPECT_EQ(Shade(Face::Man, Side::Right, glm::mat4(1.0f)), 90 + ((165 * 147) >> 8));
	// Turned from the sun it keeps the dark side's shade
	const auto away = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, -1.0f));
	EXPECT_EQ(Shade(Face::Man, Side::Right, away), k_DarkSide);
	// The scale of its frame doesn't matter
	EXPECT_EQ(Shade(Face::Man, Side::Right, glm::scale(glm::mat4(1.0f), glm::vec3(0.6f))), 90 + ((165 * 147) >> 8));
	// Each eye of each face looks a little off its front, so the two eyes of one face differ
	EXPECT_NE(Shade(Face::Boy, Side::Right, glm::mat4(1.0f)), Shade(Face::Boy, Side::Left, glm::mat4(1.0f)));
}

TEST(VillagerEyes, EachFaceHasItsOwnIris)
{
	EXPECT_EQ(IrisOffset(Face::Man), glm::vec2(0.5f, 0.75f));
	EXPECT_EQ(IrisOffset(Face::Woman), glm::vec2(0.375f, 0.75f));
	EXPECT_EQ(IrisOffset(Face::Boy), glm::vec2(0.625f, 0.75f));
}

TEST(VillagerEyes, TheBoysEyesAreTheLargerSet)
{
	EXPECT_EQ(PieceFile(Face::Man, Piece::Eyeball), "Eyes/eye_ball.l3d");
	EXPECT_EQ(PieceFile(Face::Woman, Piece::LeftLowerLid), "Eyes/l_paupe_down.l3d");
	EXPECT_EQ(PieceFile(Face::Boy, Piece::Eyeball), "Eyes/eye_ball2.l3d");
	EXPECT_EQ(PieceFile(Face::Boy, Piece::RightUpperLid), "Eyes/r_paupe_up2.l3d");
}

TEST(VillagerEyes, PlacingGivesEveryPieceAndTheVillagersStandingPoint)
{
	const auto model = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 5.0f, 200.0f));
	const std::array bones {glm::mat4(1.0f), glm::mat4(1.0f)};
	const std::array frames {glm::translate(glm::mat4(1.0f), glm::vec3(0.1f, 1.6f, -0.05f)),
	                         glm::translate(glm::mat4(1.0f), glm::vec3(0.1f, 1.6f, 0.05f))};
	const auto drawn = Place(Face::Woman, model, bones, frames, 0.2f, 0.5f);
	EXPECT_EQ(drawn.standing, glm::vec3(100.0f, 5.0f, 200.0f));
	EXPECT_FLOAT_EQ(drawn.closed, 0.5f);
	for (const auto side : {Side::Right, Side::Left})
	{
		const auto index = static_cast<size_t>(side);
		const auto eye = EyeFrame(model * bones.at(index), frames.at(index));
		const auto& placed = drawn.eyes.at(index);
		ExpectNear(placed.eyeball, Rolled(eye, 0.2f));
		// The lids don't turn with the eyeball
		ExpectNear(placed.upperLid, Lid(eye, 0.5f, true));
		ExpectNear(placed.lowerLid, Lid(eye, 0.5f, false));
		EXPECT_EQ(placed.shade, Shade(Face::Woman, side, eye));
	}
}
