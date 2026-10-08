/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/vec3.hpp>

namespace openblack
{
class LandIslandInterface;
}

/// Where to find water to drink: the coast and the rivers, and the abode's cached drinking water.
///
/// Positions are MapCoords given in world units: x and z the world position (the original keeps them as 16.16 fixed
/// cells, world * 6553.6 truncated, and so do these functions inside), y the height above the ground (the MapCoords
/// altitude, usually 0), not the world altitude.
///
/// No consumer yet: the shepherd moving its flock to water, the abode's creation and the creature need villager jobs
/// and the creature. The tiger's lair placement calls FindNearestDrinkingWater(pos, 500) but never uses the answer (see
/// AnimalPredators.cpp).
namespace openblack::ecs::water_queries
{

/// The new abode looks for drinking water this far
constexpr float k_AbodeDrinkingWaterRadius = 200.0f;
/// The shepherd's radius when the abode has no drinking water yet
constexpr float k_ShepherdDrinkingWaterRadius = 400.0f;
/// The tiger's lair placement
constexpr float k_TigerLairDrinkingWaterRadius = 500.0f;

/// The distance in metres between two world points: gutils::GetDistanceInMetres with the points truncated to
/// MapCoords first
[[nodiscard]] float GetDistanceInMetres(glm::vec3 a, glm::vec3 b);

/// Walks the cells in a square spiral from `from` (map_coords::Spiral: -x, -z, +x +x, +z +z, -x x3, -z x3...; each
/// step moves the cell and keeps the fraction and y) until one is in the game map and coastal, or the walk gets
/// farther than `radius` from `from` (it stops at the first such cell, so the corners of the square are not all
/// seen), at most 999999 steps
[[nodiscard]] std::optional<glm::vec3> FindNearestCoastalTo(const LandIslandInterface& island, glm::vec3 from, float radius);
/// The river point (CREATE_STREAM_POINT) nearest to `from` in xz, strictly
/// closer than `radius`; y = the point's altitude minus the ground's there
[[nodiscard]] std::optional<glm::vec3> FindNearestStreamPosTo(const LandIslandInterface& island, glm::vec3 from, float radius);
/// The nearest river point within `radius`, then (true either way) the
/// nearest coast that is not farther from `from` than that river point, if any; with no river, the coast within
/// `radius`. `out` changes only where something is found, like the original's out MapCoords.
bool FindNearestDrinkingWater(const LandIslandInterface& island, glm::vec3 from, glm::vec3& out, float radius);

/// The abode's drinking water: whether it was found and where
struct DrinkingWater
{
	bool found {false};
	glm::vec3 position {0.0f};
};
/// FindNearestDrinkingWater from the abode into its cache; the flag is the
/// answer (the position stays the old one when nothing is found)
bool FindNearestDrinkingWater(const LandIslandInterface& island, DrinkingWater& water, glm::vec3 abodePosition, float radius);
/// The cached drinking water, if found
[[nodiscard]] std::optional<glm::vec3> GetNearestWaterPos(const DrinkingWater& water);

// With the Locator's island (no island: nothing is found)
[[nodiscard]] std::optional<glm::vec3> FindNearestCoastalTo(glm::vec3 from, float radius);
[[nodiscard]] std::optional<glm::vec3> FindNearestStreamPosTo(glm::vec3 from, float radius);
bool FindNearestDrinkingWater(glm::vec3 from, glm::vec3& out, float radius);

} // namespace openblack::ecs::water_queries
