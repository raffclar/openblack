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

/// The steps that one frame takes
struct Steps
{
	uint32_t count {0};
	/// For each step, the part of the frame at which it falls: 0 at the start of the frame, 1 at its end. Input that
	/// moves in the frame is put at the place between the last frame and this frame that this part gives.
	std::array<float, k_MostSteps> points {};

	/// A frame that takes one step at its end, as each frame did before the steps
	[[nodiscard]] static constexpr Steps One() { return {.count = 1, .points = {1.0f}}; }
	[[nodiscard]] float Point(uint32_t step) const { return points.at(step); }
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
};

/// The place between two places at a part of the way from the first to the second
template <typename T>
[[nodiscard]] constexpr T Between(const T& from, const T& to, float part)
{
	return from + (to - from) * part;
}

} // namespace openblack::input_steps
