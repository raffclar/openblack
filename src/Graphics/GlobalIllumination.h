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
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Atmosphere.h"

/// EXPERIMENT (not for merge): the rules of the light that the land and the sky give to the shade. The original game
/// bakes its light into the land. The experiment adds:
/// - the sky's light: the sky of the air's model (see atmosphere), less where the hills, the things round a point and
///   its own folds hide the sky;
/// - one bounce of the sun's light: a grid of light probes over the island, 64 by 64. Each probe sends rays against the
///   land's heights only, and keeps the light that the lit ground and the water send back as four spherical harmonics
///   (L1) for each colour. The grid is made again a band of rows a frame after each new set of shadows, about every
///   3 seconds, and the new grid blends in over the old one.
/// The shade then gets the light of grass, sand and cliffs on its sides, and is never black.
namespace openblack::graphics::global_illumination
{

/// The probes along each side of the grid, and the world units between two probes (the map is 5130 units across)
inline constexpr uint16_t k_ProbesPerSide = 64;
inline constexpr float k_MapUnits = 5130.0f;
inline constexpr float k_ProbeSpacing = k_MapUnits / static_cast<float>(k_ProbesPerSide);
/// How high over the ground (or the sea) each probe is, in world units
inline constexpr float k_ProbeLift = 3.0f;
/// The rays that each probe sends
inline constexpr uint16_t k_RaysPerProbe = 64;
/// The rows of the grid that one frame makes; the whole grid takes k_ProbesPerSide / k_RowsPerFrame frames
inline constexpr uint16_t k_RowsPerFrame = 8;
/// The seconds over which a new grid blends in
inline constexpr float k_BlendSeconds = 1.5f;
/// The share of the light's colour that the shade keeps: the rest is grey, so the shade is not too blue or too warm
inline constexpr float k_ColourShare = 0.45f;
/// The darkest and the brightest that the light of the shade may make a pixel, as a part of the open sky's light
inline constexpr float k_DarkestShade = 0.35f;
inline constexpr float k_BrightestShade = 2.0f;

/// Four spherical harmonics (L1) for each colour: the mean, then the parts along y, z and x
struct ShL1
{
	std::array<glm::vec3, 4> c {};
};

/// The four harmonics' values in a direction
[[nodiscard]] std::array<float, 4> ShBasis(glm::vec3 direction);
/// Adds the light from a direction, for a solid angle, to the harmonics
void ShAdd(ShL1& sh, glm::vec3 direction, glm::vec3 radiance, float solidAngle);
/// The light that falls on a surface that faces `normal`, from the light that the harmonics hold (never below 0)
[[nodiscard]] glm::vec3 ShIrradiance(const ShL1& sh, glm::vec3 normal);

/// Directions spread evenly over the sphere (the spherical Fibonacci spiral)
[[nodiscard]] std::vector<glm::vec3> RayDirections(uint16_t count);

/// The sky's light as harmonics, from `samples` directions over the sphere: the air's sky above the horizon and
/// nothing below it
[[nodiscard]] ShL1 SkySh(glm::vec3 towardsLight, float altitude, const atmosphere::Air& air, uint16_t samples);

/// The middle of a probe across the land, in world units
[[nodiscard]] glm::vec2 ProbeCentre(uint16_t column, uint16_t row);

/// The making of the grid of probes again, a band of rows a frame. Start begins it; each Next gives the next band
/// until the grid is done.
class ProbeSweep
{
public:
	struct Band
	{
		uint16_t firstRow;
		uint16_t rows;
	};

	/// Begins the grid again from its first row
	void Start();
	/// The band of rows to make this frame, or none when the grid is done or not begun
	[[nodiscard]] std::optional<Band> Next();
	/// Whether the grid is being made
	[[nodiscard]] bool Running() const { return _nextRow.has_value(); }

private:
	std::optional<uint16_t> _nextRow;
};

/// How much of the newest grid shows, 0 to 1, seconds after it is done
[[nodiscard]] float ProbeBlend(float seconds);

/// What the shade's light makes of a pixel's colour, for the light that reaches it and the open sky's light on flat
/// ground: their ratio, with some of its colour taken out, held between the darkest and the brightest shade
[[nodiscard]] glm::vec3 ShadeTint(glm::vec3 light, glm::vec3 openSky);

} // namespace openblack::graphics::global_illumination
