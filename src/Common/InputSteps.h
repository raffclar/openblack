/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <chrono>

#include <glm/vec2.hpp>

/// The fixed steps of the player's input. The game moved the hand and the camera, and weighed strokes and slaps, once a
/// frame at the 60 Hz screen refresh. openblack does these in steps of a sixtieth of a second at any frame rate: a frame
/// takes as many steps as its time owes, from none to a few. So the same mouse movement gives the same strokes, throws
/// and camera movement at 30, 60, 144 or 300 frames a second, and at 60 frames a second each frame takes one step.
namespace openblack::input_steps
{

constexpr uint32_t k_StepsPerSecond = 60;
constexpr float k_StepSeconds = 1.0f / static_cast<float>(k_StepsPerSecond);
constexpr float k_StepMs = 1000.0f / static_cast<float>(k_StepsPerSecond);
constexpr std::chrono::microseconds k_StepTime {16'667};
/// A frame takes no more steps than this, a tenth of a second. The time of a longer frame after that is not used.
constexpr uint32_t k_MostSteps = 6;

/// The whole milliseconds of the step with this number since the clock started: 16, 17 and 17 again and again, so that
/// 60 steps take exactly a second. The camera counts whole milliseconds, as the game did for the time of each frame.
[[nodiscard]] constexpr std::chrono::milliseconds StepTime(uint64_t step)
{
	return std::chrono::milliseconds(
	    static_cast<int64_t>(((step + 1) * 1000 / k_StepsPerSecond) - (step * 1000 / k_StepsPerSecond)));
}

/// The steps that one frame takes
struct Steps
{
	uint32_t count {0};
	/// For each step, the part of the frame at which it falls: 0 at the start of the frame, 1 at its end. Input that
	/// moves in the frame is put at the place between the last frame and this frame that this part gives.
	std::array<float, k_MostSteps> points {};
	/// For each step, its whole milliseconds (see StepTime)
	std::array<std::chrono::milliseconds, k_MostSteps> times {};

	/// A frame that takes one step at its end, as each frame did before the steps
	[[nodiscard]] static constexpr Steps One() { return {.count = 1, .points = {1.0f}, .times = {StepTime(0)}}; }
	[[nodiscard]] float Point(uint32_t step) const { return points.at(step); }
	[[nodiscard]] std::chrono::milliseconds Time(uint32_t step) const { return times.at(step); }
};

/// The clock that counts the steps that each frame owes
class Clock
{
public:
	/// A frame of this length goes by: the steps that it owes
	[[nodiscard]] Steps Advance(std::chrono::microseconds frame);
	/// The clock starts again from nothing
	void Restart();

private:
	/// The clock counts in sixtieths of a microsecond, so that a step is a whole number of 1000000 counts
	static constexpr int64_t k_CountsPerStep = 1'000'000;
	/// A step can come up to a millisecond early, so that frames of 16 and 17 milliseconds at 60 a second each take one
	/// step at their end. The clock keeps what a step took early and gives it back on the next steps.
	static constexpr int64_t k_EarlyCounts = 60'000;
	int64_t _owed {0};
	/// The count of the steps since the clock started
	uint64_t _taken {0};
};

/// The mouse in one frame, as the camera reads it
struct FrameMouse
{
	/// Where the cursor was at the end of the last frame, and where it is now
	glm::ivec2 from {0, 0};
	glm::ivec2 to {0, 0};
	/// The game put the cursor somewhere in this frame: each step reads the cursor where it is now
	bool jumped {false};
	/// The movement of the mouse, and its movement that the sides of the screen do not stop
	glm::ivec2 delta {0, 0};
	glm::ivec2 pushDelta {0, 0};
	/// The notches of the wheel, and the actions that the wheel pressed
	float wheel {0.0f};
	uint64_t wheelActions {0};
};

/// The part of the mouse that one step of the input takes
struct StepMouse
{
	glm::ivec2 position {0, 0};
	glm::ivec2 delta {0, 0};
	glm::ivec2 pushDelta {0, 0};
	float wheel {0.0f};
	uint64_t wheelActions {0};
};

/// Divides the mouse of each frame across the steps that the frame takes. The movement goes to each step by the part
/// of the frame up to the step, so that the steps of a frame and the steps of the frames before give the full movement
/// in whole pixels. The cursor of a step is between the last frame's and this frame's at the step's part. The wheel and
/// the actions that it pressed go to the first step. A frame with no step keeps its movement and its wheel for the next
/// step.
class MouseSplit
{
public:
	[[nodiscard]] std::array<StepMouse, k_MostSteps> Split(const FrameMouse& frame, const Steps& steps);
	/// Forgets the movement and the wheel that no step took
	void Restart();

private:
	glm::ivec2 _delta {0, 0};
	glm::ivec2 _pushDelta {0, 0};
	float _wheel {0.0f};
	uint64_t _wheelActions {0};
};

/// The place between two places at a part of the way from the first to the second
template <typename T>
[[nodiscard]] constexpr T Between(const T& from, const T& to, float part)
{
	return from + (to - from) * part;
}

} // namespace openblack::input_steps
