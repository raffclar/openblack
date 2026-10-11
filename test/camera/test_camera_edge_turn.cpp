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
#include <optional>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <ECS/Registry.h>
#include <Locator.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
constexpr glm::vec3 k_Origin {1000.0f, 40.0f, 1000.0f};
constexpr glm::vec3 k_Focus {1000.0f, 0.0f, 1060.0f};
constexpr auto k_Frame = 16ms;
/// Pressed on the right-hand side of the screen, where dragging turns the camera
constexpr glm::ivec2 k_PressedAt {k_Width - 12, k_Height / 2};

/// No land under any pixel
class NoLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

/// A real mouse, as the game reads it: the frame reads where the pointer was as it began, and the mouse's movement
/// arrives after that, so the pointer itself is already further on when the camera looks at it. Putting the cursor
/// somewhere moves the pointer there.
class RealMouse final: public MockAction
{
public:
	/// A new frame: the frame reads the pointer where it is, then the mouse moves on by so much
	void NextFrame(glm::ivec2 movement)
	{
		_atFrameStart = _pointer;
		_pointer += movement;
		_movement = movement;
	}
	void SetHeld(bool held) { _held = held; }
	[[nodiscard]] int Warps() const { return _warps; }

	[[nodiscard]] bool GetBindable(input::BindableActionMap actions) const override
	{
		return _held && (static_cast<uint64_t>(actions) & static_cast<uint64_t>(input::BindableActionMap::MOVE)) != 0;
	}
	[[nodiscard]] glm::uvec2 GetMousePosition() const override { return glm::uvec2(_atFrameStart); }
	[[nodiscard]] glm::ivec2 GetPointerPosition() const override { return _pointer; }
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return _movement; }
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const override
	{
		return {glm::vec3(1000.0f, 0.0f, 1040.0f), std::nullopt};
	}
	void WarpCursor(glm::ivec2 position) override
	{
		_pointer = position;
		++_warps;
	}

private:
	glm::ivec2 _atFrameStart {k_PressedAt};
	glm::ivec2 _pointer {k_PressedAt};
	glm::ivec2 _movement {0, 0};
	bool _held {false};
	int _warps {0};
};

/// The way the camera looks across the land, in radians round the up axis
float Heading(const CameraModel& model)
{
	const auto look = glm::xz(model.GetTargetFocus() - model.GetTargetOrigin());
	return std::atan2(look.x, look.y);
}

float Difference(float after, float before)
{
	auto turn = after - before;
	while (turn > glm::pi<float>())
	{
		turn -= glm::two_pi<float>();
	}
	while (turn < -glm::pi<float>())
	{
		turn += glm::two_pi<float>();
	}
	return turn;
}

class EdgeTurn: public testing::Test
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
		_mouse = new RealMouse();
		Locator::gameActionSystem::reset<MockAction>(_mouse);

		_camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f);
		_camera.SetOrigin(k_Origin).SetFocus(k_Focus);
		_camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_Origin, k_Focus));
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

	/// One frame of the game with the mouse moving by so much
	void Step(glm::ivec2 movement)
	{
		_mouse->NextFrame(movement);
		_camera.HandleActions(k_Frame);
		_camera.Update(k_Frame);
	}

	Camera _camera;
	NoLandPicking* _picking = nullptr;
	RealMouse* _mouse = nullptr;
};
} // namespace

TEST_F(EdgeTurn, ALongDragDownTheSideOfTheScreenKeepsTurningTheCamera)
{
	// The cursor rests at the side of the screen, which offers turning, then the land is gripped there
	for (int frame = 0; frame < 5; ++frame)
	{
		Step({0, 0});
	}
	_mouse->SetHeld(true);
	Step({0, 0});

	// The mouse is dragged steadily down: every frame of it turns the camera on, not only the first ones
	int turningFrames = 0;
	float turned = 0.0f;
	constexpr int k_DragFrames = 60;
	for (int frame = 0; frame < k_DragFrames; ++frame)
	{
		const auto before = Heading(_camera.GetModel());
		Step({0, 4});
		const auto turn = Difference(Heading(_camera.GetModel()), before);
		turned += turn;
		if (frame >= 10 && std::abs(turn) > glm::radians(0.05f))
		{
			++turningFrames;
		}
	}
	// The cursor was held on the ring all along
	EXPECT_GT(_mouse->Warps(), k_DragFrames / 2);
	// Turning after the drag was decided, frame after frame
	EXPECT_GE(turningFrames, k_DragFrames - 10 - 5);
	// About 240 pixels round the ring of an 800 by 600 screen is a good part of a quarter turn
	EXPECT_GT(std::abs(turned), glm::radians(30.0f));
}

TEST_F(EdgeTurn, TheCameraStopsTurningWhenTheMouseStops)
{
	for (int frame = 0; frame < 5; ++frame)
	{
		Step({0, 0});
	}
	_mouse->SetHeld(true);
	for (int frame = 0; frame < 30; ++frame)
	{
		Step({0, 4});
	}
	// Held still, the cursor stays on the ring and the camera settles where the drag left it
	for (int frame = 0; frame < 60; ++frame)
	{
		Step({0, 0});
	}
	const auto settled = Heading(_camera.GetModel());
	for (int frame = 0; frame < 20; ++frame)
	{
		Step({0, 0});
	}
	EXPECT_NEAR(Difference(Heading(_camera.GetModel()), settled), 0.0f, glm::radians(0.1f));
}
