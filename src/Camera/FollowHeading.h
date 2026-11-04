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

#include <glm/vec3.hpp>

/// The maths of the camera that follows a thing and turns with the way it faces, free of the game so that it can be
/// tested alone. The camera looks at the middle of the thing from behind the way the thing faces, plus a heading that
/// turns by a step each frame. While a creature turns too fast, the camera keeps its last heading. Only a saved game of
/// the original game gives this camera: no part of play makes it.
namespace openblack::follow_heading
{

/// A full turn, as the game holds it
inline constexpr float k_FullTurn = 6.28318548f;
/// The camera looks down at least this steeply, a hundredth of a full turn
inline constexpr float k_LowestPitch = 0.0628318563f;
/// The glides of the camera take two seconds as it starts and one second after two seconds
inline constexpr float k_StartSeconds = 2.0f;
inline constexpr float k_EndSeconds = 1.0f;
inline constexpr float k_BlendSeconds = 2.0f;

/// The values of the camera that a saved game keeps
struct Settings
{
	/// The heading that the camera adds to the way the thing faces. Each frame it turns by the speed.
	float heading {0.0f};
	float pitch {0.0f};
	float distance {0.0f};
	/// The turn of the added heading in each frame
	float headingSpeed {0.0f};
};

/// What the camera knows of its thing in a frame
struct Thing
{
	/// Where the thing is, raised by half its height
	glm::vec3 point {0.0f};
	/// The way the thing faces, as the camera reads it
	float facing {0.0f};
	/// The thing turns too fast for the camera to follow smoothly: the camera keeps its last heading
	bool turnsTooFast {false};
};

/// Where the camera stands and looks after a frame, and how long its glides take
struct View
{
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
	float seconds {0.0f};
};

/// The plan actions of a creature during which it turns too fast for this camera
[[nodiscard]] bool TurnsTooFast(uint32_t planAction);

/// One frame of the camera, some seconds after it started. The added heading turns by its speed. The heading of the
/// camera is the held heading, which a thing that does not turn too fast changes first. All the cameras of this kind
/// share the held heading.
[[nodiscard]] View Step(Settings& settings, const Thing& thing, float secondsSinceStart, float& heldHeading);

/// The seconds that the glides take, some seconds after the camera started
[[nodiscard]] float GlideSeconds(float secondsSinceStart);

} // namespace openblack::follow_heading
