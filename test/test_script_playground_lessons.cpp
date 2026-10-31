/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <limits>
#include <numbers>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Input/LandGrip.h"
#include "ScriptHeaders/ScriptLessonNatives.h"

using namespace openblack::script::lesson_natives;
using Miracles = openblack::ecs::components::Player::Miracles;

TEST(PlaygroundLessonNatives, AScriptNamesTheMagicTypesOneToFortyOne)
{
	EXPECT_FALSE(ValidMagic(0));
	EXPECT_TRUE(ValidMagic(1));
	EXPECT_TRUE(ValidMagic(41));
	EXPECT_FALSE(ValidMagic(42));
	EXPECT_FALSE(ValidMagic(-1));
}

TEST(PlaygroundLessonNatives, APlayerHasAMagicTypeThatWasEverEnabled)
{
	Miracles miracles;
	miracles.everEnabled.at(5) = true;
	EXPECT_TRUE(HasPlayerMagic(5, &miracles));
	EXPECT_FALSE(HasPlayerMagic(6, &miracles));
	// Enabled now but never marked: only the mark counts
	miracles.enabled.at(7) = 1;
	EXPECT_FALSE(HasPlayerMagic(7, &miracles));
}

TEST(PlaygroundLessonNatives, AMagicTypeOutOfTheRangeGivesFalseEvenForNoPlayer)
{
	Miracles miracles;
	miracles.everEnabled.fill(true);
	EXPECT_FALSE(HasPlayerMagic(0, &miracles));
	EXPECT_FALSE(HasPlayerMagic(42, &miracles));
	EXPECT_FALSE(HasPlayerMagic(0, nullptr));
}

TEST(PlaygroundLessonNatives, APlayerThatIsNotThereHasEveryMagicType)
{
	EXPECT_TRUE(HasPlayerMagic(1, nullptr));
	EXPECT_TRUE(HasPlayerMagic(41, nullptr));
}

TEST(PlaygroundLessonNatives, AKeyIsDownWhileItIsHeld)
{
	KeysHeld held;
	held.set(57);
	EXPECT_TRUE(KeyDown(57, held));
	EXPECT_FALSE(KeyDown(28, held));
}

TEST(PlaygroundLessonNatives, AKeyNumberPastTheTableIsNeverDown)
{
	KeysHeld held;
	held.set();
	EXPECT_TRUE(KeyDown(254, held));
	EXPECT_FALSE(KeyDown(255, held));
	EXPECT_FALSE(KeyDown(-1, held));
	EXPECT_FALSE(KeyDown(std::numeric_limits<int32_t>::max(), held));
}

TEST(PlaygroundLessonNatives, TheMouseHasAWheelWhenTheSystemSaysSoOrTheWheelTurned)
{
	EXPECT_FALSE(MouseHasWheel(false, false));
	EXPECT_TRUE(MouseHasWheel(true, false));
	EXPECT_TRUE(MouseHasWheel(false, true));
}

TEST(PlaygroundLessonNatives, TheCursorIsWithinRotationWhileACameraIconShowsOrIsOffered)
{
	EXPECT_FALSE(WithinRotation(0));
	// The icon that turns the camera shows
	EXPECT_TRUE(WithinRotation(0x2));
	// Only offered
	EXPECT_TRUE(WithinRotation(0x200));
	// The cross alone counts too
	EXPECT_TRUE(WithinRotation(0x8));
}

TEST(PlaygroundLessonNatives, TheHalfViewAngleIsHalfTheDegreesInRadians)
{
	EXPECT_FLOAT_EQ(HalfViewAngle(180.0f), std::numbers::pi_v<float> * 0.5f);
	EXPECT_FLOAT_EQ(HalfViewAngle(90.0f), std::numbers::pi_v<float> * 0.25f);
	EXPECT_EQ(HalfViewAngle(0.0f), 0.0f);
}

TEST(PlaygroundLessonNatives, APointAheadIsInTheViewCone)
{
	const glm::vec3 at {100.0f, 5.0f, 200.0f};
	const float half = HalfViewAngle(90.0f);
	// Facing +x
	EXPECT_TRUE(InViewCone(at, 0.0f, half, at + glm::vec3(10.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(InViewCone(at, 0.0f, half, at + glm::vec3(10.0f, 0.0f, -5.0f)));
	EXPECT_TRUE(InViewCone(at, 0.0f, half, at + glm::vec3(10.0f, 0.0f, 5.0f)));
	// The height does not count
	EXPECT_TRUE(InViewCone(at, 0.0f, half, at + glm::vec3(10.0f, 500.0f, 0.0f)));
}

TEST(PlaygroundLessonNatives, APointToTheSideOrBehindIsOutOfTheViewCone)
{
	const glm::vec3 at {100.0f, 5.0f, 200.0f};
	const float half = HalfViewAngle(90.0f);
	EXPECT_FALSE(InViewCone(at, 0.0f, half, at + glm::vec3(0.0f, 0.0f, 10.0f)));
	EXPECT_FALSE(InViewCone(at, 0.0f, half, at + glm::vec3(0.0f, 0.0f, -10.0f)));
	EXPECT_FALSE(InViewCone(at, 0.0f, half, at + glm::vec3(-10.0f, 0.0f, 0.0f)));
	EXPECT_FALSE(InViewCone(at, 0.0f, half, at + glm::vec3(10.0f, 0.0f, 20.0f)));
}

TEST(PlaygroundLessonNatives, TheViewConeTurnsWithTheThing)
{
	const glm::vec3 at {0.0f, 0.0f, 0.0f};
	const float half = HalfViewAngle(180.0f);
	const float facingZ = std::numbers::pi_v<float> * 0.5f;
	// Facing +z with a half turn of view: all of +z is in, all of -z is out
	EXPECT_TRUE(InViewCone(at, facingZ, half, {5.0f, 0.0f, 1.0f}));
	EXPECT_TRUE(InViewCone(at, facingZ, half, {-5.0f, 0.0f, 1.0f}));
	EXPECT_FALSE(InViewCone(at, facingZ, half, {5.0f, 0.0f, -1.0f}));
	EXPECT_FALSE(InViewCone(at, facingZ, half, {-5.0f, 0.0f, -1.0f}));
}

TEST(PlaygroundLessonNatives, APointThatIsNotANumberIsOutOfTheViewCone)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(InViewCone({0.0f, 0.0f, 0.0f}, 0.0f, HalfViewAngle(90.0f), {nan, 0.0f, 0.0f}));
}

TEST(PlaygroundLessonNatives, TheGripOfTheLandStartsWithADragAndEndsWhenTheMoveButtonIsLetGo)
{
	using openblack::input::land_grip::GripsLand;
	// A drag of the land starts the grip
	EXPECT_TRUE(GripsLand(true, false, true));
	// Both buttons stop the drag, but the grip continues while the Move button counts as held
	EXPECT_TRUE(GripsLand(false, true, true));
	// The Move button let go ends the grip
	EXPECT_FALSE(GripsLand(false, true, false));
	// The Move button held with no drag before does not start a grip
	EXPECT_FALSE(GripsLand(false, false, true));
}
