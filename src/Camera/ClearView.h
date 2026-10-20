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

#include <bit>

#include <glm/vec3.hpp>

/// The clear view: while the player holds the zoom key and the rotate key together, the world camera eases in over half
/// a second to a close view of the point under the hand, or of the thing under the hand. It looks from the same heading,
/// from a short distance and a pitch between its own and straight down. When the player releases a key, it eases back.
namespace openblack::clear_view
{

/// A camera farther than this from what it looks at takes the far orbit
inline constexpr float k_FarDistance = std::bit_cast<float>(0x43960000u);
/// A camera nearer than this takes the near orbit; this is also the far orbit's radius
inline constexpr float k_NearDistance = std::bit_cast<float>(0x42480000u);
inline constexpr float k_FarRadius = k_NearDistance;
inline constexpr float k_NearRadius = std::bit_cast<float>(0x41200000u);
inline constexpr float k_MiddleRadius = std::bit_cast<float>(0x41700000u);
/// The pitch moves this far towards straight down before it takes a quarter
inline constexpr float k_PitchShift = std::bit_cast<float>(0x4016CBE4u);
inline constexpr float k_PitchShare = std::bit_cast<float>(0x3E800000u);
/// The view starts and stops the orbit below this share of the ease
inline constexpr float k_Starts = 0.01f;

/// The radius of the orbit, by the distance from the camera to what it looks at
[[nodiscard]] float OrbitRadius(float distance);
/// The pitch of the orbit, from the pitch of the camera
[[nodiscard]] float OrbitPitch(float pitch);

/// The orbit that the clear view eases to, kept from the frame that it starts
struct Orbit
{
	/// The camera that the view eases from
	glm::vec3 startOrigin {0.0f};
	glm::vec3 startFocus {0.0f};
	float radius {0.0f};
	float heading {0.0f};
	float pitch {0.0f};
};

/// The orbit from the camera as the clear view starts
[[nodiscard]] Orbit Start(const glm::vec3& origin, const glm::vec3& focus);

/// The camera at a share of the ease from the start to the orbit round a point on the land
struct View
{
	glm::vec3 origin;
	glm::vec3 focus;
};
[[nodiscard]] View Eased(const Orbit& orbit, const glm::vec3& centre, float share);

} // namespace openblack::clear_view
