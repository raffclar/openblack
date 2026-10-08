/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The editor's state: its orbiting camera takes the camera from the player's and hands it back, keeps off while a
// script's camera moves the view, and its step plays one turn from paused

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Camera/ScriptCamera.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/EditorSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace std::chrono_literals;
using openblack::ecs::systems::EditorSystem;
using CameraMode = openblack::ecs::systems::EditorSystemInterface::CameraMode;

namespace
{
class EditorSystemTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_camera = &Locator::camera::emplace(glm::vec3(0.0f));
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		_thing = registry.Create();
		registry.Assign<ecs::components::Transform>(_thing, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		_playerModel = &_camera->GetModel();
		script_camera::Reset();
	}
	void TearDown() override { script_camera::Reset(); }

	/// The editor opened with the thing picked
	void OpenOnTheThing()
	{
		_editor.SetOpen(true);
		_editor.GetSelection().Select(_thing);
	}

	[[nodiscard]] bool PlayerHasTheCamera() const { return &_camera->GetModel() == _playerModel; }

	EditorSystem _editor;
	entt::entity _thing {entt::null};

private:
	test::RestoreService<Locator::camera> _restoreCamera;
	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	Camera* _camera {nullptr};
	const CameraModel* _playerModel {nullptr};
};

TEST_F(EditorSystemTest, OrbitTakesTheCameraAndFreeHandsItBack)
{
	OpenOnTheThing();
	_editor.SetCameraMode(CameraMode::Orbit);
	EXPECT_EQ(_editor.GetCameraMode(), CameraMode::Orbit);
	EXPECT_FALSE(PlayerHasTheCamera());

	_editor.SetCameraMode(CameraMode::Free);
	EXPECT_EQ(_editor.GetCameraMode(), CameraMode::Free);
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(EditorSystemTest, ClosingHandsTheCameraBack)
{
	OpenOnTheThing();
	_editor.SetCameraMode(CameraMode::Follow);
	ASSERT_FALSE(PlayerHasTheCamera());

	_editor.SetOpen(false);
	EXPECT_EQ(_editor.GetCameraMode(), CameraMode::Free);
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(EditorSystemTest, RefusesOrbitAndFollowWhileAScriptMovesTheCamera)
{
	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	OpenOnTheThing();

	_editor.SetCameraMode(CameraMode::Orbit);
	EXPECT_EQ(_editor.GetCameraMode(), CameraMode::Free);
	EXPECT_TRUE(PlayerHasTheCamera());

	_editor.SetCameraMode(CameraMode::Follow);
	EXPECT_EQ(_editor.GetCameraMode(), CameraMode::Free);
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(EditorSystemTest, LetsGoOfTheCameraWhenAScriptTakesIt)
{
	OpenOnTheThing();
	_editor.SetCameraMode(CameraMode::Orbit);
	ASSERT_FALSE(PlayerHasTheCamera());

	ASSERT_TRUE(script_camera::Begin({0.0f, 10.0f, 0.0f}, {0.0f, 0.0f, 10.0f}));
	_editor.Update(16ms);
	EXPECT_EQ(_editor.GetCameraMode(), CameraMode::Free);
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST_F(EditorSystemTest, ClosedItLeavesTheCameraAlone)
{
	_editor.GetSelection().Select(_thing);
	_editor.Update(16ms);
	EXPECT_EQ(_editor.GetCameraMode(), CameraMode::Free);
	EXPECT_TRUE(PlayerHasTheCamera());
}

TEST(EditorSystemStep, PlaysOneTurnFromPausedThenPausesAgain)
{
	const test::RestoreService<Locator::time> restoreClock;
	auto& clock = Locator::time::emplace<TimeSystem>();
	clock.Pause(true);
	const auto turn = clock.Turn();
	EditorSystem editor;

	editor.StepTurn();
	EXPECT_FALSE(clock.IsPaused());
	EXPECT_TRUE(editor.IsStepping());

	// No turn played yet: still stepping
	editor.Update(16ms);
	EXPECT_FALSE(clock.IsPaused());

	clock.StartTurn();
	ASSERT_EQ(clock.Turn(), turn + 1);
	editor.Update(16ms);
	EXPECT_TRUE(clock.IsPaused());
	EXPECT_FALSE(editor.IsStepping());
}
} // namespace
