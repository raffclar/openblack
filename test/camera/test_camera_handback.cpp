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
#include <optional>
#include <tuple>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <ECS/Registry.h>
#include <ECS/Systems/CameraHelpSystemInterface.h>
#include <ECS/Systems/Implementations/ScriptControlSystem.h>
#include <Locator.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;
using openblack::ecs::systems::ScriptControlSystem;

namespace
{
// Where a script left the camera at the end of the land-dragging lesson: looking north-east (29.8 degrees round) and
// 21.6 degrees down
constexpr glm::vec3 k_LessonOrigin {1493.645f, 13.934f, 2091.68f};
constexpr glm::vec3 k_LessonFocus {1502.725f, 6.726f, 2107.511f};
constexpr auto k_Frame = 16ms;

float FlatLand(float /*x*/, float /*z*/)
{
	return 0.0f;
}

/// No land under any pixel
class NoLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

/// The player turns the camera right, from the first frame, or does nothing
class TurningAction final: public MockAction
{
public:
	explicit TurningAction(bool turning)
	    : _turning(turning)
	{
	}

	[[nodiscard]] bool GetBindable(input::BindableActionMap action) const override
	{
		return _turning && action == input::BindableActionMap::ROTATE_RIGHT;
	}

private:
	bool _turning;
};

/// What the land's scripts let the camera do: as it starts, or tilting itself too
class FakeCameraHelp final: public ecs::systems::CameraHelpSystemInterface
{
public:
	explicit FakeCameraHelp(bool autoPitch)
	{
		if (autoPitch)
		{
			_help.features |= camera_help::feature::k_AutoPitch;
		}
	}

	[[nodiscard]] const camera_help::CameraHelp& Get() const override { return _help; }
	[[nodiscard]] camera_help::CameraHelp& Get() override { return _help; }

private:
	camera_help::CameraHelp _help;
};

/// The way the camera looks across the land, as a unit vector
glm::vec2 Heading(glm::vec3 origin, glm::vec3 focus)
{
	return glm::normalize(glm::xz(focus - origin));
}

class CameraHandback: public testing::TestWithParam<std::tuple<bool, bool>>
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<MockWindowingSystem>();
		_picking = new NoLandPicking();
		_picking->camera = &_camera;
		Locator::pickingSystem::reset<MockPickingSystem>(_picking);
		const auto [turning, autoPitch] = GetParam();
		Locator::gameActionSystem::reset<MockAction>(new TurningAction(turning));
		Locator::cameraHelpSystem::emplace<FakeCameraHelp>(autoPitch);

		_camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f, 65536.0f);
		_camera.SetOrigin(k_LessonOrigin).SetFocus(k_LessonFocus);
	}

	void TearDown() override
	{
		Locator::cameraHelpSystem::reset();
		Locator::gameActionSystem::reset();
		Locator::pickingSystem::reset();
		Locator::windowing::reset();
		Locator::terrainSystem::reset();
		Locator::audio::reset();
		Locator::entitiesRegistry::reset();
	}

	Camera _camera;
	NoLandPicking* _picking = nullptr;
};
} // namespace

TEST_P(CameraHandback, ThePlayersCameraLooksTheWayTheScriptLeftIt)
{
	ScriptControlSystem control;
	ASSERT_TRUE(control.StartCameraControl(_camera, {.task = 7, .templeScript = false, .insideTemple = false}, FlatLand));
	ASSERT_TRUE(control.EndCameraControl(_camera, 7));
	const auto& model = _camera.GetModel();
	ASSERT_NE(dynamic_cast<const DefaultWorldCameraModel*>(&model), nullptr);

	// It starts where the script left it, before its first frame
	EXPECT_FLOAT_EQ(glm::distance(model.GetTargetOrigin(), k_LessonOrigin), 0.0f);
	EXPECT_FLOAT_EQ(glm::distance(model.GetTargetFocus(), k_LessonFocus), 0.0f);

	// Over its first frames it keeps facing the same way, turned only by the player's own turning
	const auto heading = Heading(k_LessonOrigin, k_LessonFocus);
	for (int frame = 0; frame < 10; ++frame)
	{
		_camera.HandleActions(k_Frame);
		_camera.Update(k_Frame);
		const auto now = Heading(model.GetTargetOrigin(), model.GetTargetFocus());
		EXPECT_GT(glm::dot(now, heading), std::cos(glm::radians(20.0f))) << "frame " << frame;
	}
}

// With and without the player turning from the first frame, and with and without the camera tilting itself
INSTANTIATE_TEST_SUITE_P(TurningAndTilting, CameraHandback, testing::Combine(testing::Bool(), testing::Bool()));
