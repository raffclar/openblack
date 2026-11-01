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

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
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

TEST(PlaygroundLessonNatives, ACreaturesLookIsMeasuredInPartsOfAHalfTurn)
{
	// 360 divided by the angle, cut towards 0
	EXPECT_EQ(CreatureLookParts(60.0f), 6);
	EXPECT_EQ(CreatureLookParts(7.0f), 51);
	EXPECT_EQ(CreatureLookParts(360.0f), 1);
	EXPECT_EQ(CreatureLookParts(-60.0f), -6);
	// More than a full turn gives no parts
	EXPECT_EQ(CreatureLookParts(400.0f), 0);
	// No angle, or not a number: the value for out of range
	EXPECT_EQ(CreatureLookParts(0.0f), std::numeric_limits<int32_t>::min());
	EXPECT_EQ(CreatureLookParts(std::numeric_limits<float>::quiet_NaN()), std::numeric_limits<int32_t>::min());
}

TEST(PlaygroundLessonNatives, ACreatureSeesTheCameraWhenItLooksNearTheDirectionToIt)
{
	using namespace openblack;
	const auto creature = map_coords::FromMetres({100.0f, 100.0f});
	const auto camera = map_coords::FromMetres({160.0f, 140.0f});
	const auto towards = gutils::GetAngleFromXZ(creature, camera);
	// 60 degrees: 6 parts, so near is less than 0x800 / 6 = 341 game angle units apart
	EXPECT_TRUE(CreatureSeesCamera(creature, camera, towards, 60.0f));
	EXPECT_TRUE(CreatureSeesCamera(creature, camera, static_cast<uint16_t>(towards + 340), 60.0f));
	EXPECT_FALSE(CreatureSeesCamera(creature, camera, static_cast<uint16_t>(towards + 341), 60.0f));
	EXPECT_FALSE(CreatureSeesCamera(creature, camera, static_cast<uint16_t>(towards - 341), 60.0f));
	// A full turn of look sees the camera in all directions
	EXPECT_TRUE(CreatureSeesCamera(creature, camera, static_cast<uint16_t>(towards + 1000), 360.0f));
}

TEST(PlaygroundLessonNatives, ACreatureSeesTheCameraInItsOwnMapCell)
{
	using namespace openblack;
	const auto creature = map_coords::FromMetres({100.0f, 100.0f});
	const auto camera = map_coords::FromMetres({101.0f, 101.0f});
	const auto away = static_cast<uint16_t>(gutils::GetAngleFromXZ(creature, camera) + 1000);
	EXPECT_TRUE(CreatureSeesCamera(creature, camera, away, 60.0f));
	// A negative angle is a very large number of parts: only the own cell counts
	EXPECT_TRUE(CreatureSeesCamera(creature, camera, away, -60.0f));
	const auto far = map_coords::FromMetres({160.0f, 140.0f});
	EXPECT_FALSE(CreatureSeesCamera(creature, far, gutils::GetAngleFromXZ(creature, far), -60.0f));
}

TEST(PlaygroundLessonNatives, ACreatureWithNoPartsOfLookSeesNothing)
{
	using namespace openblack;
	// The game divides by 0 here; openblack sees nothing
	const auto creature = map_coords::FromMetres({100.0f, 100.0f});
	EXPECT_FALSE(CreatureSeesCamera(creature, creature, 0, 400.0f));
}
