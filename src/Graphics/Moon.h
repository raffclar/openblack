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

#include <array>
#include <optional>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// Black & White's moon: a half sphere that keeps a place beside the camera, turned to face it, its face lit by the
/// real moon's phase, and a glow about it.
namespace openblack::graphics::moon
{

/// Where the moon stands from the camera at an hour of script time, and how strongly it shows, 0 to 200: it swings on
/// an ellipse through the day, showing only for a few hours either side of midnight. None while it is down.
struct Placement
{
	glm::vec3 offset;
	float alpha;
};
[[nodiscard]] std::optional<Placement> Place(float scriptHour);
/// Where the moon stands from the camera at an hour of script time, whether or not it shows
[[nodiscard]] glm::vec3 Offset(float scriptHour);

/// The day the moon months are counted from, in whole days since 1970: 6 January 2000, a new moon
inline constexpr int64_t k_NewMoonDay = 10962;
/// Moon months a day, one every 29.53 days
inline constexpr double k_MonthsPerDay = 0.03386318012808897;
inline constexpr int64_t k_SecondsPerDay = 86400;

/// How far through its moon month the real moon is, by whole days of a seconds since 1970 time: 0 at a new moon and
/// about 0.5 at a full moon. Before 2000 it counts back from 0, so it is negative.
[[nodiscard]] double MonthFraction(int64_t unixTime);

/// The phase of the real moon, 0 to 2 pi, by whole days of a seconds since 1970 time: a whole turn at a new moon and
/// half a turn at a full moon
[[nodiscard]] float Phase(int64_t unixTime);

/// What scripts are told of the moon from its phase: 0 half way through the moon month, at the full moon, rising to 1
/// at a new moon either side
[[nodiscard]] float ScriptPercentage(float phase);

/// Noon of the day within half a moon month of a seconds since 1970 time that is nearest to a point of the moon month,
/// 0 the new moon and 0.5 the full moon: for jumping the date the phase is taken from
[[nodiscard]] int64_t DateAtFraction(int64_t unixTime, double fraction);

/// Where the moon stands in the sky from the camera, in degrees: its compass bearing, 0 north and 90 east, and its
/// height above the horizon
struct SkyAngles
{
	float azimuth;
	float elevation;
};
[[nodiscard]] SkyAngles Angles(const glm::vec3& offset);

/// The moon's axes in the world: square to the line from the camera, four times the mesh's size
[[nodiscard]] glm::mat3 Basis(const glm::mat4& view, const glm::mat4& inverseView, const glm::vec3& position);

/// The moon mesh's model: the basis tilted a little and turned by the phase, at two thirds the size
[[nodiscard]] glm::mat4 Model(const glm::mat3& basis, const glm::vec3& position, float phase);

/// The glow about the moon: a square of 4000 units on the basis, from part of the atmosphere texture
struct Glow
{
	std::array<glm::vec3, 4> corners;
	std::array<glm::vec2, 4> uvs;
};
[[nodiscard]] Glow MakeGlow(const glm::mat3& basis, const glm::vec3& position);
/// The glow of the moon's copy in the sea, for drawing in the view mirrored through sea level. The copy stands where
/// the moon is with its height mirrored through sea level, and its glow faces the camera from there; its corners are
/// mirrored back, so that the mirrored view shows it so. The copy's glow is all that is drawn of it: the moon in the sea
/// is the moon itself, mirrored.
[[nodiscard]] Glow SeaGlow(const glm::mat4& view, const glm::mat4& inverseView, const glm::vec3& position);
/// The glow's two triangles
inline constexpr std::array<uint16_t, 6> k_GlowIndices = {0, 1, 3, 3, 2, 0};

/// The glow's colour, a dim copy of the moon's: red a sixth, green a fifth and blue a quarter of it
[[nodiscard]] glm::vec3 GlowColour(const glm::vec3& moonColour);

} // namespace openblack::graphics::moon
