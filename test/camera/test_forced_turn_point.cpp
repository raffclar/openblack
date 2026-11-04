/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <chrono>
#include <memory>
#include <numbers>
#include <optional>
#include <tuple>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <Camera/FightCameraModel.h>
#include <Camera/ForcedTurnPoint.h>
#include <ECS/Registry.h>
#include <Input/BindableActions.h>
#include <Locator.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
constexpr glm::vec3 k_Origin {1000.0f, 60.0f, 1000.0f};
constexpr glm::vec3 k_Focus {1000.0f, 0.0f, 1060.0f};
/// A point to the side of what the camera looks at
constexpr glm::vec3 k_Point {1040.0f, 5.0f, 1050.0f};
constexpr auto k_Frame = 16ms;

/// No land under the cursor
class NoLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

/// The key that turns the camera to the left and the key that moves it to the left, held while asked
class TurnKeyAction final: public MockAction
{
public:
	[[nodiscard]] bool GetBindable(input::BindableActionMap action) const override
	{
		return (turnLeft && action == input::BindableActionMap::ROTATE_LEFT) ||
		       (moveLeft && action == input::BindableActionMap::MOVE_LEFT);
	}

	bool turnLeft {false};
	bool moveLeft {false};
};

/// A camera that is not the player's camera
class OtherCameraModel final: public CameraModel
{
public:
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds /*dt*/, const Camera& /*camera*/) override
	{
		return std::nullopt;
	}
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/, float /*rise*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return k_Origin; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return k_Focus; }
};

class ForcedTurnPoint: public testing::Test
{
protected:
	void SetUp() override
	{
		// The camera reports its flights to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<MockWindowingSystem>();
		auto* picking = new NoLandPicking();
		picking->camera = &_camera;
		Locator::pickingSystem::reset<MockPickingSystem>(picking);
		_action = new TurnKeyAction();
		Locator::gameActionSystem::reset<MockAction>(_action);

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

	void RunFrames(int frames)
	{
		for (int frame = 0; frame < frames; ++frame)
		{
			_camera.HandleActions(k_Frame);
			_camera.Update(k_Frame);
			_camera.HandleKeys(k_Frame);
		}
	}

	Camera _camera;
	TurnKeyAction* _action = nullptr;
};
} // namespace

TEST(ForcedTurnPointRules, TheDistanceAddsTheSquaresInTheOrderOfTheGame)
{
	EXPECT_EQ(forced_turn::Distance({3.0f, 0.0f, 0.0f}, {0.0f, 4.0f, 0.0f}), 5.0f);
	const glm::vec3 point {1040.3f, 5.7f, 1050.1f};
	const glm::vec3 origin {1000.9f, 60.2f, 999.4f};
	const float dx = point.x - origin.x;
	const float dy = point.y - origin.y;
	const float dz = point.z - origin.z;
	const float squares = (dz * dz) + (dy * dy);
	EXPECT_EQ(forced_turn::Distance(point, origin), std::sqrt(squares + (dx * dx)));
}

TEST(ForcedTurnPointRules, TheViewLooksAtThePointFromTheSameDistance)
{
	const auto view = forced_turn::AboutPoint(k_Point, k_Origin, 0.0f, 0.0f);
	EXPECT_EQ(view.focus, k_Point);
	// A heading of 0 and a pitch of 0 put the camera level with the point, on the side of the larger depth
	const float distance = forced_turn::Distance(k_Point, k_Origin);
	EXPECT_NEAR(view.origin.x, k_Point.x, 1e-3f);
	EXPECT_NEAR(view.origin.y, k_Point.y, 1e-3f);
	EXPECT_NEAR(view.origin.z, k_Point.z + distance, 1e-3f);
	// A quarter turn puts it across from the point; a pitch of a quarter turn puts it over the point
	const auto across = forced_turn::AboutPoint(k_Point, k_Origin, std::numbers::pi_v<float> * 0.5f, 0.0f);
	EXPECT_NEAR(across.origin.x, k_Point.x + distance, 1e-3f);
	const auto over = forced_turn::AboutPoint(k_Point, k_Origin, 0.0f, std::numbers::pi_v<float> * 0.5f);
	EXPECT_NEAR(over.origin.y, k_Point.y + distance, 1e-3f);
}

TEST(ForcedTurnPointRules, OnlyThePlayersCameraAndItsFightViewTakeThePoint)
{
	const DefaultWorldCameraModel world(k_Origin, k_Focus);
	EXPECT_TRUE(forced_turn::TakesPoint(world));
	const FightCameraModel fight(nullptr, {.origin = k_Origin, .focus = k_Focus});
	EXPECT_TRUE(forced_turn::TakesPoint(fight));
	const OtherCameraModel other;
	EXPECT_FALSE(forced_turn::TakesPoint(other));
}

TEST_F(ForcedTurnPoint, TheCameraLooksAtThePointAndKeepsItsPlace)
{
	_camera.SetForcedTurnPoint(k_Point);
	RunFrames(1);
	const auto& model = _camera.GetModel();
	EXPECT_EQ(model.GetTargetFocus(), k_Point);
	// With no control used, the camera stays where it is and only turns to look at the point
	EXPECT_LT(glm::distance(model.GetTargetOrigin(), k_Origin), 1e-2f);
	// The camera eases to the view and stays there
	RunFrames(400);
	EXPECT_LT(glm::distance(_camera.GetFocus(), k_Point), 1e-2f);
	EXPECT_LT(glm::distance(_camera.GetOrigin(), k_Origin), 1e-2f);
	EXPECT_EQ(_camera.GetModel().GetTargetFocus(), k_Point);
	EXPECT_EQ(_camera.GetForcedTurnPoint(), k_Point);
}

TEST_F(ForcedTurnPoint, ATurnGoesRoundThePoint)
{
	_camera.SetForcedTurnPoint(k_Point);
	RunFrames(1);
	const float distance = glm::distance(_camera.GetModel().GetTargetOrigin(), k_Point);
	_action->turnLeft = true;
	RunFrames(30);
	_action->turnLeft = false;
	RunFrames(1);
	const auto& model = _camera.GetModel();
	EXPECT_EQ(model.GetTargetFocus(), k_Point);
	// The camera moved round the point. The turn of the world camera can change the distance a little, and the end of
	// the step keeps the distance that the turn gives.
	EXPECT_GT(glm::distance(model.GetTargetOrigin(), k_Origin), 1.0f);
	EXPECT_NEAR(glm::distance(model.GetTargetOrigin(), k_Point), distance, distance * 0.01f);
}

TEST_F(ForcedTurnPoint, TheFightViewRemovesThePointOnceItFollowsTheFighters)
{
	_camera.SetForcedTurnPoint(k_Point);
	auto fight = std::make_unique<FightCameraModel>(nullptr, fight_view::View {.origin = k_Origin, .focus = k_Focus});
	auto* view = fight.get();
	view->SetPlayerModel(_camera.SetModel(std::move(fight)));
	// With no fighters the view does not follow them, and the point stays
	RunFrames(1);
	EXPECT_EQ(_camera.GetForcedTurnPoint(), k_Point);
	view->SetFighters({.position = k_Focus + glm::vec3(-5.0f, 0.0f, 0.0f), .radius = 2.0f, .height = 6.0f},
	                  {.position = k_Focus + glm::vec3(5.0f, 0.0f, 0.0f), .radius = 2.0f, .height = 6.0f});
	RunFrames(1);
	EXPECT_FALSE(_camera.GetForcedTurnPoint().has_value());
}

TEST_F(ForcedTurnPoint, TheFightViewDoesNotFollowTheFightersWhileTheKeysMoveTheCamera)
{
	auto fight = std::make_unique<FightCameraModel>(nullptr, fight_view::View {.origin = k_Origin, .focus = k_Focus});
	auto* view = fight.get();
	view->SetPlayerModel(_camera.SetModel(std::move(fight)));
	view->SetFighters({.position = k_Focus + glm::vec3(-5.0f, 0.0f, 0.0f), .radius = 2.0f, .height = 6.0f},
	                  {.position = k_Focus + glm::vec3(5.0f, 0.0f, 0.0f), .radius = 2.0f, .height = 6.0f});
	// The keys of a frame act in the next frame. In the steps that the movement key moves the camera, the view does not
	// follow the fighters, and the point stays.
	_action->moveLeft = true;
	RunFrames(1);
	_camera.SetForcedTurnPoint(k_Point);
	const auto originBefore = _camera.GetOrigin(Camera::Interpolation::Target);
	RunFrames(3);
	EXPECT_FALSE(_camera.GetModel().EndsForcedTurnPoint());
	EXPECT_EQ(_camera.GetForcedTurnPoint(), k_Point);
	// The player's camera moves to the side
	EXPECT_NE(_camera.GetOrigin(Camera::Interpolation::Target), originBefore);
	_action->moveLeft = false;
	// The key is let go: the step after the last key step follows the fighters again and removes the point
	RunFrames(2);
	EXPECT_TRUE(_camera.GetModel().EndsForcedTurnPoint());
	EXPECT_FALSE(_camera.GetForcedTurnPoint().has_value());
}
