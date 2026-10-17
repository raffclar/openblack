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

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

/// EXPERIMENT (not for merge): the rules of the volumetric fog. The original game has a haze by distance only. The fog
/// fills a grid of cells that follows the view (160 across, 96 down, 48 deep), and the light of the sun or the moon,
/// with the shadows of the hills, the trees and the clouds, scatters in it: shafts of light come through the trees and
/// the gaps in the clouds. It has these layers:
/// - a thin haze over all the land that thins with height;
/// - a ground fog in the valleys and over the water at dawn, which thins and lifts as the sun rises and the day warms;
/// - a mist that the wind moves, from a noise volume.
/// There is more ground fog after rain and none in a storm, which has a murk of its own in the haze.
namespace openblack::graphics::volumetric_fog
{

/// The cells of the grid across the view, down the view and into the view
inline constexpr uint16_t k_CellsAcross = 160;
inline constexpr uint16_t k_CellsDown = 96;
inline constexpr uint16_t k_Slices = 48;
/// The slices lie in a grid of tiles in one texture: this many tiles across, and as many rows as the slices need
inline constexpr uint16_t k_TilesAcross = 8;
inline constexpr uint16_t k_TileRows = k_Slices / k_TilesAcross;
/// The nearest and the farthest distance that the grid covers, in world units
inline constexpr float k_Nearest = 3.0f;
inline constexpr float k_Farthest = 3200.0f;

/// The distance of the middle of a slice from the eye (the slices are farther apart far away)
[[nodiscard]] float SliceDistance(float slice);
/// The slice at a distance from the eye, as a fraction: SliceDistance's inverse
[[nodiscard]] float SliceAt(float distance);

/// How much ground fog the time of day gives, 0 to 1: the most from before sunrise to an hour after it, then less and
/// less until mid morning; a little at night and none in the afternoon. `hour` is 0 to 24.
[[nodiscard]] float DawnFog(float hour);
/// How high the top of the ground fog is over the sea, in world units: low at dawn, then higher as the fog lifts
[[nodiscard]] float FogTop(float hour);

/// How wet the land is after rain, 0 to 1: it gets wet soon in rain and dries slowly over some minutes
[[nodiscard]] float StepWetness(float wetness, float rain, float seconds);

/// The fog of a moment
struct FogState
{
	/// The haze over all the land at the sea's level, for each world unit, and the height over which it halves
	float hazeDensity {0.0f};
	float hazeHeight {0.0f};
	/// The ground fog's density for each world unit, and the height of its top over the sea
	float groundDensity {0.0f};
	float groundTop {0.0f};
};

/// The fog for an hour, how wet the land is and how stormy the weather is (0 to 1)
[[nodiscard]] FogState FogFor(float hour, float wetness, float storm);

/// The uniforms of the fog for a frame
struct FogUniforms
{
	/// x the haze's density, y the haze's height, z the ground fog's density, w the ground fog's top
	glm::vec4 layers {0.0f};
	/// xy how far the wind has moved the mist, z the seconds for the mist's slow change, w the frame's number
	glm::vec4 drift {0.0f};
	/// xyz the direction towards the sun or the moon, w how much of the light is the moon's
	glm::vec4 light {0.0f, 1.0f, 0.0f, 0.0f};
	/// rgb the light's colour and strength, w 1 while there is light
	glm::vec4 lightColour {0.0f};
	/// rgb the light of the sky in the fog, w the sea's level
	glm::vec4 ambient {0.0f};
	/// x the cells across, y the cells down, z the slices, w the farthest distance
	glm::vec4 grid {k_CellsAcross, k_CellsDown, k_Slices, k_Farthest};
};

} // namespace openblack::graphics::volumetric_fog
