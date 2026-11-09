/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>
#include <memory>
#include <optional>
#include <tuple>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <Camera/WorldCamera.h>
#include <Camera/ZoomToPlaces.h>
#include <ECS/Registry.h>
#include <Locator.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
constexpr glm::vec3 k_Origin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_Focus {1000.0f, 0.0f, 1060.0f};
constexpr glm::vec3 k_FarOrigin {2000.0f, 80.0f, 2000.0f};
constexpr glm::vec3 k_FarFocus {2000.0f, 0.0f, 2080.0f};
constexpr auto k_Frame = 16ms;

/// The cursor picks no land
class NoPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

/// The wheel turns by a notch in each frame, the clear view keys can be held, and the buttons can be held
class Controls final: public MockAction
{
public:
	[[nodiscard]] bool GetBindable(input::BindableActionMap action) const override
	{
		using input::BindableActionMap;
		if (action == BindableActionMap::ZOOM_ON || action == BindableActionMap::ROTATE_ON)
		{
			return clearView;
		}
		return false;
	}
	[[nodiscard]] float GetMouseWheelDelta() const override { return wheel; }

	bool clearView {false};
	float wheel {0.0f};
};

class WorldCameraBehindTemple: public testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<MockWindowingSystem>();
		auto* picking = new NoPicking();
		picking->camera = &_camera;
		Locator::pickingSystem::reset<MockPickingSystem>(picking);
		_controls = new Controls();
		Locator::gameActionSystem::reset<MockAction>(_controls);

		_camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f);
		std::ignore = _camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_Origin, k_Focus));
		_camera.SetOrigin(k_Origin).SetFocus(k_Focus);
	}

	void TearDown() override
	{
		Locator::gameActionSystem::reset();
		Locator::pickingSystem::reset();
		Locator::windowing::reset();
		Locator::terrainSystem::reset();
		Locator::audio::reset();
		Locator::entitiesRegistry::reset();
	}

	/// One frame of the world's camera behind the temple: the mouse and the step of the camera, with no keys
	void Frame()
	{
		_camera.HandleActions(k_Frame);
		_camera.Update(k_Frame);
	}

	DefaultWorldCameraModel& World() { return dynamic_cast<DefaultWorldCameraModel&>(_camera.GetModel()); }

	Camera _camera;
	Controls* _controls = nullptr;
};
} // namespace

TEST(WorldCameraLeave, APlacePickedOnTheMapPutsThePlayersCameraThereAndStartsItAgain)
{
	const world_camera::Place place {.origin = k_FarOrigin, .focus = k_FarFocus};
	const auto leave = world_camera::OnLeave(place, world_camera::Model::World);
	ASSERT_TRUE(leave.cutTo.has_value());
	EXPECT_EQ(leave.cutTo->origin, k_FarOrigin);
	EXPECT_EQ(leave.cutTo->focus, k_FarFocus);
	EXPECT_FALSE(leave.newWorldModel);
	EXPECT_TRUE(leave.startAgain);
}

TEST(WorldCameraLeave, APlacePickedOnTheMapGivesANewCameraOfThePlayersOwnInPlaceOfAnotherModel)
{
	const world_camera::Place place {.origin = k_FarOrigin, .focus = k_FarFocus};
	const auto leave = world_camera::OnLeave(place, world_camera::Model::Other);
	ASSERT_TRUE(leave.cutTo.has_value());
	EXPECT_TRUE(leave.newWorldModel);
	EXPECT_FALSE(leave.startAgain);
}

TEST(WorldCameraLeave, WithNoPlacePickedOnlyThePlayersCameraStartsAgain)
{
	const auto world = world_camera::OnLeave(std::nullopt, world_camera::Model::World);
	EXPECT_FALSE(world.cutTo.has_value());
	EXPECT_FALSE(world.newWorldModel);
	EXPECT_TRUE(world.startAgain);
	// The camera that follows the creature continues
	const auto other = world_camera::OnLeave(std::nullopt, world_camera::Model::Other);
	EXPECT_FALSE(other.cutTo.has_value());
	EXPECT_FALSE(other.newWorldModel);
	EXPECT_FALSE(other.startAgain);
}

TEST(WorldCameraLeave, InTheTempleThePlayersCameraTakesNoControls)
{
	EXPECT_TRUE(world_camera::TakesControls(false));
	EXPECT_FALSE(world_camera::TakesControls(true));
}

TEST(WorldCameraLeave, StartingAgainForgetsTheTapAndTheFlightToThePlaceButKeepsWhereToFlyBack)
{
	const zoom_to::CameraView back {.origin = k_Origin, .focus = k_Focus};
	zoom_to::ZoomToPlaces places({.lastTap = 1234, .flownTo = true, .returnTo = back});
	places.StartAgain();
	EXPECT_EQ(places.GetState().lastTap, 0u);
	EXPECT_FALSE(places.GetState().flownTo);
	EXPECT_EQ(places.GetState().returnTo.origin, back.origin);
	EXPECT_EQ(places.GetState().returnTo.focus, back.focus);
}

TEST_F(WorldCameraBehindTemple, TheWheelDoesNotZoomTheCameraInTheTemple)
{
	World().SetInputsOff(true);
	_controls->wheel = 1.0f;
	for (int frame = 0; frame < 30; ++frame)
	{
		Frame();
	}
	// The wheel moves the eye nearer to what the camera looks at: here the eye stays
	EXPECT_EQ(_camera.GetOrigin(Camera::Interpolation::Target), k_Origin);
}

TEST_F(WorldCameraBehindTemple, TheWheelZoomsTheCameraOutside)
{
	_controls->wheel = 1.0f;
	for (int frame = 0; frame < 30; ++frame)
	{
		Frame();
	}
	EXPECT_NE(_camera.GetOrigin(Camera::Interpolation::Target), k_Origin);
}

TEST_F(WorldCameraBehindTemple, TheKeysOfTheClearViewStillActInTheTemple)
{
	World().SetInputsOff(true);
	_controls->clearView = true;
	for (int frame = 0; frame < 60; ++frame)
	{
		Frame();
	}
	EXPECT_NE(_camera.GetOrigin(Camera::Interpolation::Target), k_Origin);
}

TEST_F(WorldCameraBehindTemple, AFlightGoesOnBehindTheTemple)
{
	World().SetInputsOff(true);
	World().SetFlight(k_FarOrigin, k_FarFocus, 0.4f);
	for (int frame = 0; frame < 400; ++frame)
	{
		Frame();
	}
	EXPECT_LT(glm::distance(_camera.GetOrigin(), k_FarOrigin), 1.0f);
}

TEST_F(WorldCameraBehindTemple, StartingAgainInTheFirstPartOfAFlightStopsItAtItsHalfWayPoint)
{
	World().SetInputsOff(true);
	World().SetFlight(k_FarOrigin, k_FarFocus, 0.4f);
	// The first frame sets off for the half way point
	Frame();
	const auto halfWay = _camera.GetOrigin(Camera::Interpolation::Target);
	ASSERT_NE(halfWay, k_FarOrigin);
	World().StartAgain();
	EXPECT_FALSE(World().InputsOff());
	for (int frame = 0; frame < 400; ++frame)
	{
		Frame();
	}
	// The second part of the flight does not start: the camera stays at the half way point
	EXPECT_LT(glm::distance(_camera.GetOrigin(), halfWay), 1.0f);
	EXPECT_GT(glm::distance(_camera.GetOrigin(), k_FarOrigin), 100.0f);
}

TEST_F(WorldCameraBehindTemple, ACameraThatContinuesHasTheSameMovementModelAndTime)
{
	_camera.SetModelSeconds(3.5f);
	_camera.SetForcedTurnPoint(k_Focus);
	World().SetFlight(k_FarOrigin, k_FarFocus, 0.4f);
	Frame();
	const auto origin = _camera.GetOrigin();
	const auto target = _camera.GetOrigin(Camera::Interpolation::Target);
	const auto seconds = _camera.GetModelSeconds();

	auto behind = _camera.ContinueWith(_camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_Origin, k_Focus)));
	EXPECT_EQ(behind->GetOrigin(), origin);
	EXPECT_EQ(behind->GetOrigin(Camera::Interpolation::Target), target);
	EXPECT_EQ(behind->GetForcedTurnPoint(), std::optional(k_Focus));
	EXPECT_EQ(behind->GetAspect(), _camera.GetAspect());
	behind->SetModelSeconds(seconds);

	// The player's camera now shows something else, and gets the world's camera back with its movement
	_camera.SetOrigin(k_Origin).SetFocus(k_Focus);
	const auto* model = &behind->GetModel();
	behind->Update(k_Frame);
	const auto behindOrigin = behind->GetOrigin();
	_camera.TakeBackFrom(*behind);
	EXPECT_EQ(&_camera.GetModel(), model);
	EXPECT_EQ(_camera.GetOrigin(), behindOrigin);
	EXPECT_EQ(_camera.GetModelSeconds(), behind->GetModelSeconds());
}
