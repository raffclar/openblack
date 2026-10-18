/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// EXPERIMENT (not for merge): the thunderstorm clouds (cumulonimbus) of the volumetric clouds. Each storm holds one to
/// three cells. A cell is a tall tower from a dark flat bottom up to the tropopause, with an anvil that spreads out
/// downwind at the top. The cells grow, live and break up in turn, so that a storm always has a mature cell. The shaders
/// shape each cell from these numbers (clouds.sh), and the lightning strikes come down from the cells.
namespace openblack::graphics::cumulonimbus
{

/// The world units for one metre (as the sky's own scale)
inline constexpr float k_UnitsPerMetre = 0.949f;
/// The flat bottom of a cell over the sea: about 1 km
inline constexpr float k_Base = 1000.0f;
/// The top of a mature cell over the sea: the tropopause, 11 km
inline constexpr float k_Tropopause = 11000.0f * k_UnitsPerMetre;
/// How far the dome over the strongest updraft goes up through the top of the anvil
inline constexpr float k_DomeHeight = 900.0f;
/// The radius of a cell's tower at its bottom
inline constexpr float k_TowerRadius = 2500.0f;
/// How far the top of a tower leans downwind in the wind's shear, in radii of the tower
inline constexpr float k_Lean = 0.8f;
/// The anvil, in radii of the tower: how far it reaches upwind of the tower's top, how far downwind, and how wide it
/// is at its downwind end
inline constexpr float k_AnvilUpwind = 2.0f;
inline constexpr float k_AnvilDownwind = 5.5f;
inline constexpr float k_AnvilWidth = 3.0f;
/// The depth of the anvil under the top, at its upwind end
inline constexpr float k_AnvilDepth = 1800.0f;
/// The most cells at a time
inline constexpr size_t k_MaxCells = 4;
/// A storm weaker than this has no cells: they would not show, and they cost the clouds' rays time
inline constexpr float k_FaintestStorm = 0.02f;
/// A storm builds its cells at full strength from a rain of 1 / k_RainForCells; with less rain they are weaker
inline constexpr float k_RainForCells = 2.0f;
/// The most cells in one storm
inline constexpr uint32_t k_MostCellsInStorm = 3;
/// A storm holds one more cell for each of these units of its outer radius
inline constexpr float k_RadiusForEachCell = 3000.0f;
/// The cells stand at most this share of the storm's outer radius from its middle
inline constexpr float k_CellReach = 0.45f;
/// The seconds from the start of a cell to its end. The next cell starts at a new place.
inline constexpr float k_LifeSeconds = 300.0f;
/// The parts of a cell's life: it grows up to the tropopause over the first part, and it breaks up over the last part
inline constexpr float k_GrowthShare = 0.25f;
inline constexpr float k_DecayShare = 0.2f;
/// A strike comes from a cell that is at least this tall (as a share of its full height) and that has not broken up
inline constexpr float k_MatureHeight = 0.6f;
/// A strike lands at most this share of the tower's radius from the middle of the cell, under its core
inline constexpr float k_CoreShare = 0.4f;
/// The distance at which a cell has half the chance to make a strike that a cell at the wanted place has
inline constexpr float k_StrikeReach = 8000.0f;

/// A storm as the cells see it
struct StormSite
{
	/// The storm's own number, which stays the same while it lives
	uint32_t serial {0};
	glm::vec2 centre {0.0f};
	float outerRadius {0.0f};
	/// How strong it is now, 0 to 1
	float strength {0.0f};
	/// Its rain, 0 to 1
	float rain {0.0f};
};

/// A cell of a storm
struct Cell
{
	/// The number of the storm that holds the cell
	uint32_t serial {0};
	/// The middle of the tower at its bottom, across the ground
	glm::vec2 centre {0.0f};
	float radius {k_TowerRadius};
	/// How tall the tower is, as a share of the height from k_Base to k_Tropopause, 0 to 1
	float height {0.0f};
	/// How much of the cloud is still there, 1 until it breaks up, then down to 0
	float fade {0.0f};
	/// The rain under its core, 0 to 1
	float rain {0.0f};
};

/// The cells now
struct Cells
{
	std::array<Cell, k_MaxCells> cells {};
	size_t count {0};

	[[nodiscard]] std::span<const Cell> All() const { return {cells.data(), count}; }
};

/// A number from 0 to 1 for a seed and an index, always the same for the same two
[[nodiscard]] float RandomOf(uint32_t seed, uint32_t index);
/// The number of cells in a storm of an outer radius
[[nodiscard]] uint32_t CellCount(float outerRadius);
/// The height of a cell at a point of its life (0 to 1): it grows over k_GrowthShare and stays at the top after that
[[nodiscard]] float HeightAt(float life);
/// How much of a cell is still there at a point of its life: 1, then down to 0 over the last k_DecayShare
[[nodiscard]] float FadeAt(float life);
/// The cells of the storms at a time in seconds. Only storms with rain have cells. The strongest storms come first, and
/// there are at most k_MaxCells.
[[nodiscard]] Cells CellsFor(std::span<const StormSite> storms, float seconds);
/// Where a strike of a storm comes down: under the core of one of the storm's mature cells. The cells nearer the wanted
/// place have more chance. `randoms` are three numbers from 0 to 1: the choice of the cell, and the place under its
/// core. With no mature cell, the tallest cell of the storm. None when the storm has no cell.
[[nodiscard]] std::optional<glm::vec2> StrikePlace(std::span<const Cell> cells, uint32_t serial, glm::vec2 wanted,
                                                   const std::array<float, 3>& randoms);
/// How much of the anvil a cell has, 0 while its tower is low to 1 at its full height
[[nodiscard]] float AnvilGrowth(float height);
/// The upright cylinder that holds a cell's tower, its dome and its rain: xy the middle across the ground, z the radius,
/// w the top over the sea. It goes down to the sea. `downwind` is a direction of length 1.
[[nodiscard]] glm::vec4 TowerBound(const Cell& cell, glm::vec2 downwind);
/// The flat cylinder that holds a cell's anvil: xy the middle across the ground, z the radius (0 with no anvil), w the
/// bottom over the sea. Its top is the top of the tower's cylinder.
[[nodiscard]] glm::vec4 AnvilBound(const Cell& cell, glm::vec2 downwind);
/// The shaders' array of the cells: [i] xy the middle, z the radius, w the height; [k_MaxCells + i] xy the direction
/// downwind across the ground, z the fade, w the rain; [2 k_MaxCells + i] the tower's cylinder (TowerBound);
/// [3 k_MaxCells + i] the anvil's cylinder (AnvilBound). The cells that are not there are all 0.
[[nodiscard]] std::array<glm::vec4, k_MaxCells * 4> Pack(std::span<const Cell> cells, glm::vec2 downwind);

} // namespace openblack::graphics::cumulonimbus
