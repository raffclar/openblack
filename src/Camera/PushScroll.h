/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <chrono>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Push scrolling: in full screen, when the player pushes the mouse on past a side of the screen, the world camera moves
/// that way and turns, and slows to a stop when the push ends. The player's profile turns it on and off, and the scripts
/// can take it away with the camera's features. Pure, tested on its own and against the game's own code.
///
/// The camera keeps a speed across, forwards and round. A push past the left or right side adds speed across and round,
/// and a push past the top or bottom adds speed forwards and round. Each frame the speed moves and turns the camera, then
/// fades.
///
/// The game measured the push and its turn on the screen's pixels. Here they are measured on the game's default 800x600
/// screen (deviation 91, a case of the screen rule 4): the edge controls' 4:3 area of the screen is laid over it, so a
/// push feels the same on every screen as on the default one, and exactly as the game's there.
namespace openblack::camera_push_scroll
{

/// The screen the push is measured on: the game's default screen
inline constexpr glm::ivec2 k_Screen {800, 600};
/// The camera feature that allows push scrolling, and those that let it move and turn the camera
inline constexpr uint32_t k_FeaturePushScroll = 0x100;
inline constexpr uint32_t k_FeatureMove = 0x08;
inline constexpr uint32_t k_FeatureRotate = 0x02;

/// The mouse as the push reads it
struct Pointer
{
	/// Where the pointer would be if the screen's sides did not hold it: the pointer itself while it is inside the screen,
	/// and beyond a side the place it had plus each movement since
	glm::ivec2 beyond {0, 0};
	/// The pointer, on the push's screen
	glm::ivec2 position {0, 0};
	/// The mouse's own movement this frame
	glm::ivec2 delta {0, 0};
	/// Whether the pointer is off each side of the real screen, across and down
	bool insideX {true};
	bool insideY {true};
};

/// The pointer on the push's screen, from the pointer on the real screen. On a screen wider than 4:3, the strip down the
/// middle that it has over a 4:3 screen is folded away, as for the other edge controls.
[[nodiscard]] glm::ivec2 PushScreenPosition(glm::ivec2 position, glm::ivec2 screenSize);
/// Whether the pointer is away from the real screen's first and last column, and its first and last row
[[nodiscard]] glm::bvec2 Inside(glm::ivec2 position, glm::ivec2 screenSize);

/// Takes the frame's pointer and the mouse's movement. In a window the push never reads beyond the sides.
void Track(Pointer& pointer, glm::ivec2 position, glm::ivec2 delta, glm::bvec2 inside, bool windowed);

/// How fast a push moves the camera, from the heights of the camera and of what it looks at: three times the height
/// between them, at least 60 and at most 2000 (240 while the camera is below what it looks at), halved
[[nodiscard]] float Speed(float cameraHeight, float focusHeight);

/// The push of the frame: past the left or right side by more than an eighth of the screen's width and moving further
/// out, the speed across grows with the movement and the speed round grows with it times the cube of the pointer's
/// height from the middle; past the top or the bottom, the same forwards and round (by the pointer's place across), at
/// 0.87 of the speed. The caller checks that push scrolling is allowed, the land is not gripped and the game is in full
/// screen.
void Push(glm::vec3& speed, const Pointer& pointer, float pushSpeed, float seconds);

/// What the speed does to the camera in a frame
struct Strafe
{
	/// The movement over the land, in the world
	glm::vec3 move {0.0f};
	/// The change of the camera's heading, in radians
	float turn {0.0f};
	/// The camera moved over the land, which the help counts as dragging it
	bool moved {false};
	/// The camera turned by more than a hundredth of the speed round, which the help counts, and which way
	bool turnCounted {false};
	bool turnedPositive {false};
	bool turnedNegative {false};
	/// The speed round turned the camera at all
	bool turned {false};
	/// The camera started to move again from a stop
	bool started {false};
};

/// Moves and turns the camera by the speed over the frame, as the camera's features allow. Without the push scrolling
/// feature the speed stops and nothing moves. `heading` and `pitch` are the camera's, in radians.
[[nodiscard]] Strafe StrafeStep(glm::vec3& speed, float seconds, float heading, float pitch, uint32_t features);

/// The speed fades: quickly while the pointer is inside the screen, slowly while it is held at a side (the turn slower
/// still); a speed that has nearly stopped stops
void Fade(glm::vec3& speed, float seconds, bool inside);

/// The frame's time as the game's camera takes it: whole milliseconds, as seconds
[[nodiscard]] float Seconds(uint32_t milliseconds);

/// Counts real time in whole milliseconds, as the game's clock gives the camera, keeping the parts of a millisecond for
/// the next frames
class MillisecondClock
{
public:
	/// The whole milliseconds this frame adds
	[[nodiscard]] uint32_t Step(std::chrono::microseconds dt);

private:
	std::chrono::microseconds _left {0};
};

} // namespace openblack::camera_push_scroll
