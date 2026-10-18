/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace openblack::hand_orientation
{

/// The hand's up eases to the land's slope over this many seconds
constexpr float k_UpEaseSeconds = 0.4f;
/// The line of sight must lean at least this far from straight down, east-west or north-south, to turn the hand
constexpr float k_MinHeadingLean = 0.01f;

/// Whether the line of sight leans far enough across the land to turn the hand
[[nodiscard]] bool LeansAcrossLand(glm::vec3 rayDirection);

/// Which way the hand faces across the land: along the line of sight through the cursor, flattened. Looking straight
/// down it keeps the way it faced.
[[nodiscard]] glm::vec3 HeadingAlongRay(glm::vec3 rayDirection, glm::vec3 previousHeading);

/// The hand's heading as an angle about the up axis, from the line of sight through the cursor: 0 looking along -z,
/// growing towards +x. Looking straight down, or with no lean across the land, it is 0.
[[nodiscard]] float HeadingAngle(glm::vec3 rayDirection);

/// The rows of the hand's frame while it holds a town's totem, in the model's own axes (row-vector form: the model's x,
/// y and z in the world). The hand turns half a turn from its heading angle, then tips about its x by the tilt. Each
/// step rounds to a float as the game does.
[[nodiscard]] std::array<glm::vec3, 3> TippedRows(float headingAngle, float tilt);

/// The hand's rotation from those rows, for the hand's model as openblack loads it. Its axes are a quarter turn about x
/// from the model's own axes, so the rows only change places and sign.
[[nodiscard]] glm::mat3 TippedHand(const std::array<glm::vec3, 3>& rows);

/// The hand turned to face along a heading on level land, from how it is turned facing along the camera's own
/// heading. Both headings are level.
[[nodiscard]] glm::mat3 TurnToHeading(const glm::mat3& facingCamera, glm::vec3 cameraHeading, glm::vec3 heading);

/// The hand stood up on a slope: its up becomes the slope's up and its front the heading laid along the slope, from
/// how it stands facing along the heading on level land
[[nodiscard]] glm::mat3 StandOnSlope(const glm::mat3& onLevelLand, glm::vec3 heading, glm::vec3 up);

/// The hand tipped forwards by an angle about the level line across its heading, so that its fingers point down
/// towards what is ahead of it, from how it stands facing along the heading on level land
[[nodiscard]] glm::mat3 TipForwards(const glm::mat3& onLevelLand, glm::vec3 heading, float angle);

} // namespace openblack::hand_orientation
