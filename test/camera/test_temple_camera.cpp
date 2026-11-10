/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <optional>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/CameraPath.h"
#include "Camera/Camera.h"
#include "Camera/CameraZoomer.h"
#include "Camera/TempleCameraModel.h"
#include "Camera/TemplePress.h"
#include "Common/Zoomer.h"

using namespace openblack;

namespace
{
/// A .cam file of a path over a second along x, looking ahead of itself
std::vector<uint8_t> MakeCam(uint32_t durationMs, uint32_t pointCount)
{
	std::vector<uint8_t> data(3 * sizeof(uint32_t) + pointCount * 6 * sizeof(float));
	const uint32_t header[3] {static_cast<uint32_t>(data.size()), durationMs, pointCount};
	std::memcpy(data.data(), header, sizeof(header));
	for (uint32_t i = 0; i < pointCount; ++i)
	{
		const float point[6] {static_cast<float>(i) * 10.0f, 5.0f, 0.0f, static_cast<float>(i) * 10.0f + 1.0f, 5.0f, 0.0f};
		std::memcpy(&data[sizeof(header) + i * sizeof(point)], point, sizeof(point));
	}
	return data;
}
} // namespace

TEST(Zoomer, ArrivesAtItsDestinationInTime)
{
	Zoomer value(2.0f);
	value.SetDestination(10.0f, 0.5f);
	float previous = value.GetValue();
	for (int i = 0; i < 25; ++i)
	{
		value.Update(0.02f);
		// Easing out of rest, it heads to the destination without going back
		EXPECT_GE(value.GetValue(), previous - 1e-4f);
		previous = value.GetValue();
	}
	EXPECT_FLOAT_EQ(value.GetValue(), 10.0f);
	EXPECT_FLOAT_EQ(value.GetSpeed(), 0.0f);
}

TEST(Zoomer, ItsSpeedIsHowFastItsValueChanges)
{
	// Zoomer's path is a polynomial in time whose terms are those of a Taylor series, so its speed is its derivative
	Zoomer value(0.0f);
	value.SetDestination(100.0f, 7.0f);
	value.Update(2.0f);
	const float before = value.GetValue();
	const float speed = value.GetSpeed();
	value.Update(0.001f);
	EXPECT_NEAR((value.GetValue() - before) / 0.001f, speed, 0.05f);
	// Easing out of rest towards a stop with no pull left, it has come most of the way by halfway
	value.Update(1.499f);
	EXPECT_NEAR(value.GetValue(), 68.75f, 0.01f);
}

TEST(Zoomer, PutsItThereForNoTime)
{
	Zoomer value(2.0f);
	value.SetDestination(-3.0f, 0.0f);
	EXPECT_FLOAT_EQ(value.GetValue(), -3.0f);
	value.Update(0.1f);
	EXPECT_FLOAT_EQ(value.GetValue(), -3.0f);
}

TEST(CameraZoomer, AGlideOfLessThanAboutAFiftiethOfASecondBarelyMovesUntilItsTimeIsUp)
{
	// The game keeps the determinant of the glide's matrix at 1e-10 or more, so a very short glide barely moves and then
	// jumps to its destination. A longer glide is halfway at 0.6875 of the way.
	camera::Zoomer shortGlide;
	shortGlide.SetPosition(0.0f);
	shortGlide.SetDestination(10.0f, 0.0f, 0.01f);
	shortGlide.Update(0.005f);
	EXPECT_LT(shortGlide.Value(), 0.001f);
	shortGlide.Update(0.006f);
	EXPECT_EQ(shortGlide.Value(), 10.0f);

	camera::Zoomer longGlide;
	longGlide.SetPosition(0.0f);
	longGlide.SetDestination(10.0f, 0.0f, 0.1f);
	longGlide.Update(0.05f);
	EXPECT_NEAR(longGlide.Value(), 6.875f, 1e-4f);
}

TEST(CameraPath, SamplesBetweenItsPointsAndHoldsTheLast)
{
	CameraPath path;
	ASSERT_TRUE(path.LoadFromBuffer(MakeCam(1000, 11)));
	// 10 spans of 100ms: halfway through the third is between the third and fourth points
	const auto middle = path.SampleAt(std::chrono::milliseconds(250));
	EXPECT_NEAR(middle.position.x, 25.0f, 1e-4f);
	EXPECT_NEAR(middle.focus.x, 26.0f, 1e-4f);
	EXPECT_FLOAT_EQ(path.SampleAt(std::chrono::milliseconds(0)).position.x, 0.0f);
	// Past its end, it stays just short of the last point, as InnerCamera's does
	EXPECT_NEAR(path.SampleAt(std::chrono::milliseconds(5000)).position.x, 99.9f, 1e-3f);
}

TEST(CameraPath, RefusesCamFilesShortOfTheirPoints)
{
	// CameraPath reports the file it couldn't read to the game's log
	if (spdlog::get("game") == nullptr)
	{
		spdlog::create<spdlog::sinks::null_sink_st>("game");
	}
	CameraPath path;
	auto data = MakeCam(1000, 4);
	data.resize(data.size() - 4);
	EXPECT_FALSE(path.LoadFromBuffer(data));
}

TEST(TempleCamera, LooksAtTheMainRoomFromItsOrbit)
{
	// Unturned, it starts behind the pool looking over it at the scroll, leaning a little up
	const auto start = TempleCameraModel::OrbitPose(0.0f, 0.7f);
	EXPECT_NEAR(start.origin.y, 9.92f, 1e-4f);
	EXPECT_NEAR(start.origin.z, 18.32f, 1e-4f);
	EXPECT_NEAR(start.focus.y, 8.4f, 1e-4f);
	// A quarter turn takes it round to the -x side
	const auto turned = TempleCameraModel::OrbitPose(glm::half_pi<float>(), 0.0f);
	EXPECT_NEAR(turned.origin.x, -14.4f, 1e-4f);
	EXPECT_NEAR(turned.origin.z, 0.0f, 1e-4f);
}

TEST(TempleCamera, FindsTheMainRoomsDoorsAroundItsWalls)
{
	// From the room's centre at door height, out along each door's eighth of the room, a little past its start
	for (uint32_t door = 0; door < 8; ++door)
	{
		const float angle = static_cast<float>(door) * glm::quarter_pi<float>() + 0.0f;
		const glm::vec3 direction {std::cos(angle), 0.0f, std::sin(angle)};
		const auto hit =
		    TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 15.0f, 0.0f), glm::vec3(0.0f, 15.0f, 0.0f) + direction * 0.2f);
		ASSERT_TRUE(hit.has_value());
		EXPECT_FALSE(hit->floor);
		EXPECT_NEAR(hit->distance, TempleCameraModel::k_RoomRadius, 1e-3f);
		const auto found = TempleCameraModel::DoorAt(*hit);
		ASSERT_TRUE(found.has_value()) << "door " << door;
		EXPECT_EQ(*found, door);
	}
	// Between doors, and above them, there are none
	const glm::vec3 between {std::cos(0.4f), 0.0f, std::sin(0.4f)};
	EXPECT_FALSE(TempleCameraModel::DoorAt(
	    *TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 15.0f, 0.0f), glm::vec3(0.0f, 15.0f, 0.0f) + between)));
	EXPECT_FALSE(
	    TempleCameraModel::DoorAt(*TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 30.0f, 0.0f), glm::vec3(1.0f, 30.0f, 0.0f))));
}

TEST(TempleCamera, HitsTheFloorBelowAndLeadsThroughTheDoorsToTheRooms)
{
	const auto hit = TempleCameraModel::RayCastRoom(glm::vec3(0.0f, 10.0f, 20.0f), glm::vec3(0.0f, 9.8f, 19.95f));
	ASSERT_TRUE(hit.has_value());
	EXPECT_TRUE(hit->floor);
	EXPECT_NEAR(hit->point.z, 17.5f, 1e-3f);

	// The rooms off the main room, around it from +x
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(0), TempleRoom::CreatureCave);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(1), TempleRoom::Options);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(3), TempleRoom::Multi);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(4), TempleRoom::Credits);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(5), TempleRoom::SaveGame);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(7), TempleRoom::Challenge);
	// And the way out
	EXPECT_FALSE(TempleCameraModel::RoomBehindDoor(2).has_value());
	// Through a door, the camera heads out along its eighth
	const auto through = TempleCameraModel::TurnToDoor({{100.0f, 8.0f, 0.0f}, {120.0f, 10.0f, 0.0f}}, 2);
	EXPECT_NEAR(through.origin.x, 0.0f, 1e-3f);
	EXPECT_NEAR(through.origin.z, 100.0f, 1e-3f);
}

TEST(TempleCamera, ClickingADoorOnScreenFindsIt)
{
	// The main room as the player takes it over, looking at the scroll through InnerCamera's lens
	constexpr glm::vec2 k_Screen {1280.0f, 720.0f};
	const auto pose = TempleCameraModel::OrbitPose(0.0f, 0.7f);
	Camera camera;
	camera.SetProjectionMatrixPerspective(90.0f, k_Screen.x / k_Screen.y, 0.2f);
	camera.SetOrigin(pose.origin);
	camera.SetFocus(pose.focus);

	// The middle of door 7 at door height, where the mouse is, cast back into the room as the camera's model does
	const float doorAngle = -glm::quarter_pi<float>();
	const glm::vec3 door {TempleCameraModel::k_RoomRadius * std::cos(doorAngle), 15.0f,
	                      TempleCameraModel::k_RoomRadius * std::sin(doorAngle)};
	glm::vec3 screen;
	ASSERT_TRUE(camera.ProjectWorldToScreen(door, glm::vec4(0.0f, 0.0f, k_Screen), screen));
	glm::vec3 rayOrigin;
	glm::vec3 rayDirection;
	camera.DeprojectScreenToWorld(glm::vec2(screen) / k_Screen, rayOrigin, rayDirection);
	const auto direction = rayDirection * (0.2f / glm::dot(rayDirection, camera.GetForward()));
	const auto hit = TempleCameraModel::RayCastRoom(camera.GetOrigin(), camera.GetOrigin() + direction);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->height, 15.0f, 0.05f);
	EXPECT_EQ(TempleCameraModel::DoorAt(*hit), 7u);
	EXPECT_EQ(TempleCameraModel::RoomBehindDoor(7), TempleRoom::Challenge);
}

TEST(TempleCamera, PictureRoomsStartLookingAlongTheEndOfTheirPaths)
{
	// A path ending at the options room's centre, looking out along -z
	const TempleCameraModel::Pose end {{87.0f, 10.0f, 87.0f}, {87.0f, 10.0f, 77.0f}};
	const float yaw = TempleCameraModel::PictureStartYaw(end);
	const auto pose = TempleCameraModel::PictureOrbitPose(TempleRoom::Options, yaw, 0.6f, 9.0f, end.origin);
	const auto look = glm::normalize(pose.focus - pose.origin);
	EXPECT_NEAR(look.x, 0.0f, 1e-4f);
	EXPECT_LT(look.z, -0.9f);
	// It stands back from the room's centre, the other way, raised by its height
	EXPECT_NEAR(pose.origin.x, 87.0f, 1e-3f);
	EXPECT_NEAR(pose.origin.z, 107.0f, 1e-3f);
	EXPECT_NEAR(pose.origin.y, 1.4f + 9.0f, 1e-3f);
}

TEST(TempleCamera, TheCreaturesRoomLooksOnFromTheEndOfItsPath)
{
	const TempleCameraModel::Pose end {{170.0f, -8.0f, 5.0f}, {213.0f, 4.0f, 19.0f}};
	const auto look = TempleCameraModel::HeadingAndPitch(end);
	const auto pose = TempleCameraModel::LookPose(end.origin, look.x, look.y);
	EXPECT_EQ(pose.origin, end.origin);
	const auto along = glm::normalize(pose.focus - pose.origin);
	const auto expected = glm::normalize(end.focus - end.origin);
	EXPECT_NEAR(glm::dot(along, expected), 1.0f, 1e-5f);
	EXPECT_NEAR(glm::length(pose.focus - pose.origin), 20.0f, 1e-3f);
}

TEST(TempleCamera, CastsAgainstEachRoomsOwnWalls)
{
	// From the challenge room's centre, out along +x, to its walls 37 away
	const auto room = TempleCameraModel::CylinderOf(TempleRoom::Challenge);
	const auto hit = TempleCameraModel::RayCastRoom(room.centre + glm::vec3(0.0f, 10.0f, 0.0f),
	                                                room.centre + glm::vec3(1.0f, 10.0f, 0.0f), room);
	ASSERT_TRUE(hit.has_value());
	EXPECT_NEAR(hit->distance, 37.0f, 1e-3f);
	EXPECT_NEAR(hit->point.x, room.centre.x + 37.0f, 1e-3f);
	EXPECT_NEAR(hit->height, 10.0f, 1e-3f);
}

TEST(TemplePress, APressHeldFromOutsideIsNoPressUntilLetGo)
{
	using temple_press::Button;
	temple_press::HeldFromOutside held;
	// Still holding the click that took the player in, the temple hears nothing
	EXPECT_EQ(held.Filter(Button::Held), Button::None);
	EXPECT_EQ(held.Filter(Button::Held), Button::None);
	// Once let go, the next press is a press
	EXPECT_EQ(held.Filter(Button::None), Button::None);
	EXPECT_EQ(held.Filter(Button::Held), Button::Held);
	// A double click always comes through
	temple_press::HeldFromOutside fresh;
	EXPECT_EQ(fresh.Filter(Button::DoubleClick), Button::DoubleClick);
}

TEST(TemplePress, TheCinemaBarsKeepPressesFromTheTemple)
{
	using temple_press::Button;
	temple_press::HeldFromOutside held;
	EXPECT_EQ(held.Filter(Button::None), Button::None);
	// While the advisors introduce a room, neither a press nor a double click reaches the temple
	EXPECT_EQ(held.FilterUnderCinemaBars(Button::Held), Button::None);
	EXPECT_EQ(held.FilterUnderCinemaBars(Button::DoubleClick), Button::None);
	// A press held through the scene isn't one once the bars go, until it is let go and pressed again
	EXPECT_EQ(held.Filter(Button::Held), Button::None);
	EXPECT_EQ(held.Filter(Button::None), Button::None);
	EXPECT_EQ(held.Filter(Button::Held), Button::Held);
	// Let go under the bars, the next press after them is one
	EXPECT_EQ(held.FilterUnderCinemaBars(Button::None), Button::None);
	EXPECT_EQ(held.Filter(Button::Held), Button::Held);
}

TEST(TemplePress, TheFadeHoldsPressesBackForTheFirstSecond)
{
	using temple_press::Button;
	// Coming in, the fade falls from 1.2 at a whole a second: no press cuts the flight short until it is down to 0.2
	EXPECT_EQ(temple_press::UnderFade(Button::Held, 1.2f), Button::None);
	EXPECT_EQ(temple_press::UnderFade(Button::Held, 0.21f), Button::None);
	EXPECT_EQ(temple_press::UnderFade(Button::Held, 0.2f), Button::Held);
	EXPECT_EQ(temple_press::UnderFade(Button::Held, 0.0f), Button::Held);
	EXPECT_EQ(temple_press::UnderFade(Button::DoubleClick, 1.2f), Button::DoubleClick);
	EXPECT_EQ(temple_press::UnderFade(Button::None, 0.0f), Button::None);
}

TEST(TemplePress, TheWatchKeepsTheRuleThatTookTheLastPress)
{
	using temple_press::Button;
	using temple_press::Taker;
	temple_press::PressWatch watch;
	EXPECT_EQ(watch.Last().presses, 0u);
	// A press under the fade: the fade takes it
	watch.Note(Button::Held, Button::Held, false, false, Button::None, 0.9f, false);
	EXPECT_EQ(watch.Last().takenBy, Taker::Fade);
	EXPECT_FLOAT_EQ(watch.Last().fade, 0.9f);
	EXPECT_EQ(watch.Last().presses, 1u);
	// The same press held on is no new press
	watch.Note(Button::Held, Button::Held, false, false, Button::Held, 0.0f, false);
	EXPECT_EQ(watch.Last().takenBy, Taker::Fade);
	EXPECT_EQ(watch.Last().presses, 1u);
	// Let go, then a press on a control
	watch.Note(Button::None, Button::None, false, false, Button::None, 0.0f, false);
	watch.Note(Button::Held, Button::Held, false, true, Button::None, 0.0f, false);
	EXPECT_EQ(watch.Last().takenBy, Taker::Control);
	EXPECT_EQ(watch.Last().presses, 2u);
	// A press that reaches the camera while it looks at a scroll
	watch.Note(Button::None, Button::None, false, false, Button::None, 0.0f, true);
	watch.Note(Button::Held, Button::Held, false, false, Button::Held, 0.0f, true);
	EXPECT_EQ(watch.Last().takenBy, Taker::None);
	EXPECT_TRUE(watch.Last().lookingAtScroll);
	EXPECT_EQ(watch.Last().presses, 3u);
}

TEST(TemplePress, TheFirstRuleThatTakesAPressIsNamed)
{
	using temple_press::Button;
	using temple_press::PressWatch;
	using temple_press::Taker;
	EXPECT_EQ(PressWatch::FirstTaker(Button::None, false, false, Button::None), Taker::HeldFromOutside);
	EXPECT_EQ(PressWatch::FirstTaker(Button::None, true, false, Button::None), Taker::CinemaBars);
	EXPECT_EQ(PressWatch::FirstTaker(Button::Held, false, true, Button::None), Taker::Control);
	EXPECT_EQ(PressWatch::FirstTaker(Button::Held, false, false, Button::None), Taker::Fade);
	EXPECT_EQ(PressWatch::FirstTaker(Button::DoubleClick, false, false, Button::DoubleClick), Taker::None);
	EXPECT_EQ(temple_press::Name(Taker::CinemaBars), "cinema_bars");
	EXPECT_EQ(temple_press::Name(Taker::None), "none");
}

TEST(TempleCamera, TheDoorsCloseOnlyAfterTheFirstFourTenthsOfASecondIn)
{
	EXPECT_FALSE(TempleCameraModel::IntroClosesDoors(0.0f));
	EXPECT_FALSE(TempleCameraModel::IntroClosesDoors(0.4f));
	EXPECT_TRUE(TempleCameraModel::IntroClosesDoors(0.41f));
	EXPECT_EQ(TempleCameraModel::k_IntroDoorCloseRate, 1.6f);
}

TEST(TempleCamera, APictureRoomTurnsByItselfUnderADialogSpeedingUpOverASecond)
{
	float time = 0.0f;
	// No dialog and no words: the arrow keys turn the room, and the time stays
	EXPECT_FALSE(TempleCameraModel::PictureTurnByItself(time, 0.5f, false, false, false).has_value());
	EXPECT_FLOAT_EQ(time, 0.0f);
	// The first frame of the turn has no speed, then the turn speeds up over a second to a quarter turn a second
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.5f, true, false, false), 0.0f);
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.5f, true, false, false), 0.5f * 0.5f * 0.25f);
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.5f, true, false, false), 0.5f * 0.25f);
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.5f, true, false, false), 0.5f * 0.25f);
}

TEST(TempleCamera, TheLibraryKeepsStillUnderADialog)
{
	float time = 2.0f;
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.5f, true, true, false), 0.0f);
	EXPECT_FLOAT_EQ(time, 2.5f);
}

TEST(TempleCamera, TheFutureRoomTurnsByItselfWhileItShowsItsWords)
{
	float time = 0.0f;
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.25f, false, false, true), 0.0f);
	EXPECT_FLOAT_EQ(*TempleCameraModel::PictureTurnByItself(time, 0.25f, false, false, true), 0.25f * 0.25f * 0.25f);
	EXPECT_FLOAT_EQ(time, 0.5f);
}

TEST(TempleCamera, TheMainRoomsOrbitAfterALookAtAScroll)
{
	// The player sends the camera back before the look is fully in: the orbit keeps its turn and leans up to 1
	const auto early = TempleCameraModel::MainRoomScrollLeave(true, 0.5f);
	ASSERT_TRUE(early.leanTarget.has_value());
	EXPECT_EQ(*early.leanTarget, 1.0f);
	EXPECT_FALSE(early.restartOrbit);
	// After a look that came fully in, the orbit starts again from its first turn and lean
	const auto full = TempleCameraModel::MainRoomScrollLeave(true, 1.0f);
	EXPECT_TRUE(full.restartOrbit);
	// The room that stops the look does not change the lean
	EXPECT_FALSE(TempleCameraModel::MainRoomScrollLeave(false, 0.5f).leanTarget.has_value());
	EXPECT_TRUE(TempleCameraModel::MainRoomScrollLeave(false, 1.0f).restartOrbit);
}

TEST(TempleCamera, TheRoomsStepByTheWholeMillisecondsTimesAThousandth)
{
	// Each step counts its whole milliseconds and multiplies them by the float a thousandth, as the game does
	for (int64_t milliseconds = 1; milliseconds < 120; ++milliseconds)
	{
		const float expected = static_cast<float>(milliseconds) * 0.001f;
		EXPECT_EQ(TempleCameraModel::StepSeconds(std::chrono::milliseconds(milliseconds)), expected);
	}
	// A part of a millisecond does not count
	EXPECT_EQ(TempleCameraModel::StepSeconds(std::chrono::microseconds(16'900)), 16.0f * 0.001f);
}

TEST(TempleCamera, TheCreaturesRoomMovesToItsAimMoreSlowlyFurtherIntoAZoom)
{
	EXPECT_EQ(TempleCameraModel::CaveLookEaseTime(0.0f), 0.15f);
	EXPECT_EQ(TempleCameraModel::CaveLookEaseTime(0.5f), (0.5f * 0.1f) + 0.15f);
	EXPECT_EQ(TempleCameraModel::CaveLookEaseTime(1.0f), (1.0f * 0.1f) + 0.15f);
}

TEST(TempleCamera, TheCreaturesRoomAimsAPartOfTheWayToItsZoom)
{
	const glm::vec3 from {120.0f, 10.0f, -120.0f};
	const TempleCameraModel::Pose look {from, {121.0f, 9.0f, -110.0f}};
	const TempleCameraModel::Pose zoomed {{100.0f, 12.0f, -100.0f}, {90.0f, 8.0f, -95.0f}};
	// With no zoom the aim is the look, and the look does not change
	const auto none = TempleCameraModel::CaveZoomAim(look, zoomed, from, 0.0f);
	EXPECT_EQ(none.aim.origin, look.origin);
	EXPECT_EQ(none.aim.focus, look.focus);
	EXPECT_FALSE(none.look.has_value());
	// A part of the way in, each point is the way between times the zoom, then the start added
	const auto part = TempleCameraModel::CaveZoomAim(look, zoomed, from, 0.25f);
	EXPECT_EQ(part.aim.origin, ((zoomed.origin - look.origin) * 0.25f) + look.origin);
	EXPECT_EQ(part.aim.focus, ((zoomed.focus - look.focus) * 0.25f) + look.focus);
	EXPECT_FALSE(part.look.has_value());
	// Fully in, the look turns to the point that the camera looks at from the place of the look
	const auto full = TempleCameraModel::CaveZoomAim(look, zoomed, from, 1.0f);
	ASSERT_TRUE(full.look.has_value());
	const auto expected = TempleCameraModel::HeadingAndPitch({from, full.aim.focus});
	EXPECT_EQ(full.look->x, expected.x);
	EXPECT_EQ(full.look->y, expected.y);
}

TEST(TempleCamera, AWalkBackStartsHalfwayToTheDoorAtItsHeight)
{
	const TempleCameraModel::Pose through {{40.0f, 16.0f, 0.0f}, {20.0f, 10.0f, 0.0f}};
	const TempleCameraModel::Pose from {{10.0f, 30.0f, 6.0f}, {0.0f, 4.0f, 2.0f}};
	const auto start = TempleCameraModel::WalkBackStart(through, from);
	EXPECT_EQ(start.origin, glm::vec3(25.0f, 16.0f, 3.0f));
	EXPECT_EQ(start.focus, glm::vec3(10.0f, 7.0f, 1.0f));
}

TEST(TempleCamera, ACloseLookMovesTheAimAlongACosineOfItsZoom)
{
	const TempleCameraModel::Pose aim {{0.0f, 10.0f, 0.0f}, {4.0f, 10.0f, 0.0f}};
	const TempleCameraModel::Pose look {{8.0f, 2.0f, -4.0f}, {12.0f, 0.0f, -4.0f}};
	// With no zoom, the aim does not change
	const auto none = TempleCameraModel::WithSubMeshLook(aim, look, 0.0f);
	EXPECT_EQ(none.origin, aim.origin);
	EXPECT_EQ(none.focus, aim.focus);
	// Halfway into the zoom, the aim is halfway to the look
	const auto half = TempleCameraModel::WithSubMeshLook(aim, look, 0.5f);
	EXPECT_EQ(half.origin, glm::vec3(4.0f, 6.0f, -2.0f));
	EXPECT_EQ(half.focus, glm::vec3(8.0f, 5.0f, -2.0f));
	// Fully in, and past it, the aim is the look
	for (const float zoom : {1.0f, 1.5f})
	{
		const auto full = TempleCameraModel::WithSubMeshLook(aim, look, zoom);
		EXPECT_EQ(full.origin, look.origin);
		EXPECT_EQ(full.focus, look.focus);
	}
	// A quarter into the zoom, the cosine gives less than a quarter of the way
	const auto quarter = TempleCameraModel::WithSubMeshLook(aim, look, 0.25f);
	EXPECT_LT(quarter.origin.x, 2.0f);
	EXPECT_GT(quarter.origin.x, 1.0f);
}

TEST(TempleCamera, TheRoomsMoveToACloseLooksAimMoreSlowlyTheNearerItIs)
{
	EXPECT_EQ(TempleCameraModel::MainRoomLookEaseTime(0.0f), 0.0f);
	EXPECT_EQ(TempleCameraModel::MainRoomLookEaseTime(1.0f), 0.2f);
	EXPECT_EQ(TempleCameraModel::MainRoomLookEaseTime(0.5f), 0.1f);
	EXPECT_EQ(TempleCameraModel::PictureRoomLookEaseTime(0.0f), 0.0f);
	EXPECT_EQ(TempleCameraModel::PictureRoomLookEaseTime(1.0f), 0.5f);
	EXPECT_EQ(TempleCameraModel::PictureRoomLookEaseTime(0.5f), 0.25f);
}

TEST(TempleCamera, TheCameraComesBackFromACloseLookAfterItsAim)
{
	// The aim comes back from the look along the cosine of the zoom, and the camera eases after it. Thus the camera is
	// behind its aim while the zoom goes out, and arrives at the end.
	const TempleCameraModel::Pose aim {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
	const TempleCameraModel::Pose look {{10.0f, 0.0f, 0.0f}, {11.0f, 0.0f, 0.0f}};
	Zoomer3 camera;
	camera.Reset(look.origin);
	float zoom = 1.0f;
	constexpr float k_Step = 0.016f;
	bool behind = false;
	for (int i = 0; i < 200; ++i)
	{
		zoom = std::max(zoom - (0.7f * k_Step), 0.0f);
		const auto target = TempleCameraModel::WithSubMeshLook(aim, look, zoom);
		camera.SetDestination(target.origin, TempleCameraModel::MainRoomLookEaseTime(zoom));
		camera.Update(k_Step);
		behind = behind || camera.GetValue().x > target.origin.x + 0.01f;
	}
	EXPECT_TRUE(behind);
	EXPECT_EQ(zoom, 0.0f);
	EXPECT_EQ(camera.GetValue(), aim.origin);
}

TEST(TempleCamera, APressOverTheCreatureRoomsDoorWalksBack)
{
	const glm::vec3 eye {60.0f, 15.0f, 0.0f};
	const glm::vec3 nearPoint {70.0f, 12.0f, 5.0f};
	const glm::vec3 farPoint {40.0f, 10.0f, 30.0f};
	// A new press over the door walks back through it, also far from the camera
	EXPECT_EQ(TempleCameraModel::CaveWalkBackDoor(true, true, 0u, std::nullopt, farPoint, eye), 0);
	// No press, or a point too far from the main room's centre, does not walk back
	EXPECT_FALSE(TempleCameraModel::CaveWalkBackDoor(false, true, 0u, std::nullopt, farPoint, eye).has_value());
	EXPECT_FALSE(
	    TempleCameraModel::CaveWalkBackDoor(true, true, 0u, std::nullopt, glm::vec3(79.0f, 0.0f, 0.0f), eye).has_value());
	EXPECT_FALSE(TempleCameraModel::CaveWalkBackDoor(true, true, 0u, std::nullopt, std::nullopt, eye).has_value());
	// A held press over the door walks back only after a walk back through it, and only near to the camera
	EXPECT_FALSE(TempleCameraModel::CaveWalkBackDoor(true, false, 0u, std::nullopt, nearPoint, eye).has_value());
	EXPECT_EQ(TempleCameraModel::CaveWalkBackDoor(true, false, 0u, 0u, nearPoint, eye), 0);
	EXPECT_FALSE(TempleCameraModel::CaveWalkBackDoor(true, false, 0u, 0u, farPoint, eye).has_value());
	// Before the first walk back, a press over no door near to the camera walks back through the door before the
	// first
	EXPECT_EQ(TempleCameraModel::CaveWalkBackDoor(true, false, std::nullopt, std::nullopt, nearPoint, eye), -1);
	EXPECT_EQ(TempleCameraModel::CaveWalkBackDoor(true, true, std::nullopt, std::nullopt, nearPoint, eye), -1);
	EXPECT_FALSE(TempleCameraModel::CaveWalkBackDoor(true, false, std::nullopt, 0u, nearPoint, eye).has_value());
}

TEST(TempleCamera, ARoomOfPicturesTurnsToTheLookWhenThePlayerLeavesIt)
{
	// The room turns a quarter turn on from the way the aim looks, as at its start, and goes low and leans to 0.2
	const TempleCameraModel::Pose alongX {{10.0f, 20.0f, 5.0f}, {14.0f, 18.0f, 5.0f}};
	const auto fromX = TempleCameraModel::PictureScrollLeave(alongX);
	EXPECT_EQ(fromX.yaw, glm::half_pi<float>());
	EXPECT_EQ(fromX.lean, 0.2f);
	EXPECT_EQ(fromX.height, 9.0f);
	const TempleCameraModel::Pose alongZ {{0.0f, 20.0f, 0.0f}, {0.0f, 20.0f, 3.0f}};
	EXPECT_EQ(TempleCameraModel::PictureScrollLeave(alongZ).yaw, glm::pi<float>());
	const TempleCameraModel::Pose back {{0.0f, 20.0f, 0.0f}, {-2.0f, 20.0f, -2.0f}};
	EXPECT_FLOAT_EQ(TempleCameraModel::PictureScrollLeave(back).yaw, -glm::quarter_pi<float>());
	// The turn is the same as the turn at the start of a look along the same way
	const TempleCameraModel::Pose slant {{3.0f, 9.0f, -4.0f}, {7.0f, 9.0f, 1.0f}};
	EXPECT_FLOAT_EQ(TempleCameraModel::PictureScrollLeave(slant).yaw, TempleCameraModel::PictureStartYaw(slant));
}

TEST(TempleCamera, ARoomOfPicturesHalfLeansAtTheHeightOfTheAimWhileItLooks)
{
	const TempleCameraModel::Pose aim {{1.0f, 27.5f, 2.0f}, {0.0f, 0.0f, 0.0f}};
	EXPECT_EQ(TempleCameraModel::PictureLookLeanAndHeight(aim), glm::vec2(0.5f, 27.5f));
}

TEST(TempleCamera, TheLeanOfARoomOfPicturesFollowsItsHeight)
{
	// At the lowest height the lean goes to 0.2, and higher it goes further down
	EXPECT_EQ(TempleCameraModel::PictureLeanForHeight(0.7f, 9.0f, 0.0f, false), 0.7f);
	const float low = TempleCameraModel::PictureLeanForHeight(0.7f, 9.0f, 0.016f, false);
	EXPECT_LT(low, 0.7f);
	EXPECT_NEAR(low, 0.7f - (0.5f * (1.0f - std::exp(-0.016f))), 1e-6f);
	float lean = 0.7f;
	for (int i = 0; i < 2000; ++i)
	{
		lean = TempleCameraModel::PictureLeanForHeight(lean, 80.0f, 0.016f, false);
	}
	EXPECT_NEAR(lean, ((80.0f - 9.0f) * 0.0084507046f) + 0.2f, 1e-4f);
	// When it levels out, the lean goes to 0
	EXPECT_LT(TempleCameraModel::PictureLeanForHeight(0.7f, 80.0f, 0.016f, true), 0.7f);
	EXPECT_EQ(TempleCameraModel::PictureLeanForHeight(0.0f, 80.0f, 0.016f, true), 0.0f);
}

TEST(TempleCamera, ThePressThatLeavesALookDoesNotDragUntilLetGo)
{
	// Held from the leave, the press stays held back; once let go, the next press drags
	EXPECT_TRUE(TempleCameraModel::PressFromLookStillHeld(true, true));
	EXPECT_FALSE(TempleCameraModel::PressFromLookStillHeld(true, false));
	EXPECT_FALSE(TempleCameraModel::PressFromLookStillHeld(false, true));
}

TEST(TemplePress, TheHoldAfterADoubleClickIsNoPressUntilLetGo)
{
	using temple_press::Button;
	temple_press::DoubleClickHold hold;
	// A first click is a press while the player holds it
	EXPECT_EQ(hold.Filter(Button::Held), Button::Held);
	EXPECT_EQ(hold.Filter(Button::None), Button::None);
	// The second click comes once as a double click, and its hold is not a press
	EXPECT_EQ(hold.Filter(Button::DoubleClick), Button::DoubleClick);
	EXPECT_EQ(hold.Filter(Button::Held), Button::None);
	EXPECT_EQ(hold.Filter(Button::Held), Button::None);
	// After the player lets go, the next press is a press
	EXPECT_EQ(hold.Filter(Button::None), Button::None);
	EXPECT_EQ(hold.Filter(Button::Held), Button::Held);
}

TEST(TemplePress, ARoomKeepsTheHoveredSubMeshWhileAPressIsHeld)
{
	using State = temple_press::RoomPress::State;
	temple_press::RoomPress press;
	EXPECT_TRUE(press.FollowsHover());
	// A press that goes down off the controls goes to the camera, and the room stops following the cursor, also when it
	// moves onto a control
	press.Step(true, false, false);
	EXPECT_EQ(press.GetState(), State::OffControls);
	press.Step(true, true, false);
	EXPECT_EQ(press.GetState(), State::OffControls);
	EXPECT_FALSE(press.FollowsHover());
	// Letting go frees the room
	press.Step(false, true, false);
	EXPECT_TRUE(press.FollowsHover());
	// A press that goes down on a control stays with the control
	press.Step(true, true, false);
	EXPECT_EQ(press.GetState(), State::OnControl);
	press.Step(true, false, false);
	EXPECT_EQ(press.GetState(), State::OnControl);
	press.Step(false, false, false);
	EXPECT_EQ(press.GetState(), State::Free);
}

TEST(TemplePress, ARoomTakesNoPressWhileABoxIsUp)
{
	using State = temple_press::RoomPress::State;
	temple_press::RoomPress press;
	// The press goes down while the box is up: the room does not hear it
	press.Step(true, false, true);
	EXPECT_EQ(press.GetState(), State::Free);
	// The box goes while the player holds the press: it is still no press, as the room did not hear it go down
	press.Step(true, false, false);
	EXPECT_EQ(press.GetState(), State::Free);
	// A box that comes up frees the room from a held press
	press.Step(false, false, false);
	press.Step(true, false, false);
	EXPECT_EQ(press.GetState(), State::OffControls);
	press.Step(true, false, true);
	EXPECT_EQ(press.GetState(), State::Free);
}

TEST(TempleCamera, AHeldPressOrADoubleClickStopsACloseLook)
{
	// The look stops in each step that the camera gets a press, also a press that started before the look
	EXPECT_TRUE(TempleCameraModel::LeavesLookOnPress(static_cast<uint8_t>(temple_press::Button::Held)));
	EXPECT_TRUE(TempleCameraModel::LeavesLookOnPress(static_cast<uint8_t>(temple_press::Button::DoubleClick)));
	// A press on a control comes to the camera as no press, so the look continues
	EXPECT_FALSE(TempleCameraModel::LeavesLookOnPress(static_cast<uint8_t>(temple_press::Button::None)));
}
