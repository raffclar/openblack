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
#include <functional>
#include <span>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// EXPERIMENT (not for merge): the light of each place on the planet's ball. The island's light follows the height of
/// the sun over the island. A place far round the ball has the light of its own sun height: the day side is in the
/// light of day while the island has its night. A table holds the light at a set of sun heights, and the shaders read
/// it with the height of the sun at each point (world_curve.sh, PlaceLightAt). These are the CPU twins of those rules.
namespace openblack::graphics::place_light
{

/// The sun heights of the table, in degrees over the horizon: every 3 degrees from 21 under the horizon to 27 over it,
/// and then straight up. The light changes most near the horizon.
inline constexpr int k_Knots = 18;
inline constexpr float k_LowestDegrees = -21.0f;
inline constexpr float k_StepDegrees = 3.0f;
inline constexpr float k_TopDegrees = 90.0f;

/// The sun's height of a knot of the table, in degrees
[[nodiscard]] float KnotDegrees(int knot);

/// Where a sun height (the sine of its angle over the horizon) lies in the table: the first knot and the share of the
/// next one
struct KnotPlace
{
	int knot {0};
	float share {0.0f};
};
[[nodiscard]] KnotPlace PlaceOf(float sunHeight);

/// The light of a place for one height of the sun, with no moon
struct Knot
{
	/// The sun's light on the water: its colour and strength together
	glm::vec3 sunWater {0.0f};
	/// The faint light of the night sky, as a share of the moon's light, 0 to 1
	float glow {0.0f};
	/// The game's sky colour (its haze), 0 to 1
	glm::vec3 sky {0.0f};
	/// The colour that the air gives to the sun's light, in the colours of the screen
	glm::vec3 airTint {1.0f};
	/// How bright the air's light is on the screen (atmosphere::ScreenScale) at this sun height
	glm::vec3 airScale {1.0f};
};

/// What the table is made from
struct Inputs
{
	/// The sun's light on the water at its full strength
	glm::vec3 sunColour {1.0f};
	/// The sun's strength, 0 to 1, for the sine of its height (the overcast and the fog included)
	std::function<float(float sunHeight)> sunStrength;
	/// The night sky's faint light, 0 to 1, for the sine of the sun's height
	std::function<float(float sunHeight)> nightGlow;
	/// The game's sky colour (its haze), 0 to 1, for the sine of the sun's height
	std::function<glm::vec3(float sunHeight)> skyColour;
	/// The colour that the air gives to the sun's light, for the sine of the sun's height
	std::function<glm::vec3(float sunHeight)> airTint;
};

/// The light at one sun height, with the air's scale left at 1 (the caller finds it, as it costs more)
[[nodiscard]] Knot KnotAt(float sunHeight, const Inputs& inputs);

/// The table between two knots, for a sun height
[[nodiscard]] Knot Interpolate(std::span<const Knot, k_Knots> knots, float sunHeight);

/// The table as the shaders read it: four vectors a knot (the sun on the water and the glow, the sky, the air's tint,
/// the air's scale)
inline constexpr int k_VectorsPerKnot = 4;
[[nodiscard]] std::array<glm::vec4, k_Knots * k_VectorsPerKnot> Pack(std::span<const Knot, k_Knots> knots);

/// How much of the island's own light a point of the ball takes, from the up of the point in the frame that the
/// camera works in: 1 within k_IslandNear of the middle of that frame, 0 past k_IslandFar, and a smooth blend between.
/// The rest is the light of the table.
inline constexpr float k_IslandNear = 0.0314f; ///< radians: about 200 km
inline constexpr float k_IslandFar = 0.157f;   ///< radians: about 1000 km
[[nodiscard]] float IslandShare(float upY);

/// The strength of the moon's light at a place, 0 to 1: its full strength through the air at its height there, but not
/// less than the night sky's glow
[[nodiscard]] float MoonStrengthAt(float fullStrength, float moonHeight, float glow);

} // namespace openblack::graphics::place_light
