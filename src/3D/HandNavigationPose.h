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
#include <cstdint>

#include <optional>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include "Camera/CameraDrag.h"

/// The hand's pose and its up while it moves the camera, from the camera's hints: gripping the land it pans, turning
/// the camera round the edge of the screen, tilting it, and offering to do so with the cursor at the edge
namespace openblack::hand_navigation_pose
{

enum class Pose : uint8_t
{
	/// The hand's ordinary pose
	Idle,
	Grip,
	Rotate,
	Pitch,
	Zoom,
};

/// The hand's pose while it drags the land
[[nodiscard]] inline Pose WhileDragging(bool gripsLand, uint32_t tricons, uint32_t features = camera_drag::k_DefaultFeatures)
{
	namespace tricon = camera_drag::tricon;
	namespace feature = camera_drag::feature;
	if (gripsLand)
	{
		return Pose::Grip;
	}
	if ((tricons & tricon::k_Rotate) != 0)
	{
		return (features & feature::k_Rotate) != 0 ? Pose::Rotate : Pose::Idle;
	}
	if ((tricons & tricon::k_Zoom) != 0)
	{
		return (features & feature::k_Zoom) != 0 ? Pose::Zoom : Pose::Idle;
	}
	if ((tricons & tricon::k_Pitch) != 0 && (features & feature::k_Pitch) != 0)
	{
		return Pose::Pitch;
	}
	return Pose::Idle;
}

/// The hand's pose hovering with nothing under it, where the cursor's place offers turning or tilting the camera; none
/// leaves the hand to its ordinary poses
[[nodiscard]] inline std::optional<Pose> WhileHovering(uint32_t tricons, uint32_t features = camera_drag::k_DefaultFeatures)
{
	namespace tricon = camera_drag::tricon;
	namespace feature = camera_drag::feature;
	if ((tricons & tricon::k_Rotate) != 0)
	{
		return (features & feature::k_Rotate) != 0 ? std::optional(Pose::Rotate) : std::nullopt;
	}
	if ((tricons & tricon::k_Pitch) != 0 && (features & feature::k_Pitch) != 0)
	{
		return Pose::Pitch;
	}
	return std::nullopt;
}

/// How far up the hand leans when it stands towards the camera's focus, as a share of the distance to the focus
constexpr float k_FocusLean = 0.3f;

/// Offering to turn the camera, the hand stands up towards where the camera looks, leaning up by three tenths of the
/// distance. The squares are added y, x, z. At the focus itself there is no way to stand, and the up is zero.
[[nodiscard]] inline glm::vec3 UpTowardsFocus(glm::vec3 from, glm::vec3 focus)
{
	const glm::vec3 towards {focus.x - from.x, focus.y - from.y, focus.z - from.z};
	const float xx = towards.x * towards.x;
	const float zz = towards.z * towards.z;
	const float lean = std::sqrt((towards.y * towards.y + xx) + zz) * k_FocusLean + towards.y;
	if (towards.x == 0.0f && lean == 0.0f && towards.z == 0.0f)
	{
		return {towards.x, lean, towards.z};
	}
	const float share = 1.0f / std::sqrt((lean * lean + xx) + zz);
	return {share * towards.x, share * lean, towards.z * share};
}

/// While the camera turns by the ring around the middle of the screen, the hand stands towards the focus in the same
/// way, but the game adds the squares y, z, x. Only a hand that holds a thing takes this up, through its eased up.
[[nodiscard]] inline glm::vec3 UpTowardsFocusOnRing(glm::vec3 from, glm::vec3 focus)
{
	const glm::vec3 towards {focus.x - from.x, focus.y - from.y, focus.z - from.z};
	const float xx = towards.x * towards.x;
	const float zz = towards.z * towards.z;
	const float lean = std::sqrt((towards.y * towards.y + zz) + xx) * k_FocusLean + towards.y;
	if (towards.x == 0.0f && lean == 0.0f && towards.z == 0.0f)
	{
		return {towards.x, lean, towards.z};
	}
	const float share = 1.0f / std::sqrt((lean * lean + zz) + xx);
	return {share * towards.x, share * lean, towards.z * share};
}

/// Showing that it turns the camera while it drags the land, the hand is drawn this share of its height higher
constexpr float k_RotateLiftShare = 0.33f;

} // namespace openblack::hand_navigation_pose
