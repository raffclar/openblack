/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// A debugging aid for the player's hand: off unless asked for, it keeps the last frames of where the hand was put and
/// by which part of the frame, where it was drawn and where it showed on the screen, so that a jump of the hand that
/// lasts a frame or two can be found and blamed on what moved it.
namespace openblack::hand_trace
{

/// One part of the frame setting the hand's position, in the order they ran
struct Write
{
	std::string_view writer;
	glm::vec3 position {0.0f};
};

/// The most writes a frame keeps; the rest are counted but not kept
constexpr size_t k_MaxWrites = 24;

struct Frame
{
	uint32_t frame {0};
	/// The frame's real time, in seconds
	float seconds {0.0f};
	std::array<Write, k_MaxWrites> writes {};
	uint8_t writeCount {0};
	uint8_t writesLost {0};
	/// The cursor in window pixels
	glm::vec2 cursor {0.0f};
	/// Where the hand was put before its drawn fade, and where it is drawn
	glm::vec3 placed {0.0f};
	glm::vec3 drawn {0.0f};
	/// The middle of its fingertips as posed, which is where the hand shows, and that point on the screen
	glm::vec3 visible {0.0f};
	std::optional<glm::vec2> screen;
	/// Its wrist as posed, and that on the screen: a turn of the hand about its fingertips swings the wrist
	glm::vec3 wrist {0.0f};
	std::optional<glm::vec2> wristScreen;
	/// What it is doing, as far as its drawing goes (0 free, 1 pulling, 2 holding, 3 scooping)
	uint8_t stage {0};
	bool busy {false};
	std::optional<uint32_t> held;
	/// Where what it holds is drawn
	std::optional<glm::vec3> heldAt;
	/// What the cursor picked, which the hand goes by: the object, the land, and the point on a thing it rests on
	std::optional<uint32_t> picked;
	std::optional<glm::vec3> cursorLand;
	std::optional<glm::vec3> restOnThing;

	[[nodiscard]] std::span<const Write> Writes() const { return {writes.data(), writeCount}; }
};

/// The trace: a ring of the last frames, recorded only while enabled
class HandTrace
{
public:
	/// Starts keeping the last capacity frames, forgetting any kept before
	void Enable(size_t capacity);
	void Disable();
	[[nodiscard]] bool Enabled() const { return _capacity > 0; }

	/// A new frame starts, its writes to come
	void BeginFrame(uint32_t frame, float seconds);
	/// Some part of the frame put the hand at a position
	void Note(std::string_view writer, glm::vec3 position);
	/// The frame being recorded, to fill in what it ends with; none while disabled or between frames
	[[nodiscard]] Frame* Current();
	/// The frame is done and kept
	void EndFrame();

	/// The frames kept, oldest first
	[[nodiscard]] std::vector<Frame> Frames() const;
	[[nodiscard]] size_t Capacity() const { return _capacity; }

private:
	size_t _capacity {0};
	std::vector<Frame> _ring;
	size_t _next {0};
	size_t _count {0};
	std::optional<Frame> _current;
};

/// A frame where the hand showed further from where it showed the frame before than the cursor moved, by more than
/// a number of pixels
struct Jump
{
	/// Index into the frames given
	size_t index {0};
	uint32_t frame {0};
	/// How far the hand moved on the screen, and how far beyond the cursor's move
	float pixels {0.0f};
	float beyondCursor {0.0f};
	/// Whether it came back within a few frames to near where it was before the jump: a blink rather than a move
	bool cameBack {false};
	/// The last part of the frame that moved the hand, if any
	std::string_view writer;
};

/// The jumps among frames in order: a frame's hand (its fingertips or its wrist, whichever moved more) moving on the
/// screen more than threshold pixels further than the
/// cursor did; one coming back within returnWithin frames to within threshold of where it had been is marked so
[[nodiscard]] std::vector<Jump> FindJumps(std::span<const Frame> frames, float threshold, size_t returnWithin);

} // namespace openblack::hand_trace
