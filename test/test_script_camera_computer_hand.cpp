/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <optional>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Camera/ScriptCameraModel.h"

using namespace openblack;
using namespace openblack::script_camera;

namespace
{

constexpr glm::vec3 k_Start {2500.0f, 80.0f, 2500.0f};
// The camera starts level, so that a follow of a thing starts at the lowest pitch
constexpr glm::vec3 k_StartFocus {2550.0f, 80.0f, 2550.0f};
constexpr glm::vec3 k_Hand {2600.0f, 12.0f, 2580.0f};
constexpr glm::vec3 k_Thing {2520.0f, 20.0f, 2610.0f};
constexpr float k_Distance = 100.0f;
constexpr float k_Heading = 0.5f;

/// A script camera over flat land, with the follow settings of the tests
class ScriptCameraComputerHand: public ::testing::Test
{
protected:
	ScriptCameraModel _model {k_Start, k_StartFocus, [](float, float) { return 0.0f; }};
	Camera _camera;

	/// One frame of the camera
	void Frame() { _model.Update(std::chrono::milliseconds(20), _camera); }

	/// The follow settings that the tests give after each command, as a script does
	void Settings() { _model.SetFollowSettings(k_Distance, 0.2f, k_Heading, false); }

	/// Where the camera stands back from a point with the settings of the tests
	static glm::vec3 BackFrom(const glm::vec3& point)
	{
		FollowSettings settings {.heading = k_Heading, .pitch = 0.0f, .viewingDistance = k_Distance, .relativeHeading = false};
		return FollowOrigin({.point = point}, settings);
	}

	static ScriptCameraModel::HandLookup HandAt(glm::vec3 place)
	{
		return [place]() -> std::optional<glm::vec3> { return place; };
	}

	static ScriptCameraModel::Subject ThingAt(glm::vec3 place)
	{
		return {.id = 7, .lookup = [place]() -> std::optional<FollowedThing> { return FollowedThing {.point = place}; }};
	}
};

} // namespace

TEST_F(ScriptCameraComputerHand, TheCameraLooksAtTheHand)
{
	_model.LookAtComputerHand(HandAt(k_Hand));
	Frame();
	EXPECT_EQ(_model.GetTargetFocus(), k_Hand);
}

TEST_F(ScriptCameraComputerHand, AFollowedThingComesBeforeTheHand)
{
	_model.Follow(ThingAt(k_Thing));
	_model.LookAtComputerHand(HandAt(k_Hand));
	Frame();
	EXPECT_EQ(_model.GetTargetFocus(), k_Thing);
}

TEST_F(ScriptCameraComputerHand, TheCameraStandsBackFromTheHandThatItLooksAt)
{
	_model.LookAtComputerHand(HandAt(k_Hand));
	_model.FollowComputerHand(HandAt(k_Hand));
	Settings();
	Frame();
	EXPECT_EQ(_model.GetTargetOrigin(), BackFrom(k_Hand));
}

TEST_F(ScriptCameraComputerHand, TheCameraStandsBackFromTheThingThatItLooksAtNotFromTheHand)
{
	_model.LookAt(ThingAt(k_Thing));
	_model.FollowComputerHand(HandAt(k_Hand));
	Settings();
	Frame();
	EXPECT_EQ(_model.GetTargetFocus(), k_Thing);
	EXPECT_EQ(_model.GetTargetOrigin(), BackFrom(k_Thing));
}

TEST_F(ScriptCameraComputerHand, WithNothingToLookAtTheCameraStaysWhereItIs)
{
	_model.SetFocus(k_StartFocus);
	_model.FollowComputerHand(HandAt(k_Hand));
	Settings();
	const auto before = _model.GetTargetOrigin();
	Frame();
	EXPECT_EQ(_model.GetTargetOrigin(), before);
}

TEST_F(ScriptCameraComputerHand, AFocusPointStopsTheLookAtTheHand)
{
	_model.LookAtComputerHand(HandAt(k_Hand));
	_model.SetFocus(k_StartFocus);
	Frame();
	EXPECT_EQ(_model.GetTargetFocus(), k_StartFocus);
}

TEST_F(ScriptCameraComputerHand, AThingToLookAtStopsTheLookAtTheHand)
{
	_model.LookAtComputerHand(HandAt(k_Hand));
	// The thing is gone at once, so the camera has nothing to look at
	_model.LookAt({.id = 8, .lookup = []() -> std::optional<FollowedThing> { return std::nullopt; }});
	const auto before = _model.GetTargetFocus();
	Frame();
	EXPECT_EQ(_model.GetTargetFocus(), before);
	EXPECT_NE(_model.GetTargetFocus(), k_Hand);
}

TEST_F(ScriptCameraComputerHand, AnotherFollowOfAThingKeepsTheFollowOfTheHand)
{
	// A thing to follow comes first, and the follow of the hand comes back when the thing is gone
	bool thingThere = true;
	_model.LookAtComputerHand(HandAt(k_Hand));
	_model.FollowComputerHand(HandAt(k_Hand));
	_model.Follow({.id = 9, .lookup = [&thingThere]() -> std::optional<FollowedThing> {
		               return thingThere ? std::optional(FollowedThing {.point = k_Thing}) : std::nullopt;
	               }});
	Settings();
	Frame();
	EXPECT_EQ(_model.GetTargetOrigin(), BackFrom(k_Thing));
	thingThere = false;
	Frame();
	EXPECT_EQ(_model.GetTargetFocus(), k_Hand);
	EXPECT_EQ(_model.GetTargetOrigin(), BackFrom(k_Hand));
}
