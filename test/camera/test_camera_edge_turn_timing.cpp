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

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include <SDL.h>
#include <SDL_events.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <ECS/Registry.h>
#include <ECS/Systems/HandSystemInterface.h>
#include <Input/GameActionMap.h>
#include <Inspector.h>
#include <Inspector/GameInput.h>
#include <Inspector/InputControl.h>
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
constexpr int k_DragFrames = 60;
constexpr int k_StepDown = 4;

/// The 800 by 600 window of the camera's mocks, which the inspector's input can name
class InputWindow final: public windowing::WindowingInterface
{
public:
	[[nodiscard]] void* GetHandle() const override { return nullptr; }
	[[nodiscard]] NativeHandles GetNativeHandles() const override { return {}; }
	[[nodiscard]] uint32_t GetID() const override { return 0; }
	[[nodiscard]] glm::ivec2 GetSize() const override { return {k_Width, k_Height}; }
	[[nodiscard]] float GetAspectRatio() const override { return static_cast<float>(k_Width) / static_cast<float>(k_Height); }
	[[nodiscard]] bool IsMinimised() const override { return false; }
	[[nodiscard]] bool IsWindowed() const override { return true; }
	WindowingInterface& SetDisplayMode(windowing::DisplayMode /*mode*/) override { return *this; }
};

/// No land under any pixel
class NoLand final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

/// The player's hand over the land in front of the camera
class StillHand final: public ecs::systems::HandSystemInterface
{
public:
	bool Initialize() noexcept override { return true; }
	[[nodiscard]] std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept override
	{
		return {entt::null, entt::null};
	}
	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept override
	{
		return {glm::vec3(1000.0f, 0.0f, 1040.0f), std::nullopt};
	}
	void UpdateAlignmentMorph(std::optional<map_coords::MapCoords> /*picked*/, bool /*holdPoint*/) override {}
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

/// The world camera turned by dragging the land down the side of the screen, driven by the inspector's input through
/// the game's own action map and the window's event queue, frame by frame as the game runs: the input due, the action
/// map's frame, the scripted mouse pumped, the events, then the camera
class EdgeTurnTiming: public testing::TestWithParam<std::string>
{
protected:
	void SetUp() override
	{
		ASSERT_EQ(SDL_InitSubSystem(SDL_INIT_EVENTS), 0) << SDL_GetError();
		SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::windowing::emplace<InputWindow>();
		Locator::handSystem::emplace<StillHand>();
		auto* picking = new NoLand();
		picking->camera = &_camera;
		Locator::pickingSystem::reset<MockPickingSystem>(picking);
		_actions = &static_cast<input::GameActionMap&>(Locator::gameActionSystem::emplace<input::GameActionMap>());
		auto provider = std::make_unique<inspector::InputProvider>(_input);
		_provider = provider.get();
		_inspector.Add(std::move(provider));

		_camera.SetProjectionMatrixPerspective(70.0f, Locator::windowing::value().GetAspectRatio(), 1.0f, 65536.0f);
		_camera.SetOrigin(k_Origin).SetFocus(k_Focus);
		_camera.SetModel(std::make_unique<DefaultWorldCameraModel>(k_Origin, k_Focus));
	}

	void TearDown() override
	{
		Locator::gameActionSystem::reset();
		Locator::pickingSystem::reset();
		Locator::handSystem::reset();
		Locator::windowing::reset();
		Locator::terrainSystem::reset();
		Locator::audio::reset();
		Locator::entitiesRegistry::reset();
		SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
		SDL_QuitSubSystem(SDL_INIT_EVENTS);
	}

	void Ask(const std::string& line)
	{
		const auto decoded = inspector::DecodeRequest(line);
		ASSERT_TRUE(std::holds_alternative<inspector::Request>(decoded));
		const auto answer = _inspector.Answer(std::get<inspector::Request>(decoded));
		ASSERT_TRUE(answer.Ok()) << line << ": " << answer.error;
	}

	void Step()
	{
		_provider->Frame(++_frame);
		_actions->Frame();
		_actions->PumpScriptedPointer();
		SDL_Event event;
		while (SDL_PollEvent(&event) != 0)
		{
			_actions->ProcessEvent(event);
		}
		_camera.HandleActions(k_Frame);
		_camera.Update(k_Frame);
	}

	Camera _camera;
	input::GameActionMap* _actions {nullptr};
	inspector::GameInput _input;
	inspector::Inspector _inspector;
	inspector::InputProvider* _provider {nullptr};
	uint64_t _frame {0};
};

} // namespace

// The land gripped at the side of the screen and dragged steadily down keeps turning the camera, frame after frame,
// with the pointer reaching the game as a real mouse's does and as recordings made before did. With a real mouse the
// camera read the pointer a pump late and put the cursor back on its ring before reading the mouse's move, so it only
// turned on the first frame; the inspector's earlier timing hid that
TEST_P(EdgeTurnTiming, ALongDragDownTheSideOfTheScreenKeepsTurningTheCamera)
{
	const auto& timing = GetParam();
	Ask(fmt::format(R"({{"query": "input.pointer", "params": {{"screen": [{}, {}], "timing": "{}"}}}})", k_PressedAt.x,
	                k_PressedAt.y, timing));
	for (int frame = 0; frame < 5; ++frame)
	{
		Step();
	}
	std::string points;
	for (int step = 0; step <= k_DragFrames; ++step)
	{
		points += fmt::format("{}[{}, {}]", step == 0 ? "" : ", ", k_PressedAt.x, k_PressedAt.y + step * k_StepDown);
	}
	Ask(fmt::format(R"({{"query": "input.path", "params": {{"points": [{}], "button": "left", "timing": "{}"}}}})", points,
	                timing));
	Step();

	int turningFrames = 0;
	float turned = 0.0f;
	for (int frame = 0; frame < k_DragFrames; ++frame)
	{
		const auto before = Heading(_camera.GetModel());
		Step();
		const auto turn = Difference(Heading(_camera.GetModel()), before);
		turned += turn;
		if (frame >= 10 && std::abs(turn) > glm::radians(0.05f))
		{
			++turningFrames;
		}
	}
	EXPECT_GE(turningFrames, k_DragFrames - 10 - 5) << timing << " timing turned " << glm::degrees(turned) << " degrees";
	EXPECT_GT(std::abs(turned), glm::radians(30.0f)) << timing << " timing";
}

INSTANTIATE_TEST_SUITE_P(Timings, EdgeTurnTiming, testing::Values("mouse", "immediate"));

namespace
{
/// The camera's hints of turning or tilting where the cursor is, which only the edges and corners of the screen offer
constexpr uint32_t k_EdgeHints = camera_drag::tricon::k_Rotate | camera_drag::tricon::k_Pitch | camera_drag::tricon::k_Top;

using InspectorDrivenStart = EdgeTurnTiming;
} // namespace

// A game an agent drives starts with the pointer in the middle of the window, not in the top left corner where SDL has
// it until the window first has the player's cursor: left idle, the camera offers no edge turn or tilt and stays put,
// and a press before any move grips the land there rather than starting a turn at the corner
TEST_F(InspectorDrivenStart, IdleStartLeavesThePointerInTheMiddleAndTheCameraStill)
{
	_actions->SetInputLockMode(input::LockMode::Locked);
	const auto origin = _camera.GetModel().GetTargetOrigin();
	const auto heading = Heading(_camera.GetModel());
	for (int frame = 0; frame < 60; ++frame)
	{
		Step();
		ASSERT_EQ(_actions->GetPointerPosition(), glm::ivec2(k_Width / 2, k_Height / 2)) << "frame " << frame;
		EXPECT_EQ(_camera.GetModel().GetHandCues().tricons & k_EdgeHints, 0U) << "frame " << frame;
	}
	EXPECT_LT(glm::distance(_camera.GetModel().GetTargetOrigin(), origin), 0.01f);
	EXPECT_NEAR(Difference(Heading(_camera.GetModel()), heading), 0.0f, 1e-4f);

	Ask(R"({"query": "input.button", "params": {"button": "left", "action": "press"}})");
	for (int frame = 0; frame < 30; ++frame)
	{
		Step();
		const auto cues = _camera.GetModel().GetHandCues();
		EXPECT_NE(cues.dragMode, std::optional(camera_drag::DragMode::EdgeRotate)) << "frame " << frame;
		EXPECT_NE(cues.dragMode, std::optional(camera_drag::DragMode::Pitch)) << "frame " << frame;
	}
	EXPECT_EQ(_actions->GetPointerPosition(), glm::ivec2(k_Width / 2, k_Height / 2));
	EXPECT_NEAR(Difference(Heading(_camera.GetModel()), heading), 0.0f, 1e-4f);
	Ask(R"({"query": "input.release", "params": {}})");
}

// What the middle of the window spares the camera: the top left corner offers turning and tilting
TEST_F(InspectorDrivenStart, TheCornerOffersEdgeHints)
{
	_actions->SetInputLockMode(input::LockMode::Locked);
	Ask(R"({"query": "input.pointer", "params": {"screen": [0, 0]}})");
	for (int frame = 0; frame < 5; ++frame)
	{
		Step();
	}
	EXPECT_NE(_camera.GetModel().GetHandCues().tricons & k_EdgeHints, 0U);
}
