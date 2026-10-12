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

#include <functional>
#include <numbers>
#include <optional>

#include <glm/vec3.hpp>

#include "CameraLimits.h"
#include "CameraZones.h"

/// The player's camera flying to a place double clicked, or to land gripped too far ahead to drag: where it ends, how
/// long it takes and whether it goes the long way round, high over the land. Pure rules, tested without the game.
namespace openblack::camera_fly
{

/// Why the camera flies
enum class Reason : uint8_t
{
	/// The player double clicked a place or a thing
	DoubleClick,
	/// The player gripped land too far ahead to drag
	GripTooFar,
};

/// The camera flies to end this far from the place: a long flight (more than 1.5 times the first, the place more than
/// 10 from what the camera looks at) ends 100 away; a medium one (more than 1.5 times 60) 60 away; shorter ones 30, or 15
/// when the place is within 1.5 times 30 of the camera
inline constexpr float k_FarDistance = 100.0f;
inline constexpr float k_MediumDistance = 60.0f;
inline constexpr float k_NearDistance = 30.0f;
inline constexpr float k_CloseDistance = 15.0f;
inline constexpr float k_DistanceFactor = 1.5f;
/// A long flight goes only to a place this far from what the camera looks at
inline constexpr float k_FarFocusMinimum = 10.0f;
/// Land gripped too far ahead is flown to from this far, looking as the camera already does
inline constexpr float k_GripDistance = 1000.0f;
/// The camera ends looking down at least this steeply and at most this steeply
inline constexpr float k_MinPitch = std::numbers::pi_v<float> / 8.0f;
inline constexpr float k_MaxPitch = std::numbers::pi_v<float> / 2.1f;
/// How far into its easing a flight starts: long flights from the start, shorter ones half way through the easing's
/// first 1.5 seconds, a flight to gripped land almost at once
inline constexpr float k_LongStart = 0.0f;
inline constexpr float k_ShortStart = camera_limits::k_EaseSettleSeconds * 0.5f;
inline constexpr float k_GripStart = 0.01f;
/// A long flight rises over its half way point by a tenth of how far it goes across, and at least this far over the land
inline constexpr float k_HalfWayRise = 0.1f;
inline constexpr float k_HalfWayClearance = 10.0f;

/// The heading (round the vertical) and pitch (down from level) of a camera at `origin` looking at `focus`; one almost
/// straight above looks with no heading, nearly straight down
struct Bearing
{
	float heading {0.0f};
	float pitch {0.0f};
};
[[nodiscard]] Bearing BearingOf(const glm::vec3& origin, const glm::vec3& focus);
/// Where a camera with a bearing is, a distance from what it looks at
[[nodiscard]] glm::vec3 OriginAt(const glm::vec3& focus, float distance, const Bearing& bearing);

/// The heading that looks at a place over the lowest land: 32 headings round from the camera's are scored by how far the
/// land lies below the place at five points between 3/8 and 7/8 of the way out, plus 50 times the cosine of the turn
/// from the camera's heading; the first best wins. The pitch is eased towards a fifth of the old one, a tenth of the land's
/// slope there and 0.377, between an eighth and a third of a half turn.
[[nodiscard]] Bearing BestBearing(const camera_limits::Ground& ground, const Bearing& current, float distance,
                                  const glm::vec3& place);

/// The flight
struct Flight
{
	glm::vec3 origin;
	glm::vec3 focus;
	/// How far into the easing it starts, in seconds
	float startSeconds {0.0f};
	/// Whether it goes by a point high over the half way point (long flights)
	bool halfWay {false};
	/// Whether it wooshes
	bool woosh {false};
};

/// Where a flight goes. `place` is on the land; `screenCentre` is the land the middle of the screen last showed.
[[nodiscard]] Flight Plan(Reason reason, const camera_limits::Ground& ground, const glm::vec3& place, const glm::vec3& origin,
                          const glm::vec3& focus, const glm::vec3& screenCentre);

/// The place a double click flies to, from the thing clicked (its spot on the land) or else the land or sea clicked: one
/// outside the fence is moved to the fence along the ground's first axis; one beyond the disc round the island isn't flown
/// to. Nothing without a place.
[[nodiscard]] std::optional<glm::vec3> Destination(std::optional<glm::vec3> place, const camera_zones::Zones& zones);

/// The high point a long flight goes by, half way between where the camera is and where it ends
[[nodiscard]] glm::vec3 HalfWayPoint(const glm::vec3& end, const glm::vec3& current, float rise,
                                     const std::function<float(glm::vec2)>& groundHeight);

} // namespace openblack::camera_fly
