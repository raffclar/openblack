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

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Camera/CameraModel.h"
#include "Common/InputSteps.h"
#include "Creature/CreatureTouchStep.h"
#include "ECS/Components/HandOnCreature.h"
#include "Gestures/GestureRecorder.h"
#include "Help/TurnCheer.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{

// Frames at 30, 60, 144 and 300 a second, in microseconds
constexpr std::array<int, 4> k_FrameRates {30, 60, 144, 300};
std::chrono::microseconds FrameTime(int framesPerSecond)
{
	return std::chrono::microseconds(1'000'000 / framesPerSecond);
}

// Runs frames of a frame rate for some seconds, and calls the frame with the frame's start, its length and its steps
template <typename OnFrame>
void RunFrames(int framesPerSecond, float seconds, OnFrame&& onFrame)
{
	input_steps::Clock clock;
	const auto frame = FrameTime(framesPerSecond);
	const auto total = static_cast<int64_t>(seconds * 1'000'000.0f);
	for (int64_t start = 0; start + frame.count() <= total; start += frame.count())
	{
		onFrame(static_cast<float>(start) / 1'000'000.0f, std::chrono::duration<float>(frame).count(), clock.Advance(frame));
	}
}

// Runs frames of a frame rate until their steps come to a count, and calls the frame with its length and its steps
template <typename OnFrame>
void RunSteps(int framesPerSecond, uint32_t count, OnFrame&& onFrame)
{
	input_steps::Clock clock;
	const auto frame = FrameTime(framesPerSecond);
	for (uint32_t steps = 0; steps < count;)
	{
		const auto frameSteps = clock.Advance(frame);
		steps += frameSteps.count;
		onFrame(frame, frameSteps);
	}
}

} // namespace

TEST(InputSteps, EachSecondTakesSixtyStepsAtAnyFrameRate)
{
	for (const int rate : k_FrameRates)
	{
		uint32_t steps = 0;
		RunFrames(rate, 10.0f, [&](float, float, const input_steps::Steps& frameSteps) { steps += frameSteps.count; });
		EXPECT_NEAR(steps, 600u, 1u) << rate << " frames a second";
	}
}

TEST(InputSteps, FramesOfSixteenAndSeventeenMillisecondsTakeOneStepEachAtTheirEnd)
{
	input_steps::Clock clock;
	for (int i = 0; i < 300; ++i)
	{
		const auto frame = std::chrono::milliseconds(i % 3 == 0 ? 16 : 17);
		const auto steps = clock.Advance(frame);
		ASSERT_EQ(steps.count, 1u) << "frame " << i;
		EXPECT_FLOAT_EQ(steps.Point(0), 1.0f);
	}
}

TEST(InputSteps, ASlowFrameTakesTheStepsItOwesAtTheirPlacesInTheFrame)
{
	input_steps::Clock clock;
	const auto steps = clock.Advance(50'000us);
	ASSERT_EQ(steps.count, 3u);
	EXPECT_NEAR(steps.Point(0), 1.0f / 3.0f, 1e-3f);
	EXPECT_NEAR(steps.Point(1), 2.0f / 3.0f, 1e-3f);
	EXPECT_NEAR(steps.Point(2), 1.0f, 1e-3f);
	// A long frame takes the most steps, and the rest of its time is not used
	const auto stall = clock.Advance(1s);
	EXPECT_EQ(stall.count, input_steps::k_MostSteps);
	EXPECT_EQ(clock.Advance(16'667us).count, 1u);
	// A frame of no time takes no step
	EXPECT_EQ(clock.Advance(0us).count, 0u);
}

TEST(InputSteps, FastFramesTakeAStepOnlyWhenOneIsDue)
{
	input_steps::Clock clock;
	uint32_t steps = 0;
	uint32_t framesWithSteps = 0;
	for (int i = 0; i < 300; ++i)
	{
		const auto frame = clock.Advance(3'333us);
		EXPECT_LE(frame.count, 1u);
		steps += frame.count;
		framesWithSteps += frame.count > 0 ? 1 : 0;
	}
	EXPECT_NEAR(steps, 60u, 1u);
	EXPECT_EQ(steps, framesWithSteps);
}

namespace
{
// The cursor's path while the hand holds a creature, in pixels at a time in seconds: still, then up and down, then a
// slow sweep across the body that strokes it, then a fast sweep across the middle of the screen that slaps it hard,
// then a slower sweep back that slaps it gently
glm::vec2 CursorAt(float t)
{
	const auto along = [t](float from, float to, float start, float end) {
		return from + (to - from) * std::clamp((t - start) / (end - start), 0.0f, 1.0f);
	};
	if (t < 2.4f)
	{
		return {640.0f, along(512.0f, 470.0f, 1.0f, 2.4f)};
	}
	if (t < 4.0f)
	{
		return {along(640.0f, 760.0f, 2.4f, 4.0f), 470.0f};
	}
	if (t < 5.0f)
	{
		return {along(760.0f, 470.0f, 4.5f, 4.6f), 470.0f};
	}
	return {along(470.0f, 873.0f, 5.5f, 6.05f), 470.0f};
}

struct TouchRecord
{
	std::vector<std::string> events;
	float sum {0.0f};
	uint32_t steps {0};
};

// The hand holds a fake creature for seven seconds at a frame rate. The body covers the middle of the screen, and its
// parts are bands across the screen.
TouchRecord TouchAt(int framesPerSecond)
{
	using creature_feedback::BodyPart;
	ecs::components::HandOnCreature contact;
	TouchRecord record;
	std::optional<glm::ivec2> lastFrameCursor;
	const auto world = [](glm::vec2 cursor) { return glm::vec3(cursor.x * 0.1f, 0.0f, cursor.y * 0.1f); };
	input_steps::Clock clock;
	const auto frame = FrameTime(framesPerSecond);
	for (int64_t end = frame.count(); end <= 7'000'000; end += frame.count())
	{
		const auto cursor = glm::ivec2(glm::round(CursorAt(static_cast<float>(end) / 1'000'000.0f)));
		const auto steps = clock.Advance(frame);
		const auto before = lastFrameCursor.value_or(cursor);
		for (uint32_t i = 0; i < steps.count; ++i)
		{
			const auto stepCursor = creature_hand::StepCursor(before, cursor, steps, i);
			const bool onBody = stepCursor.x > 420 && stepCursor.x < 860;
			const creature_hand::TouchAnswers answers {
			    .slap =
			        [&](const creature_feedback::Slap& slap) {
				        record.events.push_back("slap " + std::to_string(slap.animation) + (slap.mirrored ? "m" : ""));
				        return true;
			        },
			    .stroke =
			        [&](BodyPart part) {
				        record.events.push_back("stroke " + std::to_string(static_cast<int>(part)));
				        return true;
			        },
			    .nearestPart = [&]() -> std::optional<BodyPart> {
				    return stepCursor.x < 610 ? BodyPart::RightHand : stepCursor.x < 715 ? BodyPart::Groin : BodyPart::LeftHand;
			    },
			    .bruise = [&](uint8_t) { record.events.emplace_back("bruise"); },
			};
			creature_hand::WeighTouchStep(contact,
			                              {
			                                  .stepMs = input_steps::k_StepMs,
			                                  .here = world(glm::vec2(stepCursor)),
			                                  .before = world(contact.lastCursor),
			                                  .onBody = onBody,
			                                  .handHeight = 5.0f,
			                                  .cursor = glm::vec2(stepCursor),
			                                  .screenWidth = 1280.0f,
			                                  .creatureHeight = 10.0f,
			                              },
			                              answers);
			++record.steps;
		}
		lastFrameCursor = cursor;
	}
	record.sum = contact.sum;
	return record;
}
} // namespace

TEST(InputSteps, StrokesAndSlapsWeighTheSameAtAnyFrameRate)
{
	const auto atSixty = TouchAt(60);
	// The path strokes and slaps both hard and gently
	ASSERT_GE(atSixty.events.size(), 3u);
	EXPECT_NE(std::ranges::find(atSixty.events, "slap 167m"), atSixty.events.end());
	EXPECT_NE(std::ranges::find(atSixty.events, "slap 173"), atSixty.events.end());
	EXPECT_TRUE(std::ranges::any_of(atSixty.events, [](const std::string& e) { return e.starts_with("stroke"); }));
	for (const int rate : k_FrameRates)
	{
		const auto record = TouchAt(rate);
		EXPECT_EQ(record.events, atSixty.events) << rate << " frames a second";
		EXPECT_FLOAT_EQ(record.sum, atSixty.sum) << rate << " frames a second";
		EXPECT_NEAR(record.steps, atSixty.steps, 1u) << rate << " frames a second";
	}
}

TEST(InputSteps, AtSixtyFramesASecondEachFrameWeighsOnceAtItsCursor)
{
	// Each frame takes one step at its end, so the step takes the cursor of the frame, as the weighing of each frame did
	input_steps::Clock clock;
	for (int i = 0; i < 120; ++i)
	{
		const auto steps = clock.Advance(16'667us);
		ASSERT_EQ(steps.count, 1u);
		EXPECT_EQ(creature_hand::StepCursor({100, 100}, {137, 90}, steps, 0), glm::ivec2(137, 90));
	}
}

TEST(InputSteps, TheCheeringMeasuresTheSameTurningSpeedAtAnyFrameRate)
{
	// The camera turns at 3 radians a second for half a second, then stands still
	const auto measure = [](int framesPerSecond, uint32_t count) {
		float speed = 0.0f;
		float time = 0.0f;
		help::turn_cheer::SpeedSinceStep since;
		RunSteps(framesPerSecond, count, [&](std::chrono::microseconds frame, const input_steps::Steps& steps) {
			const float seconds = std::chrono::duration<float>(frame).count();
			const float turning = std::clamp(0.5f - time, 0.0f, seconds);
			time += seconds;
			speed = help::turn_cheer::MeasureSpeed(speed, since, 3.0f * turning, seconds, steps.count);
		});
		return speed;
	};
	for (const int rate : k_FrameRates)
	{
		// Turning steadily, and after six steps standing still
		EXPECT_NEAR(measure(rate, 24), 3.0f, 1e-3f) << rate << " frames a second";
		const float still = measure(60, 36);
		EXPECT_NEAR(measure(rate, 36), still, still * 0.02f) << rate << " frames a second";
	}
	// At 60 frames a second the measure moves 0.4 of the way at each frame, as it did once a frame
	float speed = 1.0f;
	help::turn_cheer::SpeedSinceStep since;
	speed = help::turn_cheer::MeasureSpeed(speed, since, 0.0f, 1.0f / 60.0f, 1);
	EXPECT_FLOAT_EQ(speed, help::turn_cheer::SmoothSpeed(1.0f, 0.0f, 1.0f / 60.0f));
}

TEST(InputSteps, TheGesturesSampleTheCursorThirtyTimesASecondAtAnyFrameRate)
{
	for (const int rate : k_FrameRates)
	{
		float sinceSample = 0.0f;
		size_t samples = 0;
		RunFrames(rate, 10.0f, [&](float, float, const input_steps::Steps& steps) {
			samples += gesture::SamplePoints(sinceSample, steps).size();
		});
		EXPECT_NEAR(samples, 300u, 1u) << rate << " frames a second";
	}
}

namespace
{
// A camera model that asks for the same place at each frame, as the world camera does while the player does nothing
class StillTargetModel final: public CameraModel
{
public:
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds /*dt*/, const Camera& /*camera*/) override
	{
		return CameraInterpolationUpdateInfo {
		    .origin = {100.0f, 50.0f, 0.0f}, .focus = {100.0f, 0.0f, 50.0f}, .duration = 400'000us, .eachStep = true};
	}
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/, float /*rise*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return {100.0f, 50.0f, 0.0f}; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return {100.0f, 0.0f, 50.0f}; }
	[[nodiscard]] bool HeldInsideTheWorld() const override { return false; }
};

glm::vec3 CameraOriginAfter(int framesPerSecond, uint32_t count)
{
	Camera camera;
	camera.SetModel(std::make_unique<StillTargetModel>());
	camera.SetOrigin({0.0f, 10.0f, 0.0f}).SetFocus({0.0f, 0.0f, 10.0f});
	RunSteps(framesPerSecond, count,
	         [&](std::chrono::microseconds frame, const input_steps::Steps& steps) { camera.Update(frame, steps); });
	return camera.GetOrigin();
}
} // namespace

TEST(InputSteps, TheCameraEasesTheSameWayAtAnyFrameRate)
{
	for (const uint32_t steps : {6u, 16u, 30u})
	{
		const auto atSixty = CameraOriginAfter(60, steps);
		EXPECT_GT(atSixty.x, 0.0f);
		for (const int rate : k_FrameRates)
		{
			const auto origin = CameraOriginAfter(rate, steps);
			EXPECT_NEAR(glm::distance(origin, atSixty), 0.0f, 1e-3f) << rate << " frames a second, " << steps << " steps";
		}
	}
}
