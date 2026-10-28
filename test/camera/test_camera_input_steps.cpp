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

#include <array>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

// The test makes its own locator
#define LOCATOR_IMPLEMENTATIONS
#include <Audio/AudioManagerNoOp.h>
#include <Camera/Camera.h>
#include <Camera/CameraModel.h>
#include <Camera/CreatureCameraModel.h>
#include <Camera/DefaultWorldCameraModel.h>
#include <Camera/EditorCameraModel.h>
#include <Camera/FightCameraModel.h>
#include <Camera/TempleCameraModel.h>
#include <Common/InputSteps.h>
#include <Common/RandomNumberManagerTesting.h>
#include <ECS/Registry.h>
#include <Locator.h>

#include "scenarios/Mock.h"

using namespace openblack;
using namespace std::chrono_literals;
using input::BindableActionMap;
using input::UnbindableActionMap;

namespace
{

constexpr std::array<int, 4> k_FrameRates {30, 60, 144, 300};

std::chrono::microseconds FrameTime(int framesPerSecond)
{
	return std::chrono::microseconds(1'000'000 / framesPerSecond);
}

/// The player's mouse and keys, as the game's action map gives them, with the mouse divided across the steps
class FakeActions final: public MockAction
{
public:
	[[nodiscard]] bool GetBindable(BindableActionMap action) const override
	{
		auto held = bindable;
		if (_step.has_value())
		{
			held = (held & ~wheelActions) | _stepMice.at(*_step).wheelActions;
		}
		return (held & static_cast<uint64_t>(action)) != 0;
	}
	[[nodiscard]] bool GetUnbindable(UnbindableActionMap action) const override
	{
		return (unbindable & static_cast<uint8_t>(action)) != 0;
	}
	[[nodiscard]] glm::uvec2 GetMousePosition() const override
	{
		return glm::uvec2(_step.has_value() ? _stepMice.at(*_step).position : position);
	}
	[[nodiscard]] glm::ivec2 GetMouseDelta() const override { return _step.has_value() ? _stepMice.at(*_step).delta : delta; }
	[[nodiscard]] glm::ivec2 GetMousePushDelta() const override
	{
		return _step.has_value() ? _stepMice.at(*_step).pushDelta : delta;
	}
	[[nodiscard]] float GetMouseWheelDelta() const override { return _step.has_value() ? _stepMice.at(*_step).wheel : wheel; }
	void SplitInput(const input_steps::Steps& steps) override
	{
		_step.reset();
		_stepMice = _split.Split({.from = lastPosition,
		                          .to = position,
		                          .delta = delta,
		                          .pushDelta = delta,
		                          .wheel = wheel,
		                          .wheelActions = wheelActions},
		                         steps);
	}
	void UseInputStep(std::optional<uint32_t> step) override { _step = step; }

	glm::ivec2 lastPosition {400, 300};
	glm::ivec2 position {400, 300};
	glm::ivec2 delta {0, 0};
	float wheel {0.0f};
	uint64_t wheelActions {0};
	uint64_t bindable {0};
	uint8_t unbindable {0};

private:
	input_steps::MouseSplit _split;
	std::array<input_steps::StepMouse, input_steps::k_MostSteps> _stepMice {};
	std::optional<uint32_t> _step;
};

/// No land under any pixel
class NoLandPicking final: public MockPickingSystem
{
public:
	[[nodiscard]] std::optional<glm::vec2> LandAtPixel(glm::u16vec2 /*screenCoord*/) const override { return std::nullopt; }
};

/// The mouse of a run: where the cursor is at a time, the wheel's notches turned in a frame from one time to the next,
/// and the actions held
struct Mouse
{
	std::function<glm::vec2(float)> cursor = [](float) { return glm::vec2(400.0f, 300.0f); };
	std::function<float(float, float)> wheel = [](float, float) { return 0.0f; };
	uint64_t bindable {0};
	uint8_t unbindable {0};
};

/// The camera's part of the game's frame: the input clock and the mouse of the frame, the first step's mouse, the
/// camera's steps, and the keys of the last step
class Frames
{
public:
	explicit Frames(Camera& camera)
	    : _camera(camera)
	{
		// The cameras report their flights to the game's log
		if (spdlog::get("game") == nullptr)
		{
			spdlog::create<spdlog::sinks::null_sink_st>("game");
		}
		_actions = &static_cast<FakeActions&>(Locator::gameActionSystem::emplace<FakeActions>());
		Locator::windowing::emplace<MockWindowingSystem>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Locator::rng::emplace<RandomNumberManagerTesting>();
		Locator::audio::emplace<audio::AudioManagerNoOp>();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		static_cast<NoLandPicking&>(Locator::pickingSystem::emplace<NoLandPicking>()).camera = &camera;
	}
	Frames(const Frames&) = delete;
	Frames& operator=(const Frames&) = delete;
	~Frames()
	{
		Locator::gameActionSystem::reset();
		Locator::windowing::reset();
		Locator::terrainSystem::reset();
		Locator::pickingSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	/// Frames at a rate until the steps come to a count
	void Run(int framesPerSecond, uint32_t stepCount, const Mouse& mouse)
	{
		const auto frame = FrameTime(framesPerSecond);
		float time = 0.0f;
		while (_steps < stepCount)
		{
			const float end = time + std::chrono::duration<float>(frame).count();
			auto& actions = *_actions;
			actions.lastPosition = actions.position;
			actions.position = glm::ivec2(glm::round(mouse.cursor(end)));
			actions.delta = actions.position - actions.lastPosition;
			actions.wheel = mouse.wheel(time, end);
			actions.wheelActions = actions.wheel > 0.0f ? static_cast<uint64_t>(BindableActionMap::ZOOM_IN) : 0;
			actions.bindable = mouse.bindable | actions.wheelActions;
			actions.unbindable = mouse.unbindable;
			Frame(frame);
			time = end;
		}
	}

	void Frame(std::chrono::microseconds frame)
	{
		auto& actions = *_actions;
		const auto steps = _clock.Advance(frame);
		_steps += steps.count;
		actions.SplitInput(steps);
		if (steps.count > 0)
		{
			actions.UseInputStep(0);
			_camera.HandleActions(std::chrono::duration_cast<std::chrono::microseconds>(steps.Time(0)));
			actions.UseInputStep(std::nullopt);
		}
		_camera.Update(steps);
		if (steps.count > 0)
		{
			actions.UseInputStep(steps.count - 1);
			_camera.HandleKeys(std::chrono::duration_cast<std::chrono::microseconds>(steps.Time(steps.count - 1)));
			actions.UseInputStep(std::nullopt);
		}
	}

	[[nodiscard]] uint32_t Steps() const { return _steps; }

private:
	Camera& _camera;
	FakeActions* _actions {nullptr};
	input_steps::Clock _clock;
	uint32_t _steps {0};
};

/// A model that keeps a record of what each of its calls read
class RecordingModel final: public CameraModel
{
public:
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& /*camera*/) override
	{
		calls += 'U';
		updateTime += dt;
		return std::nullopt;
	}
	void HandleActions(std::chrono::microseconds dt) override
	{
		calls += 'A';
		const auto& actions = Locator::gameActionSystem::value();
		moved += actions.GetMouseDelta();
		wheel += actions.GetMouseWheelDelta();
		positions.emplace_back(actions.GetMousePosition());
		actionsTime += dt;
	}
	void HandleKeys(std::chrono::microseconds dt) override
	{
		calls += 'K';
		keysTime += dt;
	}
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/, float /*rise*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return {}; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return {}; }
	[[nodiscard]] bool HeldInsideTheWorld() const override { return false; }

	std::string calls;
	std::chrono::microseconds updateTime {0};
	std::chrono::microseconds actionsTime {0};
	std::chrono::microseconds keysTime {0};
	glm::ivec2 moved {0, 0};
	float wheel {0.0f};
	std::vector<glm::uvec2> positions;
};

/// A steady movement right and down, from the middle of the screen
const Mouse k_Steady {
    .cursor = [](float t) { return glm::vec2(400.0f + 300.0f * t, 300.0f + 120.0f * t); },
    .wheel = [](float from, float to) { return (from <= 0.25f && 0.25f < to) || (from <= 0.5f && 0.5f < to) ? 1.0f : 0.0f; },
};

} // namespace

TEST(CameraInputSteps, EachStepIsAWholeNumberOfMillisecondsAndSixtyMakeASecond)
{
	EXPECT_EQ(input_steps::StepTime(0), 16ms);
	EXPECT_EQ(input_steps::StepTime(1), 17ms);
	EXPECT_EQ(input_steps::StepTime(2), 17ms);
	std::chrono::milliseconds second {0};
	for (uint64_t step = 0; step < 60; ++step)
	{
		second += input_steps::StepTime(step);
	}
	EXPECT_EQ(second, 1000ms);
	// The clock gives each step its time, from the clock's start
	input_steps::Clock clock;
	const auto steps = clock.Advance(50'000us);
	ASSERT_EQ(steps.count, 3u);
	EXPECT_EQ(steps.Time(0), 16ms);
	EXPECT_EQ(steps.Time(1), 17ms);
	EXPECT_EQ(steps.Time(2), 17ms);
	clock.Restart();
	EXPECT_EQ(clock.Advance(16'667us).Time(0), 16ms);
}

TEST(CameraInputSteps, AtSixtyFramesASecondEachStepTakesTheWholeFrame)
{
	input_steps::MouseSplit split;
	input_steps::Clock clock;
	for (int i = 0; i < 120; ++i)
	{
		const auto steps = clock.Advance(16'667us);
		ASSERT_EQ(steps.count, 1u);
		const input_steps::FrameMouse frame {
		    .from = {i, 0}, .to = {i + 3, -2}, .delta = {3, -2}, .pushDelta = {5, -4}, .wheel = 1.0f, .wheelActions = 4};
		const auto mice = split.Split(frame, steps);
		EXPECT_EQ(mice.at(0).position, frame.to);
		EXPECT_EQ(mice.at(0).delta, frame.delta);
		EXPECT_EQ(mice.at(0).pushDelta, frame.pushDelta);
		EXPECT_EQ(mice.at(0).wheel, 1.0f);
		EXPECT_EQ(mice.at(0).wheelActions, 4u);
	}
}

TEST(CameraInputSteps, AFrameWithNoStepKeepsTheMouseForTheNextStep)
{
	input_steps::MouseSplit split;
	const input_steps::Steps none {};
	std::ignore = split.Split({.delta = {4, 1}, .wheel = 1.0f, .wheelActions = 2}, none);
	std::ignore = split.Split({.delta = {3, 0}, .wheel = -1.0f, .wheelActions = 8}, none);
	const auto mice = split.Split({.from = {10, 10}, .to = {20, 10}, .delta = {10, 0}}, input_steps::Steps::One());
	EXPECT_EQ(mice.at(0).delta, glm::ivec2(17, 1));
	EXPECT_EQ(mice.at(0).wheel, 0.0f);
	EXPECT_EQ(mice.at(0).wheelActions, 10u);
	EXPECT_EQ(mice.at(0).position, glm::ivec2(20, 10));
}

TEST(CameraInputSteps, ASlowFrameDividesItsMouseAcrossItsSteps)
{
	input_steps::MouseSplit split;
	input_steps::Clock clock;
	const auto steps = clock.Advance(50'000us);
	ASSERT_EQ(steps.count, 3u);
	const auto mice = split.Split({.from = {100, 100}, .to = {130, 70}, .delta = {31, -30}, .wheel = 2.0f}, steps);
	EXPECT_EQ(mice.at(0).position, glm::ivec2(110, 90));
	EXPECT_EQ(mice.at(1).position, glm::ivec2(120, 80));
	EXPECT_EQ(mice.at(2).position, glm::ivec2(130, 70));
	EXPECT_EQ(mice.at(0).delta + mice.at(1).delta + mice.at(2).delta, glm::ivec2(31, -30));
	EXPECT_EQ(mice.at(1).delta, glm::ivec2(11, -10));
	// The wheel goes to the first step
	EXPECT_EQ(mice.at(0).wheel, 2.0f);
	EXPECT_EQ(mice.at(1).wheel, 0.0f);
	// A cursor that the game put somewhere is where it is now at each step
	const auto jumped = split.Split({.from = {100, 100}, .to = {300, 300}, .jumped = true}, clock.Advance(33'334us));
	EXPECT_EQ(jumped.at(0).position, glm::ivec2(300, 300));
}

TEST(CameraInputSteps, EachStepReadsItsMouseAndTheKeysInTheOrderOfAFrameAtSixty)
{
	for (const int rate : k_FrameRates)
	{
		Camera camera;
		camera.SetModel(std::make_unique<RecordingModel>());
		auto& model = dynamic_cast<RecordingModel&>(camera.GetModel());
		Frames frames(camera);
		frames.Run(rate, 60, k_Steady);
		const auto steps = frames.Steps();
		std::string expected;
		for (uint32_t i = 0; i < steps; ++i)
		{
			expected += "AUK";
		}
		EXPECT_EQ(model.calls, expected) << rate << " frames a second";
		// Each step goes on by its whole milliseconds, whatever the frame rate
		std::chrono::milliseconds time {0};
		for (uint32_t i = 0; i < steps; ++i)
		{
			time += input_steps::StepTime(i);
		}
		EXPECT_EQ(model.updateTime, time) << rate << " frames a second";
		EXPECT_EQ(model.actionsTime, time) << rate << " frames a second";
		EXPECT_EQ(model.keysTime, time) << rate << " frames a second";
		// The steps take the mouse's movement up to their time, within a pixel, and both notches of the wheel
		const auto seconds = std::chrono::duration<float>(time).count();
		const auto path = glm::vec2(300.0f, 120.0f) * seconds;
		EXPECT_NEAR(static_cast<float>(model.moved.x), path.x, 1.5f) << rate << " frames a second";
		EXPECT_NEAR(static_cast<float>(model.moved.y), path.y, 1.5f) << rate << " frames a second";
		EXPECT_EQ(model.wheel, 2.0f) << rate << " frames a second";
		// Each step reads the cursor where it was at the step's time
		ASSERT_EQ(model.positions.size(), steps);
		float at = 0.0f;
		for (uint32_t i = 0; i < steps; ++i)
		{
			at += std::chrono::duration<float>(input_steps::StepTime(i)).count();
			EXPECT_NEAR(static_cast<float>(model.positions.at(i).x), k_Steady.cursor(at).x, 2.5f)
			    << rate << " frames a second, step " << i;
		}
	}
}

namespace
{
// The Creature Mode camera turned by the cursor keys with Shift for 90 steps, then left to ease
glm::vec3 CreatureCameraAfterTurning(int framesPerSecond, float& yaw)
{
	Camera camera;
	camera.SetOrigin({0.0f, 30.0f, -50.0f}).SetFocus({0.0f, 0.0f, 0.0f});
	camera.SetModel(std::make_unique<CreatureCameraModel>(camera.GetOrigin(), camera.GetFocus(), glm::vec3(0.0f), 10.0f));
	Frames frames(camera);
	Mouse keys;
	keys.bindable = static_cast<uint64_t>(BindableActionMap::MOVE_RIGHT) | static_cast<uint64_t>(BindableActionMap::ROTATE_ON);
	frames.Run(framesPerSecond, 90, keys);
	frames.Run(framesPerSecond, 150, {});
	yaw = dynamic_cast<const CreatureCameraModel&>(camera.GetModel()).GetView().yaw;
	return camera.GetOrigin();
}
} // namespace

TEST(CameraInputSteps, TheCreatureModeCameraTurnsTheSameWayAtAnyFrameRate)
{
	float yawAtSixty = 0.0f;
	const auto atSixty = CreatureCameraAfterTurning(60, yawAtSixty);
	EXPECT_NE(yawAtSixty, 0.0f);
	for (const int rate : k_FrameRates)
	{
		float yaw = 0.0f;
		const auto origin = CreatureCameraAfterTurning(rate, yaw);
		// The steps after the keys differ by at most one step from frame rate to frame rate
		EXPECT_NEAR(yaw, yawAtSixty, std::abs(yawAtSixty) * 0.03f) << rate << " frames a second";
		EXPECT_NEAR(glm::distance(origin, atSixty), 0.0f, glm::length(atSixty) * 0.03f) << rate << " frames a second";
	}
}

namespace
{
// The editor's camera following a thing that has moved, after some steps
glm::vec3 EditorCameraAfter(int framesPerSecond, uint32_t steps)
{
	Camera camera;
	camera.SetOrigin({0.0f, 20.0f, -40.0f}).SetFocus({0.0f, 0.0f, 0.0f});
	auto model = std::make_unique<EditorCameraModel>(EditorCameraModel::Mode::Follow, camera.GetOrigin(), glm::vec3(0.0f));
	model->SetTarget({50.0f, 0.0f, 30.0f}, {0.0f, 1.0f}, 4.0f);
	camera.SetModel(std::move(model));
	Frames frames(camera);
	frames.Run(framesPerSecond, steps, {});
	return camera.GetOrigin();
}
} // namespace

TEST(CameraInputSteps, TheEditorCameraEasesTheSameWayAtAnyFrameRate)
{
	for (const uint32_t steps : {6u, 30u, 60u})
	{
		const auto atSixty = EditorCameraAfter(60, steps);
		for (const int rate : k_FrameRates)
		{
			// A frame rate's last frame can take a step more than the count
			const auto origin = EditorCameraAfter(rate, steps);
			const auto oneMore = EditorCameraAfter(60, steps + 1);
			const auto tolerance = glm::distance(oneMore, atSixty) * 1.01f + 1e-4f;
			EXPECT_LE(glm::distance(origin, atSixty), tolerance) << rate << " frames a second, " << steps << " steps";
		}
	}
}

namespace
{
/// The player's camera of a fight, which reads the turn from the mouse across
class TurningPlayerModel final: public CameraModel
{
public:
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds /*dt*/, const Camera& /*camera*/) override
	{
		return std::nullopt;
	}
	void HandleActions(std::chrono::microseconds /*dt*/) override
	{
		_asked = {.turn = static_cast<float>(Locator::gameActionSystem::value().GetMouseDelta().x), .controlsUsed = true};
	}
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/, float /*rise*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return {}; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return {}; }
	[[nodiscard]] Asked GetAsked() const override { return _asked; }

private:
	Asked _asked;
};

// The fight's camera turned by the mouse for a second, then left to settle
glm::vec3 FightCameraAfterTurning(int framesPerSecond, bool turned = true)
{
	Camera camera;
	camera.SetOrigin({0.0f, 30.0f, -60.0f}).SetFocus({0.0f, 5.0f, 0.0f});
	const fight_view::View start {.origin = camera.GetOrigin(), .focus = camera.GetFocus()};
	auto model = std::make_unique<FightCameraModel>(std::make_unique<TurningPlayerModel>(), start);
	model->SetFighters({.position = {-5.0f, 0.0f, 0.0f}, .radius = 2.0f, .height = 8.0f},
	                   {.position = {5.0f, 0.0f, 0.0f}, .radius = 2.0f, .height = 8.0f});
	camera.SetModel(std::move(model));
	Frames frames(camera);
	// The cursor stays where the movement ends, or where it starts for no movement
	Mouse still;
	const auto rest = k_Steady.cursor(turned ? 1.0f : 0.0f);
	still.cursor = [rest](float) { return rest; };
	if (turned)
	{
		frames.Run(framesPerSecond, 60, k_Steady);
	}
	frames.Run(framesPerSecond, 300, still);
	return camera.GetOrigin();
}
} // namespace

TEST(CameraInputSteps, TheFightCameraTurnsTheSameWayAtAnyFrameRate)
{
	const auto atSixty = FightCameraAfterTurning(60);
	// The mouse turned the view
	EXPECT_GT(glm::distance(atSixty, FightCameraAfterTurning(60, false)), 1.0f);
	for (const int rate : k_FrameRates)
	{
		const auto origin = FightCameraAfterTurning(rate);
		EXPECT_NEAR(glm::distance(origin, atSixty), 0.0f, glm::length(atSixty) * 0.02f) << rate << " frames a second";
	}
}

namespace
{
// The world camera zoomed by the mouse with both buttons held and by two notches of the wheel, then left to settle
glm::vec3 WorldCameraAfterZooming(int framesPerSecond)
{
	Camera camera;
	camera.SetProjectionMatrixPerspective(70.0f, 800.0f / 600.0f, 1.0f);
	camera.SetModel(
	    std::make_unique<DefaultWorldCameraModel>(glm::vec3(1000.0f, 150.0f, 1000.0f), glm::vec3(1100.0f, 0.0f, 1100.0f)));
	camera.SetOrigin({1000.0f, 150.0f, 1000.0f}).SetFocus({1100.0f, 0.0f, 1100.0f});
	Frames frames(camera);
	Mouse zoom = k_Steady;
	zoom.unbindable = static_cast<uint8_t>(UnbindableActionMap::TWO_BUTTON_CLICK);
	frames.Run(framesPerSecond, 60, zoom);
	Mouse still;
	still.cursor = [](float) { return glm::vec2(700.0f, 420.0f); };
	frames.Run(framesPerSecond, 300, still);
	return camera.GetOrigin();
}
} // namespace

TEST(CameraInputSteps, TheWorldCameraZoomsTheSameWayAtAnyFrameRate)
{
	const auto atSixty = WorldCameraAfterZooming(60);
	EXPECT_GT(glm::distance(atSixty, glm::vec3(1000.0f, 150.0f, 1000.0f)), 1.0f);
	for (const int rate : k_FrameRates)
	{
		const auto origin = WorldCameraAfterZooming(rate);
		EXPECT_NEAR(glm::distance(origin, atSixty), 0.0f, 0.5f) << rate << " frames a second";
	}
}

TEST(CameraInputSteps, TheTempleLeansTheSameWayAtAnyFrameRateForADrag)
{
	// The mouse goes down the screen at 240 pixels a second for a second, in the middle of the screen
	const auto leanAt = [](int framesPerSecond) {
		input_steps::Clock clock;
		input_steps::MouseSplit split;
		const auto frame = FrameTime(framesPerSecond);
		float lean = 0.0f;
		float time = 0.0f;
		glm::ivec2 last {400, 100};
		uint32_t steps = 0;
		while (steps < 60)
		{
			time += std::chrono::duration<float>(frame).count();
			const glm::ivec2 now {400, static_cast<int>(std::lround(100.0f + 240.0f * time))};
			const auto frameSteps = clock.Advance(frame);
			const auto mice = split.Split({.from = last, .to = now, .delta = now - last}, frameSteps);
			for (uint32_t i = 0; i < frameSteps.count; ++i)
			{
				const float seconds = std::chrono::duration<float>(frameSteps.Time(i)).count();
				lean += TempleCameraModel::DragLeanStep(static_cast<float>(mice.at(i).delta.y), seconds, 0.0f);
			}
			steps += frameSteps.count;
			last = now;
		}
		return lean;
	};
	const float atSixty = leanAt(60);
	EXPECT_NEAR(atSixty, 240.0f / 60.0f * 0.06f, 0.01f);
	for (const int rate : k_FrameRates)
	{
		EXPECT_NEAR(leanAt(rate), atSixty, atSixty * 0.03f) << rate << " frames a second";
	}
}
