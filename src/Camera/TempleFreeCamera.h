/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>

#include <algorithm>

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The free camera of the editor's cheat in the temple. While the cheat is on, the player flies the camera through the
/// rooms. The arrow keys move the camera forwards, backwards, left and right. The middle button, held while the mouse
/// moves, turns the camera about its own place: up and down, left and right. The camera shows the temple only on this
/// computer. It changes no state of the game, and it sends no command of a turn.
namespace openblack::temple_free_camera
{

/// The facts that decide whether the free camera has the view of the temple
struct Gate
{
	/// The player turned on the cheat in the editor of this computer
	bool cheatOn {false};
	/// The player is in the temple. The front end of a network game, which shows the temple, does not count.
	bool inTemple {false};
	/// The camera of the room has the player: its path into the room ended, and it does not go through a door
	bool roomCameraInControl {false};
};

/// Whether the free camera has the view. When it does not, the temple's own camera has the view as it has with no cheat.
[[nodiscard]] constexpr bool HasControl(const Gate& gate) noexcept
{
	return gate.cheatOn && gate.inTemple && gate.roomCameraInControl;
}

/// The speed of the arrow keys, in the temple's units each second
inline constexpr float k_MoveSpeed = 20.0f;
/// The turn of the camera for each pixel that the mouse moves while the middle button is held, in radians
inline constexpr float k_TurnPerPixel = 0.005f;
/// The camera looks up or down at most by this angle, in radians. It stops short of straight up and straight down, so
/// that the view does not turn over.
inline constexpr float k_PitchLimit = 1.48f;
/// The distance from the camera to the point that it looks at
inline constexpr float k_FocusDistance = 10.0f;

/// Where the camera is, and the way that it looks: a heading about the vertical and a pitch up from the horizontal, in
/// radians. A heading of 0 looks along z, and a larger heading turns the view towards x.
struct View
{
	glm::vec3 origin {0.0f};
	float heading {0.0f};
	float pitch {0.0f};
};

/// What the player does in a frame
struct Input
{
	bool forwards {false};
	bool backwards {false};
	bool left {false};
	bool right {false};
	/// The middle button is held
	bool steer {false};
	/// The movement of the mouse in the frame, in pixels: x to the right, y down the screen
	glm::vec2 mouse {0.0f};
	float seconds {0.0f};
};

/// The way that the camera looks, one unit long
[[nodiscard]] inline glm::vec3 Forward(const View& view) noexcept
{
	const float across = std::cos(view.pitch);
	return {std::sin(view.heading) * across, std::sin(view.pitch), std::cos(view.heading) * across};
}

/// The way to the right of the view, level with the floor
[[nodiscard]] inline glm::vec3 Right(const View& view) noexcept
{
	return {std::cos(view.heading), 0.0f, -std::sin(view.heading)};
}

/// The point that the camera looks at
[[nodiscard]] inline glm::vec3 Focus(const View& view) noexcept
{
	return view.origin + (Forward(view) * k_FocusDistance);
}

/// The view from a camera at origin that looks at focus. The pitch stays in the limit.
[[nodiscard]] inline View ViewOf(glm::vec3 origin, glm::vec3 focus) noexcept
{
	const auto way = focus - origin;
	const float across = std::sqrt((way.x * way.x) + (way.z * way.z));
	const bool straight = across == 0.0f && way.y == 0.0f;
	return {
	    .origin = origin,
	    .heading = straight ? 0.0f : std::atan2(way.x, way.z),
	    .pitch = straight ? 0.0f : std::clamp(std::atan2(way.y, across), -k_PitchLimit, k_PitchLimit),
	};
}

/// The view after a frame. The middle button turns the camera first, and then the arrow keys move it. Forwards moves
/// along the way that the camera looks, also up and down. Left and right move level with the floor.
[[nodiscard]] inline View Step(const View& now, const Input& input) noexcept
{
	auto next = now;
	if (input.steer)
	{
		next.heading += input.mouse.x * k_TurnPerPixel;
		next.pitch = std::clamp(next.pitch - (input.mouse.y * k_TurnPerPixel), -k_PitchLimit, k_PitchLimit);
	}
	const float along = (input.forwards ? 1.0f : 0.0f) - (input.backwards ? 1.0f : 0.0f);
	const float side = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f);
	const auto move = (Forward(next) * along) + (Right(next) * side);
	if (along != 0.0f || side != 0.0f)
	{
		next.origin += glm::normalize(move) * (k_MoveSpeed * input.seconds);
	}
	return next;
}

} // namespace openblack::temple_free_camera
