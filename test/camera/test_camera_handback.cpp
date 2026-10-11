/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
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

		_camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f);
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

namespace
{
/// The player holds one camera key throughout
class HoldingAction final: public MockAction
{
public:
	explicit HoldingAction(input::BindableActionMap held)
	    : _held(held)
	{
	}

	// Asked about several actions at once, it answers whether any of them is held
	[[nodiscard]] bool GetBindable(input::BindableActionMap actions) const override
	{
		return (static_cast<uint64_t>(actions) & static_cast<uint64_t>(_held)) != 0;
	}

private:
	input::BindableActionMap _held;
};

/// The land-dragging lesson's help: the land can be dragged and the keys move the camera over it, and the camera tilts
/// itself and keeps to its height over the land
class LessonCameraHelp final: public ecs::systems::CameraHelpSystemInterface
{
public:
	LessonCameraHelp() { _help.SetInterfaceLevel(1); }

	[[nodiscard]] const camera_help::CameraHelp& Get() const override { return _help; }
	[[nodiscard]] camera_help::CameraHelp& Get() override { return _help; }

private:
	camera_help::CameraHelp _help;
};

class MovingAfterHandback: public testing::TestWithParam<input::BindableActionMap>
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
		Locator::gameActionSystem::reset<MockAction>(new HoldingAction(GetParam()));
		Locator::cameraHelpSystem::emplace<LessonCameraHelp>();

		_camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f);
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

TEST_P(MovingAfterHandback, TheKeysMoveTheCameraWithoutTurningIt)
{
	ScriptControlSystem control;
	ASSERT_TRUE(control.StartCameraControl(_camera, {.task = 7, .templeScript = false, .insideTemple = false}, FlatLand));
	ASSERT_TRUE(control.EndCameraControl(_camera, 7));
	const auto& model = _camera.GetModel();

	// The camera first settles to its pitch and height with nothing held, as when the lesson's text is read
	Locator::gameActionSystem::reset<MockAction>(new MockAction());
	for (int frame = 0; frame < 120; ++frame)
	{
		_camera.HandleActions(k_Frame);
		_camera.Update(k_Frame);
	}
	Locator::gameActionSystem::reset<MockAction>(new HoldingAction(GetParam()));
	const auto origin = model.GetTargetOrigin();
	const auto heading = Heading(origin, model.GetTargetFocus());

	for (int frame = 0; frame < 60; ++frame)
	{
		_camera.HandleActions(k_Frame);
		_camera.Update(k_Frame);
		const auto now = Heading(model.GetTargetOrigin(), model.GetTargetFocus());
		EXPECT_GT(glm::dot(now, heading), std::cos(glm::radians(2.0f))) << "frame " << frame;
	}
	// A second of holding a movement key carries the camera several metres over the land
	EXPECT_GT(glm::distance(glm::xz(model.GetTargetOrigin()), glm::xz(origin)), 5.0f);
}

INSTANTIATE_TEST_SUITE_P(MovementKeys, MovingAfterHandback,
                         testing::Values(input::BindableActionMap::MOVE_LEFT, input::BindableActionMap::MOVE_RIGHT,
                                         input::BindableActionMap::MOVE_FORWARDS, input::BindableActionMap::MOVE_BACKWARDS));

namespace
{
/// Flat land at the sea's level under every pixel looking down, or under none once the cursor is over the sky
class FlatLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
	[[nodiscard]] std::optional<glm::vec3> LandUnderPixel(glm::vec3 eye, glm::vec3 nearPoint, bool /*withSea*/) const override
	{
		const auto direction = nearPoint - eye;
		if (overSky || direction.y >= 0.0f)
		{
			return std::nullopt;
		}
		return eye + direction * (-eye.y / direction.y);
	}

	bool overSky {false};
};

/// The player holds the button that drags the land and moves the mouse steadily up the screen, the hand on the land
/// under the cursor until the cursor passes over the sky
class DragUpAction final: public MockAction
{
public:
	explicit DragUpAction(const FlatLandPicking& picking)
	    : _picking(picking)
	{
	}

	[[nodiscard]] bool GetBindable(input::BindableActionMap actions) const override
	{
		return held && (static_cast<uint64_t>(actions) & static_cast<uint64_t>(input::BindableActionMap::MOVE)) != 0;
	}
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return cursor; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return delta; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override
	{
		if (_picking.overSky)
		{
			return {};
		}
		return {glm::vec3(1500.0f, 0.0f, 2100.0f), std::nullopt};
	}

	bool held {false};
	// Near the right of the screen, where the land is dragged without turning when the camera can't turn
	glm::uvec2 cursor {k_Width * 19 / 20, k_Height * 4 / 5};
	glm::ivec2 delta {0, 0};

private:
	const FlatLandPicking& _picking;
};

class DragOffTheLand: public testing::Test
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
		_action = new DragUpAction(*_picking);
		Locator::gameActionSystem::reset<MockAction>(_action);
		Locator::cameraHelpSystem::emplace<LessonCameraHelp>();

		_camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f);
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
	FlatLandPicking* _picking = nullptr;
	DragUpAction* _action = nullptr;
};
} // namespace

// Dragging the land up the beach after the handback, the player can't turn the camera: dragging on past where the land
// is (the cursor over the sky or far off) neither ends the drag nor lets the camera turn itself while it tilts back
TEST_F(DragOffTheLand, DraggingOnPastTheLandDoesNotTurnTheCamera)
{
	ScriptControlSystem control;
	ASSERT_TRUE(control.StartCameraControl(_camera, {.task = 7, .templeScript = false, .insideTemple = false}, FlatLand));
	ASSERT_TRUE(control.EndCameraControl(_camera, 7));
	const auto& model = _camera.GetModel();
	const auto step = [this](int frames) {
		for (int frame = 0; frame < frames; ++frame)
		{
			_camera.HandleActions(k_Frame);
			_camera.Update(k_Frame);
			if (_action->held && _action->cursor.y > 10)
			{
				_action->cursor.y -= 6;
			}
		}
	};
	// The camera first settles to its pitch and height
	step(120);
	const auto heading = Heading(model.GetTargetOrigin(), model.GetTargetFocus());
	const auto expectHeading = [&model, heading](const char* when, int frame) {
		const auto now = Heading(model.GetTargetOrigin(), model.GetTargetFocus());
		EXPECT_GT(glm::dot(now, heading), std::cos(glm::radians(1.0f))) << when << " frame " << frame;
	};

	// Gripping the land and dragging it towards the top of the screen
	_action->held = true;
	_action->delta = {0, -6};
	for (int frame = 0; frame < 40; ++frame)
	{
		step(1);
		expectHeading("on the land", frame);
	}
	// The cursor goes on over the sky with the button still held
	_picking->overSky = true;
	for (int frame = 0; frame < 60; ++frame)
	{
		step(1);
		expectHeading("over the sky", frame);
	}
	// Let go, and the camera settles again
	_action->held = false;
	_action->delta = {0, 0};
	for (int frame = 0; frame < 120; ++frame)
	{
		step(1);
		expectHeading("let go", frame);
	}
}

// Pressed with the cursor already over the sky, the button grips nothing, and moving the mouse doesn't turn the camera
// either
TEST_F(DragOffTheLand, HoldingTheButtonOverTheSkyDoesNotTurnTheCamera)
{
	ScriptControlSystem control;
	ASSERT_TRUE(control.StartCameraControl(_camera, {.task = 7, .templeScript = false, .insideTemple = false}, FlatLand));
	ASSERT_TRUE(control.EndCameraControl(_camera, 7));
	const auto& model = _camera.GetModel();
	for (int frame = 0; frame < 120; ++frame)
	{
		_camera.HandleActions(k_Frame);
		_camera.Update(k_Frame);
	}
	const auto heading = Heading(model.GetTargetOrigin(), model.GetTargetFocus());
	_picking->overSky = true;
	_action->held = true;
	_action->delta = {0, -6};
	for (int frame = 0; frame < 120; ++frame)
	{
		_camera.HandleActions(k_Frame);
		_camera.Update(k_Frame);
		if (_action->cursor.y > 10)
		{
			_action->cursor.y -= 6;
		}
		const auto now = Heading(model.GetTargetOrigin(), model.GetTargetFocus());
		EXPECT_GT(glm::dot(now, heading), std::cos(glm::radians(1.0f))) << "frame " << frame;
	}
}
