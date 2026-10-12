/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "CameraZones.h"

/// Where the player's world camera may go, apart from a land's fence: the disc round the island it never leaves, how
/// far it may move in a frame, the clearance it keeps from the land and from the zones' exclusions. Pure rules, tested
/// without the game.
namespace openblack::camera_limits
{

/// The camera never goes further across the ground than this from the middle of the map
inline constexpr glm::vec2 k_AllowedAreaCentre {2560.0f, 2560.0f};
inline constexpr float k_AllowedAreaRadius = 5120.0f;

/// The camera keeps this far from the land, the exclusions and the ground beneath it
inline constexpr float k_Clearance = 3.0f;
/// The closest the camera zooms to what it looks at
inline constexpr float k_ClosestZoom = k_Clearance + 0.1f;

/// Where the camera goes when it has strayed outside the disc round the island: brought straight back across the ground
/// to its rim, at the same height. Only the camera is moved, not what it looks at. Nothing when it is inside.
[[nodiscard]] std::optional<glm::vec3> KeepInsideAllowedArea(const glm::vec3& origin);

/// However a camera is moved, where it is going is kept within a ball round the middle of the map at the sea's level,
/// 3500 from it in every direction: going further, it is sent back to 3499 from the middle along the same line, easing
/// there over three seconds and arriving still. So the camera rises no higher than 3500 over the middle of the map, less
/// further out, and never reaches the map's corners. Nothing when it is inside.
inline constexpr glm::vec3 k_WorldCentre {2560.0f, 0.0f, 2560.0f};
inline constexpr float k_WorldReach = 3500.0f;
inline constexpr float k_WorldHoldRadius = 3499.0f;
inline constexpr float k_WorldHoldSeconds = 3.0f;
[[nodiscard]] std::optional<glm::vec3> KeepInsideTheWorld(const glm::vec3& destination);

/// How long the player's camera takes to ease to where it is going: 0.3 seconds, twice that when the limits moved what it
/// looks at, and twice again when they moved the camera; for the first 1.5 seconds after a flight began (or the camera
/// was made), it eases from 1.5 seconds down to that
inline constexpr float k_EaseSeconds = 0.3f;
inline constexpr float k_FlightEaseSeconds = 1.5f;
inline constexpr float k_EaseSettleSeconds = 1.5f;
[[nodiscard]] float EaseSeconds(float sinceFlight, bool pushed, bool focusMoved);

/// The furthest the camera may go in a frame: 1.6 times the distance to what it looks at each second, but at least
/// 300
inline constexpr float k_MinFrameMove = 300.0f;
inline constexpr float k_FrameMovePerFocusDistance = 1.6f;
[[nodiscard]] float FrameMoveLimit(float focusDistance, float seconds);
/// Where the camera goes when it would move further than `limit` from where it was at the start of the frame: that far
/// along the way it was going. Nothing when it moves no further than that.
[[nodiscard]] std::optional<glm::vec3> LimitFrameMove(const glm::vec3& start, const glm::vec3& origin, float limit);

/// The exclusions near enough to matter this frame, from where the camera was at its start: within twice the
/// clearance of their edge across the ground, and for domes, with the camera no higher above the dome's base than its
/// height and twice the clearance
[[nodiscard]] std::vector<camera_zones::Exclusion> NearbyExclusions(std::span<const camera_zones::Exclusion> exclusions,
                                                                    const glm::vec3& originAtStart);

/// Where a camera inside a cylinder (or within the clearance of its side) is pushed: straight out across the ground to
/// the clearance, at the same height. Nothing when it is in none.
[[nodiscard]] std::optional<glm::vec3> PushOutOfCylinders(std::span<const camera_zones::Exclusion> nearby,
                                                          const glm::vec3& origin);

/// The land under the camera
struct Ground
{
	std::function<float(glm::vec2)> height;
	std::function<glm::vec3(glm::vec2)> normal;
};

/// The height of what the camera stands off at a point: the land, or a dome's curved roof where it is higher.
/// `withDomes` false gives the land alone.
[[nodiscard]] float SurfaceHeight(const Ground& ground, std::span<const camera_zones::Exclusion> nearby, bool withDomes,
                                  glm::vec2 point);

/// The camera after it is pushed off the land and the domes round it
struct GroundPush
{
	glm::vec3 origin;
	glm::vec3 focus;
	/// Whether it was pushed at all
	bool pushed {false};
	/// Whether what it looks at was moved too
	bool focusMoved {false};
};

/// The camera feels for the land with sixteen points on a ball of the clearance's radius round it, turned to face the
/// way it looks (one below it, one above, rings of four between and six round its middle). Each point below the land or
/// a dome's roof pushes the camera out along the surface's normal by how deep it is, at most half the clearance; a
/// push with any part backwards, away from what it looks at, moves what it looks at too, by the push times that part.
/// Points are felt one after the other, each from where the pushes before have left the camera. The domes count as
/// ground for the test only while the zones keep the camera out of their exclusions.
[[nodiscard]] GroundPush PushOffTheGround(const Ground& ground, std::span<const camera_zones::Exclusion> nearby,
                                          bool exclusionsOn, const glm::vec3& origin, const glm::vec3& focus);

} // namespace openblack::camera_limits
