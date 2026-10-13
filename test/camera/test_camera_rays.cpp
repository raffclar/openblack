/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <Camera/DrawnPose.h>
#include <ECS/Registry.h>
#include <Locator.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
constexpr glm::vec3 k_StartOrigin {1000.0f, 100.0f, 1000.0f};
constexpr glm::vec3 k_StartFocus {1000.0f, 0.0f, 1100.0f};
constexpr glm::vec3 k_FarOrigin {1300.0f, 120.0f, 1200.0f};
constexpr glm::vec3 k_FarFocus {1300.0f, 0.0f, 1320.0f};
constexpr auto k_Frame = 16ms;

/// Flat land at the level of the sea under every pixel that looks down. It keeps the eye of each ray.
class FlatLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> LandUnderPixel(glm::vec3 eye, glm::vec3 nearPoint, Sea /*sea*/) const override
	{
		eyes.push_back(eye);
		const auto direction = nearPoint - eye;
		if (direction.y >= 0.0f)
		{
			return std::nullopt;
		}
		return eye + direction * (-eye.y / direction.y);
	}

	mutable std::vector<glm::vec3> eyes;
};

/// The player holds the button that drags the land and moves the mouse up the screen
class DragAction final: public MockAction
{
public:
	[[nodiscard]] bool GetBindable(input::BindableActionMap actions) const override
	{
		const auto asked = static_cast<uint64_t>(actions);
		const auto bit = [asked](input::BindableActionMap action) { return (asked & static_cast<uint64_t>(action)) != 0; };
		return (held && bit(input::BindableActionMap::MOVE)) || (turnLeft && bit(input::BindableActionMap::ROTATE_LEFT)) ||
		       (middle && bit(input::BindableActionMap::ROTATE_AROUND_MOUSE_ON));
	}
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return cursor; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return delta; }
	/// The hand is on the land
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override
	{
		return {glm::vec3(1000.0f, 0.0f, 1150.0f), std::nullopt};
	}

	bool held {false};
	/// The key that turns the camera to the left
	bool turnLeft {false};
	/// The button that turns the camera about the land under the hand
	bool middle {false};
	glm::uvec2 cursor {k_Width / 2, k_Height * 3 / 4};
	glm::ivec2 delta {0, 0};
};

/// The world camera, with a frame as the game runs it: the controls, the step, then the kept view
class CameraRays: public testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<MockWindowingSystem>();
		_picking = new FlatLandPicking();
		_picking->camera = &_camera;
		Locator::pickingSystem::reset<MockPickingSystem>(_picking);
		_action = new DragAction();
		Locator::gameActionSystem::reset<MockAction>(_action);

		_camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f);
		std::ignore = _camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_StartOrigin, k_StartFocus));
		_camera.SetOrigin(k_StartOrigin).SetFocus(k_StartFocus);
		_camera.KeepDrawnView();
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

	void RunFrames(int frames)
	{
		for (int frame = 0; frame < frames; ++frame)
		{
			_camera.HandleActions(k_Frame);
			_camera.Update(k_Frame);
			_camera.KeepDrawnView();
			if (_action->held)
			{
				_action->cursor.y -= 2;
			}
		}
	}

	/// The eyes of the rays of the next step of the camera, without the kept view after it
	std::vector<glm::vec3> EyesOfTheNextStep()
	{
		_picking->eyes.clear();
		_camera.HandleActions(k_Frame);
		_camera.Update(k_Frame);
		return _picking->eyes;
	}

	Camera _camera;
	FlatLandPicking* _picking = nullptr;
	DragAction* _action = nullptr;
};
} // namespace

// The camera looks for the land from the view that it kept, not from where it goes
TEST_F(CameraRays, TheRaysStartFromTheKeptView)
{
	RunFrames(10);
	const auto kept = _camera.GetOrigin(Camera::Interpolation::Drawn);
	// The game moves the camera after the step, before the next frame
	_camera.SetOrigin(k_FarOrigin).SetFocus(k_FarFocus);
	const auto eyes = EyesOfTheNextStep();
	ASSERT_FALSE(eyes.empty());
	for (const auto& eye : eyes)
	{
		EXPECT_LT(glm::distance(eye, kept), 1e-3f);
	}
}

// A command puts the camera after the step: the next step looks for the land from the view before the command, and the
// step after that from the place
TEST_F(CameraRays, AfterAPlaceTheNextRaysStartFromTheViewBeforeThePlace)
{
	RunFrames(10);
	const auto before = _camera.GetOrigin(Camera::Interpolation::Drawn);
	_camera.Place(k_FarOrigin, k_FarFocus);
	auto eyes = EyesOfTheNextStep();
	ASSERT_FALSE(eyes.empty());
	for (const auto& eye : eyes)
	{
		EXPECT_LT(glm::distance(eye, before), 1e-3f);
	}
	_camera.KeepDrawnView();
	const auto after = _camera.GetOrigin(Camera::Interpolation::Drawn);
	EXPECT_LT(glm::distance(after, k_FarOrigin), 1.0f);
	eyes = EyesOfTheNextStep();
	ASSERT_FALSE(eyes.empty());
	for (const auto& eye : eyes)
	{
		EXPECT_LT(glm::distance(eye, after), 1e-3f);
	}
}

// A command that puts the camera where the kept view is changes nothing: a movement continues
TEST_F(CameraRays, APlaceAtTheKeptViewChangesNothing)
{
	_camera.SetOriginInterpolator(k_StartOrigin, k_FarOrigin, glm::vec3(0.0f), glm::vec3(0.0f));
	_camera.SetFocusInterpolator(k_StartFocus, k_FarFocus, glm::vec3(0.0f), glm::vec3(0.0f));
	_camera.SetInterpolatorDuration(2s).SetInterpolatorTime(0us);
	_camera.KeepDrawnView();
	_camera.Place(_camera.GetOrigin(Camera::Interpolation::Drawn), _camera.GetFocus(Camera::Interpolation::Drawn));
	EXPECT_EQ(_camera.GetOrigin(Camera::Interpolation::Target), k_FarOrigin);
	EXPECT_EQ(_camera.GetFocus(Camera::Interpolation::Target), k_FarFocus);
	EXPECT_EQ(_camera.GetInterpolatorDuration(), 2s);
}

// A command puts the camera while the player drags the land: the drag continues from where it started, as if the camera
// was not put
TEST_F(CameraRays, ALandDragThatThePlayerHoldsContinuesAfterAPlace)
{
	// The same drag twice from the same start: the second time, a command puts the camera to the side before the last frame
	const auto drag = [this](bool place) {
		std::ignore = _camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_StartOrigin, k_StartFocus));
		_camera.SetOrigin(k_StartOrigin).SetFocus(k_StartFocus);
		_camera.KeepDrawnView();
		*_action = DragAction();
		RunFrames(30);
		_action->held = true;
		_action->delta = {0, -2};
		RunFrames(20);
		if (place)
		{
			const auto aside = _camera.GetOrigin(Camera::Interpolation::Target) + glm::vec3(60.0f, 0.0f, 0.0f);
			_camera.Place(aside, aside + (k_StartFocus - k_StartOrigin));
		}
		RunFrames(1);
		return _camera.GetOrigin(Camera::Interpolation::Target);
	};
	const auto withoutPlace = drag(false);
	ASSERT_GT(glm::distance(withoutPlace, k_StartOrigin), 1.0f) << "the drag did not move the camera";
	EXPECT_LT(glm::distance(drag(true), withoutPlace), 0.5f);
}

// A command puts the camera while the player holds the key that turns it. The turn keeps the point that it turns about:
// the next frame looks from the place towards that point, not the way that the command set.
TEST_F(CameraRays, AKeyTurnThatThePlayerHoldsKeepsItsPointAfterAPlace)
{
	RunFrames(10);
	_action->turnLeft = true;
	RunFrames(20);
	const auto point = _camera.GetFocus(Camera::Interpolation::Target);
	// The place is to the side, and it looks across the old view
	const auto aside = _camera.GetOrigin(Camera::Interpolation::Target) + glm::vec3(60.0f, 0.0f, 0.0f);
	const auto across = glm::vec3(100.0f, -100.0f, 0.0f);
	_camera.Place(aside, aside + across);
	RunFrames(1);
	const auto origin = _camera.GetOrigin(Camera::Interpolation::Target);
	const auto look = glm::normalize(_camera.GetFocus(Camera::Interpolation::Target) - origin);
	const auto towardsPoint = glm::normalize(point - aside);
	// One frame turns the camera by much less than a tenth of a turn
	EXPECT_GT(glm::dot(look, towardsPoint), 0.98f);
	EXPECT_LT(glm::dot(look, glm::normalize(across)), 0.9f);
}

// A command puts the camera while the player turns it about the land under the hand. The turn keeps that land point and
// its distance: on the next frame the camera looks at the point from the same distance, as if the camera was not put.
TEST_F(CameraRays, ATurnAboutTheHandThatThePlayerHoldsKeepsItsPointAfterAPlace)
{
	const auto turn = [this](bool place) {
		std::ignore = _camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_StartOrigin, k_StartFocus));
		_camera.SetOrigin(k_StartOrigin).SetFocus(k_StartFocus);
		_camera.KeepDrawnView();
		*_action = DragAction();
		_action->cursor = {k_Width / 2, k_Height / 2};
		RunFrames(10);
		_action->middle = true;
		RunFrames(20);
		if (place)
		{
			// Higher and further back, on the same line of sight
			const auto back = glm::normalize(k_StartOrigin - k_StartFocus) * 80.0f;
			const auto origin = _camera.GetOrigin(Camera::Interpolation::Target) + back;
			_camera.Place(origin, origin + (k_StartFocus - k_StartOrigin));
		}
		RunFrames(1);
		return std::pair {_camera.GetOrigin(Camera::Interpolation::Target), _camera.GetFocus(Camera::Interpolation::Target)};
	};
	const auto withoutPlace = turn(false);
	const auto withPlace = turn(true);
	EXPECT_LT(glm::distance(withPlace.second, withoutPlace.second), 0.5f);
	EXPECT_LT(glm::distance(withPlace.first, withoutPlace.first), 0.5f);
}

// The eye moves away from a focus at almost the same point
TEST(CameraDrawnPose, AnEyeOnItsFocusMovesAway)
{
	const auto pose = camera_drawn::Separate({.eye = {10.0f, 5.0f, 10.0f}, .focus = {10.0f, 5.0f, 10.01f}});
	EXPECT_EQ(pose.eye, glm::vec3(9.0f, 6.0f, 10.0f));
	const auto apart = camera_drawn::Separate({.eye = {10.0f, 5.0f, 10.0f}, .focus = {10.0f, 5.0f, 10.1f}});
	EXPECT_EQ(apart.eye, glm::vec3(10.0f, 5.0f, 10.0f));
}

// The eye stays one unit above the land, and the focus moves with it
TEST(CameraDrawnPose, AnEyeBelowTheLandMovesAboveIt)
{
	const auto low = camera_drawn::AboveLand({.eye = {0.0f, 3.0f, 0.0f}, .focus = {0.0f, 0.0f, 50.0f}}, 10.0f);
	EXPECT_EQ(low.eye, glm::vec3(0.0f, 11.0f, 0.0f));
	EXPECT_EQ(low.focus, glm::vec3(0.0f, 8.0f, 50.0f));
	const auto high = camera_drawn::AboveLand({.eye = {0.0f, 11.5f, 0.0f}, .focus = {0.0f, 0.0f, 50.0f}}, 10.0f);
	EXPECT_EQ(high.eye, glm::vec3(0.0f, 11.5f, 0.0f));
	EXPECT_EQ(high.focus, glm::vec3(0.0f, 0.0f, 50.0f));
}

// The frame draws from above the land when the camera is below it, but the camera itself stays where it is
TEST_F(CameraRays, TheDrawnViewIsAboveTheLand)
{
	const auto below = glm::vec3(1000.0f, -5.0f, 1000.0f);
	const auto focus = glm::vec3(1000.0f, -10.0f, 1100.0f);
	_camera.SetOrigin(below).SetFocus(focus);
	_camera.KeepDrawnView();
	EXPECT_EQ(_camera.GetOrigin(Camera::Interpolation::Drawn), glm::vec3(1000.0f, 1.0f, 1000.0f));
	EXPECT_EQ(_camera.GetFocus(Camera::Interpolation::Drawn), glm::vec3(1000.0f, -4.0f, 1100.0f));
	const auto eye = glm::vec3(glm::inverse(_camera.GetViewMatrix(Camera::Interpolation::Current))[3]);
	EXPECT_LT(glm::distance(eye, glm::vec3(1000.0f, 1.0f, 1000.0f)), 1e-3f);
	EXPECT_EQ(_camera.GetOrigin(), below);
}
