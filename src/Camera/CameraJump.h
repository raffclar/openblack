/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

/// A check for reports of the camera: it finds a move of the eye in one frame that is too long for a glide. The game
/// writes a log line for each such move, with the part of the frame that made it. The check changes nothing.
namespace openblack::camera_jump
{

/// A move of the eye in one frame that is longer than this, in metres, is a jump
inline constexpr float k_JumpDistance = 1000.0f;

/// Whether the eye jumped from `from` to `to` in one frame. An eye that is not a number also jumped.
[[nodiscard]] inline bool IsJump(const glm::vec3& from, const glm::vec3& to)
{
	const auto move = to - from;
	return !(glm::dot(move, move) <= k_JumpDistance * k_JumpDistance);
}

} // namespace openblack::camera_jump
