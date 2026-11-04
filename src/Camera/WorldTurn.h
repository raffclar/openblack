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

#include <bit>

#include <glm/vec3.hpp>

/// The turn and the tilt of the player's camera about the point that it turns about. All the steps use the game's own
/// angle maths and the game's order of operations, so that each step gives the same bits as the game.
namespace openblack::world_turn
{

/// The lowest pitch of the camera, -30 degrees
inline constexpr float k_LowestPitch = std::bit_cast<float>(uint32_t {0xBF060A92u});
/// The highest pitch of the camera, seven sixteenths of a half turn
inline constexpr float k_HighestPitch = std::bit_cast<float>(uint32_t {0x3FAFEDE0u});
/// The tilt of one unit of tilt input, in radians
inline constexpr float k_TiltPerInput = std::bit_cast<float>(uint32_t {0x3B03126Fu});

/// Where the camera is and the point that it turns about. The camera looks at the point.
struct View
{
	glm::vec3 origin;
	glm::vec3 turnPoint;
};

/// The pitch inside the limits of the camera. A pitch that is not a number becomes the lowest pitch.
[[nodiscard]] float ClampPitch(float pitch);

/// The heading after a turn: the turn input times a half turn, divided by the width of the screen in pixels
[[nodiscard]] float Turned(float heading, float turnInput, float screenWidth);

/// The pitch after a tilt: the tilt input times the tilt of one unit, less the pitch, inside the limits
[[nodiscard]] float Tilted(float pitch, float tiltInput);

/// The move over the land of one step of the movement keys. The speed grows with the height of the camera. The forwards
/// input moves less when the camera looks down less steeply. The move turns with the heading. The move has no height.
[[nodiscard]] glm::vec3 KeyMove(float heading, float pitch, float speed, float forwards, float strafe);

/// The vector with a length of one. The squares add in the order x, z, y. A vector of zero length stays the same.
[[nodiscard]] glm::vec3 Unit(glm::vec3 vector);

/// The point at a distance from the origin, on the line of sight towards the focus
[[nodiscard]] glm::vec3 AlongSight(glm::vec3 origin, glm::vec3 focus, float distance);

/// One step of a turn or a tilt about the turn point:
/// 1. The camera goes to the distance from the turn point, along the heading and the pitch inside the limits.
/// 2. If the step does not zoom, the camera and the turn point move together. Then the point on the line of sight at the
///    sight distance is the same as before the step.
[[nodiscard]] View TurnAbout(glm::vec3 origin, glm::vec3 focus, glm::vec3 turnPoint, float distance, float sightDistance,
                             float heading, float pitch, bool zoomed);

} // namespace openblack::world_turn
