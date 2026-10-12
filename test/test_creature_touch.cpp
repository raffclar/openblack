/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <chrono>
#include <optional>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "3D/HandAnimation.h"
#include "Camera/Camera.h"
#include "Camera/CreatureInteractCameraModel.h"
#include "Common/Zoomer.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureHandRules.h"
#include "Creature/CreatureLocomotion.h"

using namespace openblack;
namespace feedback = openblack::creature_feedback;

namespace
{
constexpr float k_Tolerance = 1e-3f;
constexpr float k_Height = 15.0f;
} // namespace

TEST(CreatureTouch, StepsComeAtMostSixtyTimesASecondTakingAllTheTimeSinceTheLast)
{
	float since = 0.0f;
	// Fast frames step only once a sixtieth of a second has gone by, with all of it
	for (int i = 0; i < 10; ++i)
	{
		EXPECT_FALSE(feedback::Pace(since, 1.6f).has_value());
	}
	const auto step = feedback::Pace(since, 2.0f);
	ASSERT_TRUE(step.has_value());
	EXPECT_NEAR(*step, 18.0f, k_Tolerance);
	// A slow frame steps once with all its time
	const auto slow = feedback::Pace(since, 40.0f);
	ASSERT_TRUE(slow.has_value());
	EXPECT_NEAR(*slow, 40.0f, k_Tolerance);
}

TEST(CreatureTouch, StrokesWaitLongerTheLessTheCreatureHasBeenRewarded)
{
	EXPECT_NEAR(feedback::StrokeWaitMs(0.0f), 1250.0f, k_Tolerance);
	EXPECT_NEAR(feedback::StrokeWaitMs(-1.0f), 2500.0f, k_Tolerance);
	EXPECT_NEAR(feedback::StrokeWaitMs(1.0f), 0.0f, k_Tolerance);
}

TEST(CreatureTouch, AStrokeNeedsANewPartTheWaitAndTheHandReady)
{
	using feedback::BodyPart;
	// The first stroke comes two seconds after taking hold
	EXPECT_FALSE(feedback::StrokeDue({.sinceTouch = 1999.0f, .ready = 1999.0f}, std::nullopt, BodyPart::Head, 0.0f));
	EXPECT_TRUE(feedback::StrokeDue({.sinceTouch = 2000.0f, .ready = 2000.0f}, std::nullopt, BodyPart::Head, 0.0f));
	// Not the part last stroked
	EXPECT_FALSE(feedback::StrokeDue({.sinceTouch = 9000.0f, .ready = 9000.0f}, BodyPart::Head, BodyPart::Head, 0.0f));
	// After a stroke the hand is ready a second later, but the wait for the reward so far still holds
	const feedback::StrokeClocks oneSecondOn {.sinceTouch = 1000.0f, .ready = 2000.0f};
	EXPECT_FALSE(feedback::StrokeDue(oneSecondOn, BodyPart::Head, BodyPart::Belly, 0.0f));
	EXPECT_TRUE(feedback::StrokeDue(oneSecondOn, BodyPart::Head, BodyPart::Belly, 0.2f));
}

TEST(CreatureTouch, ASweepIsWeighedAsASlapOnlyComingOntoTheBodyOrCrossingTheMiddle)
{
	// Coming onto the body
	EXPECT_TRUE(feedback::SlapConsidered(true, false, 100.0f, 90.0f, 400.0f));
	// Staying on it, or off it, on one side of the screen
	EXPECT_FALSE(feedback::SlapConsidered(true, true, 100.0f, 90.0f, 400.0f));
	EXPECT_FALSE(feedback::SlapConsidered(false, false, 100.0f, 90.0f, 400.0f));
	// Crossing the middle of the screen, on the body or off it
	EXPECT_TRUE(feedback::SlapConsidered(false, false, 410.0f, 390.0f, 400.0f));
	EXPECT_TRUE(feedback::SlapConsidered(true, true, 390.0f, 410.0f, 400.0f));
}

TEST(CreatureTouch, ASlapPlaysMirroredWhenTheHandSweepsLeft)
{
	const auto left = feedback::ClassifySlap(9.0f, 200.0f, k_Height, true);
	ASSERT_TRUE(left.has_value());
	EXPECT_TRUE(left->mirrored);
	EXPECT_EQ(left->band, feedback::SlapBand::Waist);
	const auto right = feedback::ClassifySlap(9.0f, 200.0f, k_Height, false);
	ASSERT_TRUE(right.has_value());
	EXPECT_FALSE(right->mirrored);
	// Just past five heights a second is a slap; at it, none
	EXPECT_FALSE(feedback::ClassifySlap(9.0f, feedback::k_SlapSpeed * k_Height, k_Height, false).has_value());
	// Nine heights a second is hard
	const auto hard = feedback::ClassifySlap(3.0f, feedback::k_HardSlapSpeed * k_Height, k_Height, false);
	ASSERT_TRUE(hard.has_value());
	EXPECT_FALSE(hard->gentle);
	EXPECT_EQ(hard->animation, feedback::k_SlapFeet);
}

TEST(CreatureTouch, TheThirdHardSlapInARowAtOneHeightOnTheBodyBruises)
{
	const auto hardHead = *feedback::ClassifySlap(12.0f, 200.0f, k_Height, false);
	const auto hardWaist = *feedback::ClassifySlap(9.0f, 200.0f, k_Height, false);
	const auto gentleHead = *feedback::ClassifySlap(12.0f, 100.0f, k_Height, false);
	feedback::HardSlaps slaps;
	EXPECT_FALSE(feedback::CountSlap(slaps, hardHead, true));
	// A gentle one at the same height keeps the count
	EXPECT_FALSE(feedback::CountSlap(slaps, gentleHead, true));
	EXPECT_FALSE(feedback::CountSlap(slaps, hardHead, true));
	EXPECT_TRUE(feedback::CountSlap(slaps, hardHead, true));
	EXPECT_EQ(slaps.count, 0u);
	// Off the body the count goes on past three, and the first on the body then bruises
	EXPECT_FALSE(feedback::CountSlap(slaps, hardHead, false));
	EXPECT_FALSE(feedback::CountSlap(slaps, hardHead, false));
	EXPECT_FALSE(feedback::CountSlap(slaps, hardHead, false));
	EXPECT_TRUE(feedback::CountSlap(slaps, hardHead, true));
	// Another height starts again
	EXPECT_FALSE(feedback::CountSlap(slaps, hardHead, true));
	EXPECT_FALSE(feedback::CountSlap(slaps, hardHead, true));
	EXPECT_FALSE(feedback::CountSlap(slaps, hardWaist, true));
	EXPECT_EQ(slaps.count, 1u);
}

TEST(CreatureTouch, ABruiseIsMarkedByTheSlapsSpeedAndTakesAHundredthOfLife)
{
	const auto hard = feedback::k_HardSlapSpeed * k_Height;
	EXPECT_EQ(feedback::BruiseColumn(hard, k_Height), 7);
	EXPECT_EQ(feedback::BruiseColumn(hard * 1.2f, k_Height), 6);
	EXPECT_EQ(feedback::BruiseColumn(hard * 3.0f, k_Height), 0);
	EXPECT_NEAR(feedback::LifeAfterBruise(0.5f), 0.49f, k_Tolerance);
	EXPECT_NEAR(feedback::LifeAfterBruise(0.015f), 0.01f, k_Tolerance);
	EXPECT_NEAR(feedback::LifeAfterBruise(0.005f), 0.005f, k_Tolerance);
}

TEST(CreatureTouch, TheHandCaressesWhileStrokingAndKeepsSlappingUntilItStrokesAgain)
{
	using feedback::TouchCycle;
	EXPECT_EQ(feedback::NextCycle(TouchCycle::Tickle, false, false, true), TouchCycle::Tickle);
	// Off the body the caress becomes the wiggle
	EXPECT_EQ(feedback::NextCycle(TouchCycle::Tickle, false, false, false), TouchCycle::Wiggle);
	// A fast hand slaps, on or off the body, and goes on slapping slowed down
	EXPECT_EQ(feedback::NextCycle(TouchCycle::Wiggle, true, false, false), TouchCycle::Slap);
	EXPECT_EQ(feedback::NextCycle(TouchCycle::Slap, false, false, true), TouchCycle::Slap);
	EXPECT_EQ(feedback::NextCycle(TouchCycle::Slap, false, false, false), TouchCycle::Slap);
	// Until it strokes
	EXPECT_EQ(feedback::NextCycle(TouchCycle::Slap, false, true, true), TouchCycle::Tickle);
}

TEST(CreatureTouch, AClickIsLettingGoWithinNearlyHalfASecond)
{
	EXPECT_TRUE(creature_hand::IsClick(449.0f, false));
	EXPECT_FALSE(creature_hand::IsClick(450.0f, false));
}

TEST(CreatureTouch, TheCameraLooksAtTheCreaturesMiddleFromInFront)
{
	const glm::vec3 position {100.0f, 20.0f, 300.0f};
	for (const float heading : {0.0f, 0.7f, 2.0f, -2.5f})
	{
		const auto view = creature_interact::ViewOf(position, k_Height, heading);
		EXPECT_NEAR(glm::distance(view.focus, position + glm::vec3(0.0f, 7.5f, 0.0f)), 0.0f, k_Tolerance);
		// One and a half heights from what it looks at
		EXPECT_NEAR(glm::distance(view.origin, view.focus), 22.5f, k_Tolerance);
		// Ahead of the creature, the way it faces
		const auto across = glm::normalize(glm::vec2(view.origin.x - position.x, view.origin.z - position.z));
		EXPECT_NEAR(glm::dot(across, creature_locomotion::DirectionOf(heading)), 1.0f, k_Tolerance);
		// A little above its middle: 15 up, less 0.7 of its height, for every 30 ahead
		const auto offset = view.origin - view.focus;
		EXPECT_NEAR(offset.y / glm::length(glm::vec2(offset.x, offset.z)), (15.0f - (0.7f * k_Height)) / 30.0f, k_Tolerance);
	}
	// It looks down on a small creature and up at a big one
	EXPECT_GT(creature_interact::ViewOf(position, 7.5f, 0.0f).origin.y, position.y + 3.75f);
	EXPECT_LT(creature_interact::ViewOf(position, 45.0f, 0.0f).origin.y, position.y + 22.5f);
}

TEST(CreatureTouch, TheCameraGlidesToTheCreatureInSevenTenthsOfASecond)
{
	using namespace std::chrono_literals;
	const Camera camera;
	CreatureInteractCameraModel model({0.0f, 100.0f, 0.0f}, {0.0f, 0.0f, 100.0f});
	const auto target = creature_interact::ViewOf({50.0f, 0.0f, 50.0f}, k_Height, 0.0f);
	model.SetTarget(target);
	// Held still, half way through the first glide it is on its way
	auto pose = model.Update(16ms, camera);
	ASSERT_TRUE(pose.has_value());
	EXPECT_GT(glm::distance(pose->origin, target.origin), 1.0f);
	EXPECT_EQ(pose->duration, std::chrono::microseconds::zero());
	// Without a new target it arrives 0.7 seconds after the last
	model.SetTarget(std::nullopt);
	for (int i = 0; i < 44; ++i)
	{
		pose = model.Update(16ms, camera);
	}
	ASSERT_TRUE(pose.has_value());
	EXPECT_NEAR(glm::distance(pose->origin, target.origin), 0.0f, k_Tolerance);
	EXPECT_NEAR(glm::distance(pose->focus, target.focus), 0.0f, k_Tolerance);
}

TEST(CreatureTouch, TheCameraWooshesFlyingBackFromFarAway)
{
	EXPECT_FALSE(creature_interact::WooshesFlyingBack({0.0f, 0.0f, 0.0f}, {150.0f, 0.0f, 0.0f}));
	EXPECT_TRUE(creature_interact::WooshesFlyingBack({0.0f, 0.0f, 0.0f}, {150.1f, 0.0f, 0.0f}));
}

TEST(CreatureTouch, HeldToACreatureTheHandDropsToTheWiggleAtOnceButFadesIntoEveryOtherCycle)
{
	using State = HandAnimation::State;
	using Cycle = HandAnimation::Cycle;
	// Leaving the body, the caress turns back to the wiggle without a fade
	EXPECT_FALSE(HandAnimation::FadesBetween(State::Creature, Cycle::Tickle, Cycle::Wiggle));
	// Into the caress or the slap it fades, as everywhere else
	EXPECT_TRUE(HandAnimation::FadesBetween(State::Creature, Cycle::Wiggle, Cycle::Tickle));
	EXPECT_TRUE(HandAnimation::FadesBetween(State::Creature, Cycle::Tickle, Cycle::Slap));
	EXPECT_TRUE(HandAnimation::FadesBetween(State::Creature, Cycle::Slap, Cycle::Tickle));
	EXPECT_TRUE(HandAnimation::FadesBetween(State::Normal, Cycle::Stroke, Cycle::Wiggle));
	EXPECT_FALSE(HandAnimation::FadesBetween(State::Creature, Cycle::Slap, Cycle::Slap));
}

TEST(CreatureTouch, TakingHoldTheHandsBackFacesTheCamera)
{
	const auto up = feedback::HandUpOnTakingHold(glm::vec3(0.0f), glm::vec3(0.0f, 30.0f, -40.0f));
	EXPECT_NEAR(up.y, 0.6f, k_Tolerance);
	EXPECT_NEAR(up.z, -0.8f, k_Tolerance);
}

TEST(CreatureTouch, OnTheBodyTheHandTurnsToItsSurfaceTippedTowardsTheCameraAsTheCursorMoves)
{
	const glm::vec3 creature(0.0f);
	const glm::vec3 camera(0.0f, 0.0f, -10.0f);
	const glm::vec3 out(1.0f, 0.0f, 0.0f);
	const auto glide = feedback::HandUpGlideTo(true, true, out, creature, camera);
	ASSERT_TRUE(glide.has_value());
	EXPECT_NEAR(glide->destination.x, 1.0f, k_Tolerance);
	EXPECT_NEAR(glide->destination.z, -0.4f, k_Tolerance);
	EXPECT_FLOAT_EQ(glide->seconds, 0.9f);
	// Resting still on the body, it goes on turning as it was
	EXPECT_FALSE(feedback::HandUpGlideTo(true, false, out, creature, camera).has_value());
	// Off the body it stands up again, slowly
	const auto upright = feedback::HandUpGlideTo(false, false, out, creature, camera);
	ASSERT_TRUE(upright.has_value());
	EXPECT_EQ(upright->destination, glm::vec3(0.0f, 1.0f, 0.0f));
	EXPECT_FLOAT_EQ(upright->seconds, 2.5f);
}

TEST(CreatureTouch, TheHandsUpEasesRoundOverItsTime)
{
	Zoomer3 up(glm::vec3(0.0f, 0.0f, -1.0f));
	const auto glide = feedback::HandUpGlideTo(false, false, glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
	ASSERT_TRUE(glide.has_value());
	up.SetDestination(glide->destination, glide->seconds);
	up.Update(1.0f);
	EXPECT_GT(up.GetValue().y, 0.0f);
	EXPECT_LT(up.GetValue().y, 1.0f);
	up.Update(1.5f);
	EXPECT_NEAR(up.GetValue().y, 1.0f, k_Tolerance);
	EXPECT_NEAR(up.GetValue().z, 0.0f, k_Tolerance);
}

TEST(CreatureTouch, TheBodysSurfaceFacesOutFromTheNearestCapsule)
{
	const std::array<feedback::Capsule, 2> body {
	    feedback::Capsule {.from = glm::vec3(0.0f), .to = glm::vec3(0.0f, 10.0f, 0.0f), .radius = 1.0f},
	    feedback::Capsule {.from = glm::vec3(5.0f, 0.0f, 0.0f), .to = glm::vec3(5.0f, 10.0f, 0.0f), .radius = 1.0f},
	};
	const auto normal = feedback::SurfaceNormal(glm::vec3(0.0f, 5.0f, -1.0f), body);
	EXPECT_NEAR(normal.z, -1.0f, k_Tolerance);
	const auto other = feedback::SurfaceNormal(glm::vec3(6.0f, 5.0f, 0.0f), body);
	EXPECT_NEAR(other.x, 1.0f, k_Tolerance);
	// On a capsule's own line there is no way out: straight up
	EXPECT_EQ(feedback::SurfaceNormal(glm::vec3(0.0f, 5.0f, 0.0f), body), glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST(CreatureTouch, HeldTheCreatureLooksAtTheHandOrAtTheCameraWithTheHandNearItsMiddle)
{
	const glm::vec3 middle(0.0f, 5.0f, 0.0f);
	const glm::vec3 camera(0.0f, 10.0f, -30.0f);
	const glm::vec3 sight(0.0f, 0.0f, 1.0f);
	// Near its middle (within 0.6 of its height), it looks at the camera
	EXPECT_EQ(feedback::HeldLookPoint(glm::vec3(0.0f, 5.0f, -8.0f), middle, camera, sight, k_Height), camera);
	// Further away, at a point 0.3 of its height nearer the camera than the hand
	const auto look = feedback::HeldLookPoint(glm::vec3(10.0f, 5.0f, -2.0f), middle, camera, sight, k_Height);
	EXPECT_NEAR(look.x, 10.0f, k_Tolerance);
	EXPECT_NEAR(look.z, -2.0f - (0.3f * k_Height), k_Tolerance);
}
